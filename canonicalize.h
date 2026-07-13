#pragma once

#include <string>
#include <tuple>

namespace telebit::internal {

// Applies IME escape rules (literal passthrough with collapse). Returns true if triggered.
bool applyEscapeRules(const std::string& word, const std::string& lower, std::string& outRaw);

// Normalizes English-like triple-vowel runs without turning them into Vietnamese shapes.
// Returns true if any change was made.
bool normalizeTripleVowels(std::string& s);

// Splits a Telex body into onset + raw rime (lowercase).
void splitOnsetRime(const std::string& body, std::string& onset, std::string& rimeRaw);

// Splits an already-shaped body (may contain internal placeholders A/B/E/O/Q/U/D)
// into onset + shaped rime. Used by the VNI pipeline.
void splitOnsetRimeShaped(const std::string& body, std::string& onset, std::string& rime);

// Extracts the tone key (if any) and returns (body_without_tone_keys, tone, strip=false).
std::tuple<std::string, int, bool> extractTone(const std::string& wordLower);

// Returns a canonical raw-rime Telex spelling that matches the rime table when possible.
std::string canonicalizeRimeByTable(const std::string& rimeRaw);

// Converts a raw rime Telex spelling into an internal shaped form (placeholders A/B/E/O/Q/U/D/W).
std::string applyShapesRime(std::string rimeRaw);

// Spell-check gate: true when onset + shaped rime form a (possibly partially typed)
// valid Vietnamese syllable. Rimes that are a prefix of a valid rime are accepted so
// conversion keeps working while a word is still being typed.
bool isValidSyllable(const std::string& onset, const std::string& shapedRime);

// Result of translating one VNI word (digits as modifiers) into the internal form.
struct VniWord {
    enum class Kind {
        Converted,      // body is a pre-shaped lowercase body, tone extracted
        LiteralEscape,  // body is literal output (doubled digit escape), typed case kept
        Raw             // keep the original word untouched
    };
    Kind kind = Kind::Raw;
    std::string body;
    int tone = 0;
};

// Translates a VNI word (must contain at least one digit) into the internal form.
// VNI modifiers: 1-5 tones, 0 clears tone, 6 hat (a/e/o), 7 horn (u/o), 8 breve (a), 9 đ.
VniWord translateVniWord(const std::string& word, const std::string& lower);

}  // namespace telebit::internal
