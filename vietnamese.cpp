// Vietnamese Telex -> Unicode conversion (glue translation unit).
// Heavy logic lives in:
// - rime_table.* (tables)
// - canonicalize.* (escape + parsing + canonicalization + shaping)
// - render_utf8.* (tone placement + UTF-8 rendering + casing)

#include "vietnamese.h"

#include "canonicalize.h"
#include "render_utf8.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace {

static std::string convert_word(const std::string& word) {
    using namespace telebit::internal;

    if (word.empty()) return word;

    std::string lower = word;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Telex: "gif" -> "gì". "f" is huyền on "i", but "gi" is also a valid onset so the
    // generic parser would treat the whole syllable as onset "gi" and drop the tone.
    if (lower == "gif") {
        return applyWordCase("gì", word);
    }

    std::string escapedRaw;
    if (applyEscapeRules(word, lower, escapedRaw)) {
        return escapedRaw;
    }

    std::string body;
    int tone = 0;
    bool strip = false;
    std::tie(body, tone, strip) = extractTone(lower);

    if (normalizeTripleVowels(body)) {
        return applyWordCase(body, word);
    }

    std::string onset, rimeRaw;
    splitOnsetRime(body, onset, rimeRaw);
    rimeRaw = canonicalizeRimeByTable(rimeRaw);

    std::string shaped = applyShapesRime(rimeRaw);
    std::string rimeUtf8 = renderRimeUtf8(shaped, strip ? 0 : tone);

    return applyWordCase(onset + rimeUtf8, word);
}

}  // namespace

std::string telex_to_unicode(const std::string& raw) {
    if (raw.empty()) return raw;

    std::string result;
    result.reserve(raw.size());

    std::size_t i = 0;
    while (i < raw.size()) {
        if (std::isspace(static_cast<unsigned char>(raw[i]))) {
            result.push_back(raw[i]);
            ++i;
            continue;
        }

        std::size_t start = i;
        while (i < raw.size() && !std::isspace(static_cast<unsigned char>(raw[i]))) ++i;
        std::string word = raw.substr(start, i - start);

        bool all_alpha = true;
        for (char c : word) {
            if (!std::isalpha(static_cast<unsigned char>(c))) { all_alpha = false; break; }
        }

        result += all_alpha ? convert_word(word) : word;
    }

    return result;
}
