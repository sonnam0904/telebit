// Vietnamese Telex/VNI -> Unicode conversion (C++ port of engine/vietnamese.py)
// All identifiers and comments are in English as requested.

#pragma once

#include <string>

// Conversion options shared by the core engine and the fcitx5 addon.
struct TelexOptions {
    // Restore the raw keystrokes when the converted word is not a valid
    // Vietnamese syllable (spell-check with auto-restore, like Unikey's
    // "tự khôi phục phím với từ sai").
    bool spellCheckRestore = true;
    // Interpret VNI digit modifiers (1-5 tones, 0 clears tone, 6 hat, 7 horn,
    // 8 breve, 9 đ) instead of Telex letters.
    bool vniMode = false;
    // Modern tone placement for the open rimes oa/oe/uy (hoà/khoẻ/thuý)
    // instead of the classic style (hòa/khỏe/thúy).
    bool modernTone = false;
};

// Convert a full Telex/VNI string (may contain spaces) to Vietnamese Unicode.
// Example: "tieengs vieetj" -> "tiếng việt"
std::string telex_to_unicode(const std::string& raw, const TelexOptions& opts);

// Backward-compatible overload using default options (Telex, spell check on).
std::string telex_to_unicode(const std::string& raw);

// Convert an input buffer (used while typing) to Vietnamese Unicode.
// Semantically the same as telex_to_unicode but kept for structural parity.
inline std::string convert_buffer(const std::string& buffer) {
    return telex_to_unicode(buffer);
}

inline std::string convert_buffer(const std::string& buffer, const TelexOptions& opts) {
    return telex_to_unicode(buffer, opts);
}
