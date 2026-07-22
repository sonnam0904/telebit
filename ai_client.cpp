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

// Pull the assistant text out of an OpenAI chat/completions response:
//   { "choices": [ { "message": { "content": "..." } }, ... ] }
// Returns false and fills `error` if the shape is unexpected or an API error
// object is present.
bool extractText(const std::string &body, std::string &text, std::string &error) {
    JsonValue root;
    JsonParser parser(body);
    if (!parser.parse(root) || root.type != JsonValue::Type::Object) {
        error = "Không phân tích được phản hồi JSON";
        return false;
    }

    // API error envelope: { "error": { "message": "..." } }
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

    // Build request body (OpenAI chat/completions):
    //   {"model":..,"max_tokens":N,"messages":[{"role":"system",..},{"role":"user",..}]}
    std::string body;
    body += "{\"model\":\"" + jsonEscape(config.model) + "\",";
    body += "\"max_tokens\":" + std::to_string(config.maxTokens) + ",";
    // Format the temperature with the classic ("C") locale: std::to_string /
    // %f honour LC_NUMERIC, and under a locale like vi_VN that yields a comma
    // decimal separator ("0,3"), producing invalid JSON that the server
    // rejects with a 400. Force a dot regardless of the process locale.
    {
        std::ostringstream tempStream;
        tempStream.imbue(std::locale::classic());
        tempStream << config.temperature;
        body += "\"temperature\":" + tempStream.str() + ",";
    }
    body += "\"messages\":[";
    if (!config.systemPrompt.empty()) {
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
    std::string authHeader = "Authorization: Bearer " + config.apiKey;
    headers = curl_slist_append(headers, authHeader.c_str());

    curl_easy_setopt(curl, CURLOPT_URL, config.endpoint.c_str());
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
    if (!extractText(response, text, error)) {
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
