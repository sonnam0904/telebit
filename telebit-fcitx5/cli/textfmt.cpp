#include "textfmt.h"

namespace telebit::doctor {
namespace {

// Byte length of the UTF-8 sequence starting at `byte`. Malformed input falls
// through to 1, which keeps the caller advancing instead of looping forever.
std::size_t sequence_length(unsigned char byte) {
    if ((byte & 0xE0) == 0xC0) return 2;
    if ((byte & 0xF0) == 0xE0) return 3;
    if ((byte & 0xF8) == 0xF0) return 4;
    return 1;
}

}  // namespace

std::size_t display_width(const std::string &text) {
    std::size_t width = 0;
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char byte = text[i];
        const std::size_t length = sequence_length(byte);

        char32_t code = byte;
        if (length == 2) code = byte & 0x1F;
        else if (length == 3) code = byte & 0x0F;
        else if (length == 4) code = byte & 0x07;
        for (std::size_t k = 1; k < length && i + k < text.size(); ++k) {
            code = (code << 6) | (static_cast<unsigned char>(text[i + k]) & 0x3F);
        }

        // Combining Diacritical Marks — the block that carries the Vietnamese
        // horn and tone marks when text arrives decomposed.
        const bool combining = code >= 0x0300 && code <= 0x036F;
        if (!combining) ++width;
        i += length;
    }
    return width;
}

std::string pad_to(const std::string &text, std::size_t column) {
    const std::size_t width = display_width(text);
    return text + std::string(width < column ? column - width : 0, ' ');
}

std::vector<std::string> hard_break(const std::string &word, std::size_t width) {
    std::vector<std::string> pieces;
    std::string current;
    for (std::size_t i = 0; i < word.size();) {
        const std::size_t length = sequence_length(word[i]);
        const std::string glyph = word.substr(i, length);
        if (!current.empty() && display_width(current + glyph) > width) {
            pieces.push_back(current);
            current.clear();
        }
        current += glyph;
        i += length;
    }
    if (!current.empty()) pieces.push_back(current);
    return pieces;
}

std::vector<std::string> wrap(const std::string &text, std::size_t width) {
    std::vector<std::string> lines;
    std::string current;
    const auto flush = [&] {
        if (!current.empty()) lines.push_back(current);
        current.clear();
    };

    std::size_t start = 0;
    while (start <= text.size()) {
        const std::size_t space = text.find(' ', start);
        const std::string word = text.substr(
            start, space == std::string::npos ? std::string::npos : space - start);
        if (!word.empty()) {
            if (display_width(word) > width) {
                flush();
                for (const auto &piece : hard_break(word, width)) lines.push_back(piece);
                // Keep the tail open so the following word can still join it.
                if (!lines.empty()) {
                    current = lines.back();
                    lines.pop_back();
                }
            } else if (current.empty()) {
                current = word;
            } else if (display_width(current) + 1 + display_width(word) <= width) {
                current += " " + word;
            } else {
                flush();
                current = word;
            }
        }
        if (space == std::string::npos) break;
        start = space + 1;
    }
    flush();
    if (lines.empty()) lines.emplace_back();
    return lines;
}

}  // namespace telebit::doctor
