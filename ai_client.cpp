#include "ai_client.h"

#include <cstddef>
#include <locale>
#include <mutex>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include <curl/curl.h>

namespace telebit::ai {

namespace {

// ---------------------------------------------------------------------------
// JSON: request building (escape) + a tiny recursive-descent parser used only
// to pull the generated text out of the response. Dependency-free on purpose.
// ---------------------------------------------------------------------------

std::string jsonEscape(const std::string &s) {
    std::string out;
    out.reserve(s.size() + 16);
    for (unsigned char c : s) {
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            case '\b': out += "\\b"; break;
            case '\f': out += "\\f"; break;
            default:
                if (c < 0x20) {
                    static const char *hex = "0123456789abcdef";
                    out += "\\u00";
                    out += hex[(c >> 4) & 0xf];
                    out += hex[c & 0xf];
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    return out;
}

// A minimal JSON value. Objects/arrays keep insertion order in vectors so we
// avoid pulling in a map; lookups are linear which is fine for API responses.
struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object } type = Type::Null;
    bool boolValue = false;
    double numberValue = 0.0;
    std::string stringValue;
    std::vector<JsonValue> arrayValue;
    std::vector<std::pair<std::string, JsonValue>> objectValue;

    const JsonValue *find(const std::string &key) const {
        if (type != Type::Object) return nullptr;
        for (const auto &kv : objectValue) {
            if (kv.first == key) return &kv.second;
        }
        return nullptr;
    }
};

class JsonParser {
public:
    explicit JsonParser(const std::string &text) : s_(text) {}

    bool parse(JsonValue &out) {
        skipWs();
        if (!parseValue(out)) return false;
        skipWs();
        return true;  // trailing content tolerated
    }

private:
    const std::string &s_;
    std::size_t i_ = 0;

    void skipWs() {
        while (i_ < s_.size()) {
            char c = s_[i_];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
                ++i_;
            } else {
                break;
            }
        }
    }

    bool parseValue(JsonValue &out) {
        skipWs();
        if (i_ >= s_.size()) return false;
        char c = s_[i_];
        switch (c) {
            case '{': return parseObject(out);
            case '[': return parseArray(out);
            case '"': {
                out.type = JsonValue::Type::String;
                return parseString(out.stringValue);
            }
            case 't':
            case 'f': return parseBool(out);
            case 'n': return parseNull(out);
            default: return parseNumber(out);
        }
    }

    bool parseObject(JsonValue &out) {
        out.type = JsonValue::Type::Object;
        ++i_;  // consume '{'
        skipWs();
        if (i_ < s_.size() && s_[i_] == '}') {
            ++i_;
            return true;
        }
        while (i_ < s_.size()) {
            skipWs();
            if (i_ >= s_.size() || s_[i_] != '"') return false;
            std::string key;
            if (!parseString(key)) return false;
            skipWs();
            if (i_ >= s_.size() || s_[i_] != ':') return false;
            ++i_;  // consume ':'
            JsonValue value;
            if (!parseValue(value)) return false;
            out.objectValue.emplace_back(std::move(key), std::move(value));
            skipWs();
            if (i_ >= s_.size()) return false;
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == '}') {
                ++i_;
                return true;
            }
            return false;
        }
        return false;
    }

    bool parseArray(JsonValue &out) {
        out.type = JsonValue::Type::Array;
        ++i_;  // consume '['
        skipWs();
        if (i_ < s_.size() && s_[i_] == ']') {
            ++i_;
            return true;
        }
        while (i_ < s_.size()) {
            JsonValue value;
            if (!parseValue(value)) return false;
            out.arrayValue.push_back(std::move(value));
            skipWs();
            if (i_ >= s_.size()) return false;
            if (s_[i_] == ',') {
                ++i_;
                continue;
            }
            if (s_[i_] == ']') {
                ++i_;
                return true;
            }
            return false;
        }
        return false;
    }

    bool parseString(std::string &out) {
        if (i_ >= s_.size() || s_[i_] != '"') return false;
        ++i_;  // consume opening quote
        while (i_ < s_.size()) {
            char c = s_[i_++];
            if (c == '"') return true;
            if (c == '\\') {
                if (i_ >= s_.size()) return false;
                char e = s_[i_++];
                switch (e) {
                    case '"': out += '"'; break;
                    case '\\': out += '\\'; break;
                    case '/': out += '/'; break;
                    case 'n': out += '\n'; break;
                    case 'r': out += '\r'; break;
                    case 't': out += '\t'; break;
                    case 'b': out += '\b'; break;
                    case 'f': out += '\f'; break;
                    case 'u': {
                        if (i_ + 4 > s_.size()) return false;
                        unsigned int cp = 0;
                        for (int k = 0; k < 4; ++k) {
                            char h = s_[i_++];
                            cp <<= 4;
                            if (h >= '0' && h <= '9') cp |= static_cast<unsigned>(h - '0');
                            else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
                            else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
                            else return false;
                        }
                        // Handle UTF-16 surrogate pairs.
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            if (i_ + 6 <= s_.size() && s_[i_] == '\\' && s_[i_ + 1] == 'u') {
                                i_ += 2;
                                unsigned int lo = 0;
                                for (int k = 0; k < 4; ++k) {
                                    char h = s_[i_++];
                                    lo <<= 4;
                                    if (h >= '0' && h <= '9') lo |= static_cast<unsigned>(h - '0');
                                    else if (h >= 'a' && h <= 'f') lo |= static_cast<unsigned>(h - 'a' + 10);
                                    else if (h >= 'A' && h <= 'F') lo |= static_cast<unsigned>(h - 'A' + 10);
                                    else return false;
                                }
                                cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            }
                        }
                        appendUtf8(out, cp);
                        break;
                    }
                    default: return false;
                }
            } else {
                out += c;
            }
        }
        return false;  // unterminated
    }

    static void appendUtf8(std::string &out, unsigned int cp) {
        if (cp <= 0x7F) {
            out += static_cast<char>(cp);
        } else if (cp <= 0x7FF) {
            out += static_cast<char>(0xC0 | (cp >> 6));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else if (cp <= 0xFFFF) {
            out += static_cast<char>(0xE0 | (cp >> 12));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        } else {
            out += static_cast<char>(0xF0 | (cp >> 18));
            out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
            out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
            out += static_cast<char>(0x80 | (cp & 0x3F));
        }
    }

    bool parseBool(JsonValue &out) {
        if (s_.compare(i_, 4, "true") == 0) {
            out.type = JsonValue::Type::Bool;
            out.boolValue = true;
            i_ += 4;
            return true;
        }
        if (s_.compare(i_, 5, "false") == 0) {
            out.type = JsonValue::Type::Bool;
            out.boolValue = false;
            i_ += 5;
            return true;
        }
        return false;
    }

    bool parseNull(JsonValue &out) {
        if (s_.compare(i_, 4, "null") == 0) {
            out.type = JsonValue::Type::Null;
            i_ += 4;
            return true;
        }
        return false;
    }

    bool parseNumber(JsonValue &out) {
        std::size_t start = i_;
        while (i_ < s_.size()) {
            char c = s_[i_];
            if ((c >= '0' && c <= '9') || c == '-' || c == '+' || c == '.' ||
                c == 'e' || c == 'E') {
                ++i_;
            } else {
                break;
            }
        }
        if (i_ == start) return false;
        out.type = JsonValue::Type::Number;
        try {
            out.numberValue = std::stod(s_.substr(start, i_ - start));
        } catch (...) {
            out.numberValue = 0.0;
        }
        return true;
    }
};

// OpenAI chat/completions response:
//   { "choices": [ { "message": { "content": "..." } }, ... ] }
bool extractOpenAIText(const JsonValue &root, std::string &text,
                       std::string &error) {
    const JsonValue *choices = root.find("choices");
    if (!choices || choices->type != JsonValue::Type::Array ||
        choices->arrayValue.empty()) {
        error = "Phản hồi không có choices";
        return false;
    }

    const JsonValue &first = choices->arrayValue.front();
    const JsonValue *message = first.find("message");
    const JsonValue *content =
        message ? message->find("content") : nullptr;
    if (!content || content->type != JsonValue::Type::String ||
        content->stringValue.empty()) {
        error = "Phản hồi rỗng";
        return false;
    }

    text = content->stringValue;
    return true;
}

// Anthropic Messages API response:
//   { "content": [ {"type":"thinking",…}, {"type":"text","text":"…"} ],
//     "stop_reason": "end_turn" }
// The text block is NOT necessarily first — thinking models emit one or more
// thinking blocks ahead of it — so scan for the first "text" block instead of
// indexing content[0].
bool extractAnthropicText(const JsonValue &root, std::string &text,
                          std::string &error) {
    const JsonValue *content = root.find("content");
    if (!content || content->type != JsonValue::Type::Array ||
        content->arrayValue.empty()) {
        error = "Phản hồi không có content";
        return false;
    }

    for (const JsonValue &block : content->arrayValue) {
        const JsonValue *type = block.find("type");
        if (!type || type->type != JsonValue::Type::String ||
            type->stringValue != "text") {
            continue;
        }
        const JsonValue *value = block.find("text");
        if (value && value->type == JsonValue::Type::String &&
            !value->stringValue.empty()) {
            text = value->stringValue;
            return true;
        }
    }

    // No usable text block. A refusal is the one case worth naming explicitly,
    // otherwise the user just sees "empty response" for a deliberate decline.
    if (const JsonValue *stop = root.find("stop_reason");
        stop && stop->type == JsonValue::Type::String &&
        stop->stringValue == "refusal") {
        error = "Model đã từ chối yêu cầu này";
        return false;
    }

    error = "Phản hồi rỗng";
    return false;
}

// Pull the assistant text out of a provider response. Returns false and fills
// `error` if the shape is unexpected or an API error object is present.
bool extractText(Provider provider, const std::string &body, std::string &text,
                 std::string &error, std::string &model) {
    JsonValue root;
    JsonParser parser(body);
    if (!parser.parse(root) || root.type != JsonValue::Type::Object) {
        error = "Không phân tích được phản hồi JSON";
        return false;
    }

    // Read before the error check so a failed call still reports the model when
    // the server named one.
    if (const JsonValue *m = root.find("model");
        m && m->type == JsonValue::Type::String) {
        model = m->stringValue;
    }

    // Error envelope, identical in both APIs: { "error": { "message": "..." } }
    if (const JsonValue *err = root.find("error")) {
        if (err->type == JsonValue::Type::Object) {
            if (const JsonValue *msg = err->find("message");
                msg && msg->type == JsonValue::Type::String) {
                error = msg->stringValue;
                return false;
            }
        }
        error = "Lỗi từ API";
        return false;
    }

    return provider == Provider::Anthropic
               ? extractAnthropicText(root, text, error)
               : extractOpenAIText(root, text, error);
}

// Does this Claude model still accept `temperature`?
//
// The recent families (Opus 4.8/4.7, Sonnet 5/4.6, Fable 5) removed the
// sampling parameters and answer any request carrying one with a 400; Haiku
// still honours it. Deliberately an allow-list, not a deny-list: an unknown or
// future model then degrades to "temperature ignored" rather than to a hard 400.
bool anthropicAcceptsTemperature(const std::string &model) {
    return model.find("haiku") != std::string::npos;
}

std::size_t writeCallback(char *ptr, std::size_t size, std::size_t nmemb,
                          void *userdata) {
    auto *buffer = static_cast<std::string *>(userdata);
    buffer->append(ptr, size * nmemb);
    return size * nmemb;
}

}  // namespace

AIResult ai_generate(const AIConfig &config, const std::string &prompt) {
    AIResult result;

    if (config.apiKey.empty()) {
        result.error = "Chưa cấu hình API key";
        return result;
    }

    const bool anthropic = config.provider == Provider::Anthropic;

    std::string body;
    body += "{\"model\":\"" + jsonEscape(config.model) + "\",";
    body += "\"max_tokens\":" + std::to_string(config.maxTokens) + ",";
    if (!anthropic || anthropicAcceptsTemperature(config.model)) {
        // Format the temperature with the classic ("C") locale: std::to_string
        // / %f honour LC_NUMERIC, and under a locale like vi_VN that yields a
        // comma decimal separator ("0,3"), producing invalid JSON that the
        // server rejects with a 400. Force a dot regardless of the process
        // locale.
        std::ostringstream tempStream;
        tempStream.imbue(std::locale::classic());
        tempStream << config.temperature;
        body += "\"temperature\":" + tempStream.str() + ",";
    }
    if (!config.systemPrompt.empty() && anthropic) {
        // Messages API: the system prompt is a top-level field, not a message
        // with role "system" (that role does not exist there).
        body += "\"system\":\"" + jsonEscape(config.systemPrompt) + "\",";
    }
    // No `thinking` field: adaptive thinking is off by default on Opus 4.8 and
    // the extra latency would be felt inside an input method. Models where
    // thinking is always on (Fable 5) still work — extractAnthropicText() skips
    // the thinking blocks.
    body += "\"messages\":[";
    if (!config.systemPrompt.empty() && !anthropic) {
        body += "{\"role\":\"system\",\"content\":\"" +
                jsonEscape(config.systemPrompt) + "\"},";
    }
    body += "{\"role\":\"user\",\"content\":\"" + jsonEscape(prompt) + "\"}]}";

    // curl_global_init() is NOT thread-safe and must run exactly once before
    // any curl_easy_init(). ai_generate() is invoked from detached worker
    // threads (possibly concurrently across input contexts), so guard the
    // one-time global init with std::call_once instead of relying on the
    // implicit init inside curl_easy_init(), which would race.
    static std::once_flag curlInitFlag;
    std::call_once(curlInitFlag, [] { curl_global_init(CURL_GLOBAL_DEFAULT); });

    CURL *curl = curl_easy_init();
    if (!curl) {
        result.error = "Không khởi tạo được libcurl";
        return result;
    }

    std::string response;
    struct curl_slist *headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    // Keep the header strings alive until curl_easy_perform() returns:
    // curl_slist_append() copies, but that is easy to forget when editing.
    std::string authHeader;
    if (anthropic) {
        headers = curl_slist_append(
            headers, (std::string("anthropic-version: ") + kAnthropicVersion)
                         .c_str());
        if (config.oauthToken) {
            // OAuth access tokens authenticate as a bearer token and additionally
            // require the oauth beta header. Sending one via x-api-key is a 401.
            authHeader = "Authorization: Bearer " + config.apiKey;
            headers =
                curl_slist_append(headers, "anthropic-beta: oauth-2025-04-20");
        } else {
            authHeader = "x-api-key: " + config.apiKey;
        }
    } else {
        authHeader = "Authorization: Bearer " + config.apiKey;
    }
    headers = curl_slist_append(headers, authHeader.c_str());

    std::string endpoint = config.endpoint;
    if (endpoint.empty()) {
        endpoint = anthropic ? kAnthropicEndpoint : kOpenAIEndpoint;
    }

    curl_easy_setopt(curl, CURLOPT_URL, endpoint.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE,
                     static_cast<long>(body.size()));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, config.timeoutMs);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode rc = curl_easy_perform(curl);
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        result.error = std::string("Lỗi mạng: ") + curl_easy_strerror(rc);
        return result;
    }

    std::string text;
    std::string error;
    if (!extractText(config.provider, response, text, error, result.model)) {
        if (httpCode >= 400) {
            result.error = "HTTP " + std::to_string(httpCode) + ": " + error;
        } else {
            result.error = error;
        }
        return result;
    }

    result.ok = true;
    result.text = std::move(text);
    return result;
}

}  // namespace telebit::ai
