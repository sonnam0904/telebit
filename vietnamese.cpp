// Vietnamese Telex/VNI -> Unicode conversion (glue translation unit).
// Heavy logic lives in:
// - rime_table.* (tables)
// - canonicalize.* (escape + parsing + canonicalization + shaping + validation)
// - render_utf8.* (tone placement + UTF-8 rendering + casing)

#include "vietnamese.h"

#include "canonicalize.h"
#include "render_utf8.h"

#include <algorithm>
#include <cctype>
#include <string>

namespace {

static std::string convert_word_vni(const std::string& word, const std::string& lower,
                                    const TelexOptions& opts) {
    using namespace telebit::internal;

    bool hasDigit = std::any_of(lower.begin(), lower.end(), [](unsigned char c) {
        return std::isdigit(c) != 0;
    });
    // Without digits there is nothing a VNI modifier could have changed.
    if (!hasDigit) return word;

    VniWord vni = translateVniWord(word, lower);
    if (vni.kind == VniWord::Kind::Raw) return word;
    if (vni.kind == VniWord::Kind::LiteralEscape) return vni.body;

    std::string onset, rime;
    splitOnsetRimeShaped(vni.body, onset, rime);
    std::string rimeUtf8 = renderRimeUtf8(rime, vni.tone, opts.modernTone);

    std::string converted = applyWordCase(onset + rimeUtf8, word);
    if (opts.spellCheckRestore && converted != word && !isValidSyllable(onset, rime)) {
        return word;
    }
    return converted;
}

static std::string convert_word(const std::string& word, const TelexOptions& opts) {
    using namespace telebit::internal;

    if (word.empty()) return word;

    std::string lower = word;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    if (opts.vniMode) {
        return convert_word_vni(word, lower, opts);
    }

    std::string escapedRaw;
    if (applyEscapeRules(word, lower, escapedRaw, opts.spellCheckRestore)) {
        return escapedRaw;
    }

    std::string body;
    int tone = 0;
    bool strip = false;
    std::tie(body, tone, strip) = extractTone(lower);

    if (normalizeTripleVowels(body)) {
        // A triple vowel only survives to this point when tone keys were removed
        // from between the vowels — an English word like "cheese"; restore it.
        if (opts.spellCheckRestore && tone != 0) return word;
        return applyWordCase(body, word);
    }

    std::string onset, rimeRaw;
    splitOnsetRime(body, onset, rimeRaw);
    rimeRaw = canonicalizeRimeByTable(rimeRaw);

    std::string shaped = applyShapesRime(rimeRaw);
    std::string rimeUtf8 = renderRimeUtf8(shaped, strip ? 0 : tone, opts.modernTone);

    std::string converted = applyWordCase(onset + rimeUtf8, word);
    if (opts.spellCheckRestore && converted != word && !isValidSyllable(onset, shaped)) {
        return word;
    }
    return converted;
}

}  // namespace

std::string telex_to_unicode(const std::string& raw, const TelexOptions& opts) {
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

        bool has_alpha = false;
        bool convertible = true;
        for (char c : word) {
            unsigned char uc = static_cast<unsigned char>(c);
            if (std::isalpha(uc)) {
                has_alpha = true;
            } else if (std::isdigit(uc)) {
                // Digits are VNI modifiers; in Telex mode they make the word literal.
                if (!opts.vniMode) { convertible = false; break; }
            } else {
                convertible = false;
                break;
            }
        }
        if (!has_alpha) convertible = false;

        result += convertible ? convert_word(word, opts) : word;
    }

    return result;
}

std::string telex_to_unicode(const std::string& raw) {
    return telex_to_unicode(raw, TelexOptions{});
}
