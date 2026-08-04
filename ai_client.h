// Minimal LLM text-generation client for the Telebit fcitx5 addon.
//
// Speaks two wire protocols over HTTPS (libcurl) and returns the generated
// text:
//
//   Provider::OpenAI     POST /v1/chat/completions — also covers every
//                        OpenAI-compatible service (Azure OpenAI, groq, local
//                        runners, …) since endpoint/model/key are configurable.
//   Provider::Anthropic  POST /v1/messages (the Claude Messages API).
//
// ai_generate() is synchronous and blocking: it performs a network round-trip
// and must be called from a worker thread, never from the fcitx key-event
// handler.

#pragma once

#include <string>

namespace telebit::ai {

// Which wire protocol `endpoint` speaks. The two differ in more than the URL:
// auth header, where the system prompt lives, and the response shape.
enum class Provider {
    OpenAI,
    Anthropic,
};

inline constexpr const char *kOpenAIEndpoint =
    "https://api.openai.com/v1/chat/completions";
inline constexpr const char *kAnthropicEndpoint =
    "https://api.anthropic.com/v1/messages";

// Sent with every Anthropic request; pins the Messages API wire format.
inline constexpr const char *kAnthropicVersion = "2023-06-01";

struct AIConfig {
    Provider provider = Provider::OpenAI;
    // Empty means "the default endpoint for `provider`".
    std::string endpoint;
    std::string apiKey;
    // Set when `apiKey` holds an OAuth access token (e.g. the value of
    // CLAUDE_CODE_OAUTH_TOKEN) rather than a plain API key. Anthropic
    // authenticates the two differently and rejects the wrong pairing with a
    // 401, so the caller has to tell us which one it handed over.
    bool oauthToken = false;
    std::string model = "gpt-4.1-mini";
    // Optional system instruction prepended to the request.
    std::string systemPrompt;
    int maxTokens = 4096;
    // Sampling temperature. Lower = more deterministic / instruction-obedient,
    // which small local models need to avoid echoing the prompt scaffolding.
    // For Provider::Anthropic this is sent only to models that still accept it
    // (the Haiku family); Opus 4.8/4.7, Sonnet 5/4.6 and Fable 5 removed the
    // sampling parameters and reject `temperature` with a 400.
    double temperature = 0.3;
    // Overall request timeout in milliseconds.
    long timeoutMs = 30000;
};

struct AIResult {
    bool ok = false;
    std::string text;   // generated text on success
    std::string error;  // human-readable error on failure
    // The model the server reports having served the request (both APIs echo a
    // top-level "model"). This is the ONLY trustworthy answer to "which model
    // replied?" — asking the model itself is not: without being told who it is
    // in the system prompt, it guesses from training data and frequently names
    // an older Claude. Surfaced for the TELEBIT_AI_DEBUG log.
    std::string model;
};

// Perform a single blocking generation request for `prompt`.
// Safe to call from any thread; does NOT touch fcitx state.
AIResult ai_generate(const AIConfig &config, const std::string &prompt);

}  // namespace telebit::ai
