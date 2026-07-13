#pragma once

#include <string>

namespace telebit::internal {

// Renders one internal vowel placeholder without a tone mark to UTF-8.
std::string renderVowelNoTone(char v);

// Renders one internal vowel placeholder with a given tone mark to UTF-8.
std::string renderVowelWithTone(char v, int tone);

// Renders a shaped rime to UTF-8, applying tone to the main vowel.
// modernTone places the tone on the second vowel of oa/oe/uy (hoà/khoẻ/thuý)
// instead of the classic style (hòa/khỏe/thúy).
std::string renderRimeUtf8(const std::string& shaped, int tone, bool modernTone = false);

// Uppercases ASCII and Vietnamese UTF-8 letters deterministically (no locale dependency).
std::string utf8ToUpper(const std::string& s);

// Applies original word casing (ALLCAPS / Title / lower) to the converted UTF-8 word.
std::string applyWordCase(const std::string& conv, const std::string& original);

}  // namespace telebit::internal

