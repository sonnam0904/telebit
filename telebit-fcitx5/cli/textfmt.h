// Text measuring and wrapping for the box-drawn report.
//
// Separated out because every one of these has already been the source of a
// ragged table, and none of them needs a live system to test: they are pure
// string functions with exact expected outputs.

#pragma once

#include <cstddef>
#include <string>
#include <vector>

namespace telebit::doctor {

// Width in terminal cells, not bytes and not code points.
//
// Bytes are wrong because a Vietnamese letter is two or three of them. Code
// points are wrong too: the same letter arrives either precomposed (ở =
// U+1EDF, one code point) or decomposed (o + U+031B + U+0309, three), and a
// combining mark takes no cell of its own. Counting one pushes the table's
// right border out by exactly one column.
std::size_t display_width(const std::string &text);

// Pads to exactly `column` cells. Text that already fills the column gets no
// padding — a single space there is the classic off-by-one that makes a
// box-drawn report ragged.
std::string pad_to(const std::string &text, std::size_t column);

// Splits a word wider than the column, on a code-point boundary so a break can
// never land inside a character. Absolute paths and runtime refs contain no
// spaces and routinely exceed the column; without this they punch through the
// table border.
std::vector<std::string> hard_break(const std::string &word, std::size_t width);

// Greedy word wrap on display width. Never returns an empty vector, so callers
// can index line 0 unconditionally.
std::vector<std::string> wrap(const std::string &text, std::size_t width);

}  // namespace telebit::doctor
