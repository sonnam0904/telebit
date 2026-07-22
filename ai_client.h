// Minimal LLM text-generation client for the Telebit fcitx5 addon.
//
// Calls an OpenAI-style chat/completions endpoint over HTTPS (libcurl) and
// returns the generated text. Defaults target the OpenAI API
// (https://api.openai.com/v1/chat/completions, model gpt-4.1-mini), but the
// endpoint / model / key are all configurable so the same client can point at
// any OpenAI-compatible service (Azure OpenAI, groq, local runners, …).
//
// ai_generate() is synchronous and blocking: it performs a network round-trip
// and must be called from a worker thread, never from the fcitx key-event
// handler.

#pragma once

#include <string>

namespace telebit::ai {

struct AIConfig {
    std::string endpoint = "https://api.openai.com/v1/chat/completions";
    std::string apiKey;
    std::string model = "gpt-4.1-mini";
    // Optional system instruction prepended to the request.
    std::string systemPrompt;
    int maxTokens = 4096;
    // Sampling temperature. Lower = more deterministic / instruction-obedient,
    // which small local models need to avoid echoing the prompt scaffolding.
    double temperature = 0.3;
    // Overall request timeout in milliseconds.
    long timeoutMs = 30000;
};

struct AIResult {
    bool ok = false;
    std::string text;   // generated text on success
    std::string error;  // human-readable error on failure
};

// Perform a single blocking generation request for `prompt`.
// Safe to call from any thread; does NOT touch fcitx state.
AIResult ai_generate(const AIConfig &config, const std::string &prompt);

}  // namespace telebit::ai
