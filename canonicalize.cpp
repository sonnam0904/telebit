#include "canonicalize.h"

#include "rime_table.h"

#include <algorithm>
#include <cctype>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace telebit::internal {

// Telex tone keys: s=sắc(1), f=huyền(2), r=hỏi(3), x=ngã(4), j=nặng(5). No key = ngang(0).
static const std::unordered_map<char, int> kToneKey = {
    {'s', 1}, {'f', 2}, {'r', 3}, {'x', 4}, {'j', 5},
};

static bool isAsciiVowel(char c) {
    return std::string_view("aeiouy").find(static_cast<char>(std::tolower(static_cast<unsigned char>(c)))) != std::string_view::npos;
}

// Runs the plain Telex pipeline on a lowercase word and reports whether the result
// is a valid Vietnamese syllable. Used to decide if a doubled tone key really was
// a tone application (escape) rather than plain English letters.
static bool telexWordConverts(const std::string& lowerWord) {
    std::string body;
    int tone = 0;
    bool strip = false;
    std::tie(body, tone, strip) = extractTone(lowerWord);

    std::string onset, rimeRaw;
    splitOnsetRime(body, onset, rimeRaw);
    rimeRaw = canonicalizeRimeByTable(rimeRaw);
    std::string shaped = applyShapesRime(rimeRaw);
    return isValidSyllable(onset, shaped);
}

bool applyEscapeRules(const std::string& word, const std::string& lower, std::string& outRaw) {
    struct Esc { const char* pat; int patLen; int outLen; bool toneKey; };
    const Esc escapes[] = {
        {"ss", 2, 1, true}, {"ff", 2, 1, true}, {"rr", 2, 1, true},
        {"xx", 2, 1, true}, {"jj", 2, 1, true}, {"ww", 2, 1, false},
        {"aaa", 3, 2, false}, {"eee", 3, 2, false}, {"ooo", 3, 2, false},
        // Allow typing literal "dd" (avoid Telex dd->đ) by using "ddd" escape.
        {"ddd", 3, 2, false},
    };
    std::size_t escPos = std::string::npos;
    const Esc* esc = nullptr;
    for (const auto& e : escapes) {
        std::size_t from = 0;
        while (true) {
            std::size_t p = lower.find(e.pat, from);
            if (p == std::string::npos) break;
            if (e.toneKey) {
                // A doubled tone key is only an escape when the first key actually
                // applied a tone: there must be a vowel before it and the prefix up to
                // (and including) that key must form a valid syllable. Otherwise the
                // letters are literal (English words like "address", "chess" prefixes).
                bool vowelBefore = false;
                for (std::size_t i = 0; i < p; ++i) {
                    if (isAsciiVowel(lower[i])) { vowelBefore = true; break; }
                }
                if (!vowelBefore || !telexWordConverts(lower.substr(0, p + 1))) {
                    from = p + 1;
                    continue;
                }
            }
            if (escPos == std::string::npos || p < escPos) {
                escPos = p;
                esc = &e;
            }
            break;
        }
    }
    if (escPos != std::string::npos && esc != nullptr) {
        outRaw.clear();
        outRaw.reserve(word.size() - 1);
        outRaw.append(word.substr(0, escPos));
        outRaw.append(word.substr(escPos, static_cast<std::size_t>(esc->outLen)));
        std::size_t after = escPos + static_cast<std::size_t>(esc->patLen);
        if (after < word.size()) outRaw.append(word.substr(after));
        return true;
    }
    return false;
}

bool normalizeTripleVowels(std::string& s) {
    bool changed = false;
    auto replaceAll = [&](const std::string& from, const std::string& to) {
        for (std::size_t pos = 0; (pos = s.find(from, pos)) != std::string::npos; pos += to.size()) {
            s.replace(pos, from.size(), to);
            changed = true;
        }
    };
    replaceAll("eee", "ee");
    replaceAll("aaa", "aa");
    replaceAll("ooo", "oo");
    return changed;
}

void splitOnsetRime(const std::string& body, std::string& onset, std::string& rimeRaw) {
    onset.clear();
    rimeRaw.clear();
    if (body.empty()) return;

    std::string lower = body;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

    // Special: "dd" or "dd..." (đ + optional rime) has no onset.
    if (lower.size() >= 2 && lower[0] == 'd' && lower[1] == 'd') {
        onset.clear();
        rimeRaw = lower;
        return;
    }

    const auto& onsets = getOnsets();
    for (const std::string& o : onsets) {
        if (o.empty()) {
            if (!lower.empty() && isAsciiVowel(lower[0])) {
                onset.clear();
                rimeRaw = lower;
                return;
            }
            continue;
        }
        if (lower.size() >= o.size() && lower.compare(0, o.size(), o) == 0) {
            std::string rest = lower.substr(o.size());
            if (rest.empty() || isAsciiVowel(rest[0]) || rest[0] == 'w') {
                onset = o;
                rimeRaw = rest;
                return;
            }
        }
    }

    if (!lower.empty() && !isAsciiVowel(lower[0])) {
        onset = lower.substr(0, 1);
        rimeRaw = lower.substr(1);
        return;
    }
    onset.clear();
    rimeRaw = lower;
}

std::tuple<std::string, int, bool> extractTone(const std::string& wordLower) {
    if (wordLower.empty()) return std::make_tuple(wordLower, 0, false);

    int firstVowel = -1;
    int lastVowel = -1;
    for (std::size_t i = 0; i < wordLower.size(); ++i) {
        if (isAsciiVowel(wordLower[i])) {
            if (firstVowel < 0) firstVowel = static_cast<int>(i);
            lastVowel = static_cast<int>(i);
        }
    }
    if (lastVowel < 0) return std::make_tuple(wordLower, 0, false);

    std::string suffix = wordLower.substr(static_cast<std::size_t>(lastVowel + 1));

    // Any-position tone: last tone key after first vowel wins; remove all such tone keys.
    int tone = 0;
    for (int i = firstVowel + 1; i < static_cast<int>(wordLower.size()); ++i) {
        char c = wordLower[static_cast<std::size_t>(i)];
        if (kToneKey.count(c)) {
            tone = kToneKey.at(c);
        }
    }
    if (tone == 0 && !suffix.empty() && kToneKey.count(suffix.back())) {
        tone = kToneKey.at(suffix.back());
    }

    std::string body;
    body.reserve(wordLower.size());
    for (int i = 0; i < static_cast<int>(wordLower.size()); ++i) {
        char c = wordLower[static_cast<std::size_t>(i)];
        if (i <= firstVowel) { body.push_back(c); continue; }
        if (kToneKey.count(c)) {
            continue;
        }
        body.push_back(c);
    }

    return std::make_tuple(body, tone, false);
}

std::string applyShapesRime(std::string s) {
    if (s.empty()) return s;

    // Step 0: collapse ww. uww->uW, oww->oW, aww->aW (one literal w); else ww->W.
    std::string step;
    step.reserve(s.size());
    for (std::size_t i = 0; i < s.size();) {
        if (i + 1 < s.size() && s[i] == 'w' && s[i + 1] == 'w') {
            if (!step.empty()) {
                char p = step.back();
                if (p == 'u' || p == 'o' || p == 'a') {
                    step.push_back('W');
                    i += 2;
                    continue;
                }
            }
            step.push_back('W');
            i += 2;
            continue;
        }
        step.push_back(s[i]);
        ++i;
    }
    s = std::move(step);

    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size();) {
        if (s[i] == 'W') { out.push_back('W'); ++i; continue; }
        if (i + 2 < s.size() && s[i] == 'u' && s[i + 1] == 'o' && s[i + 2] == 'w') {
            out.push_back('U'); out.push_back('Q');
            i += 3;
            continue;
        }
        if (i + 1 < s.size()) {
            char a = s[i], b = s[i + 1];
            if (b == 'W') { out.push_back(a); out.push_back('W'); i += 2; continue; }
            if (a == 'd' && b == 'd') { out.push_back('D'); i += 2; continue; }
            if (a == 'a' && b == 'a') { out.push_back('B'); i += 2; continue; }
            if (a == 'e' && b == 'e') { out.push_back('E'); i += 2; continue; }
            if (a == 'e' && b == 'w') { out.push_back('E'); i += 2; continue; }
            if (a == 'o' && b == 'o') { out.push_back('O'); i += 2; continue; }
            if (a == 'u' && b == 'w') { out.push_back('U'); i += 2; continue; }
            // uaw -> ưa (not uă): if prev is 'u', output U then 'a', skip 'w'.
            if (!out.empty() && out.back() == 'u' && a == 'a' && b == 'w') {
                out.back() = 'U';
                out.push_back('a');
                i += 2;
                continue;
            }
            if (a == 'a' && b == 'w') { out.push_back('A'); i += 2; continue; }
            if (a == 'o' && b == 'w') { out.push_back('Q'); i += 2; continue; }
        }
        if (s[i] == 'w') {
            if (!out.empty()) {
                char& p = out.back();
                if (p == 'a') { p = 'A'; ++i; continue; }
                if (p == 'o') { p = 'Q'; ++i; continue; }
                if (p == 'u') { p = 'U'; ++i; continue; }
            }
            out.push_back('U');
            ++i;
            continue;
        }
        out.push_back(s[i]);
        ++i;
    }
    return out;
}

std::string canonicalizeRimeByTable(const std::string& rimeRaw) {
    if (rimeRaw.empty()) return rimeRaw;

    auto firstVowelIdx = [](const std::string& shaped) -> int {
        for (int i = 0; i < static_cast<int>(shaped.size()); ++i) {
            if (kInternalVowels.find(shaped[static_cast<std::size_t>(i)]) != std::string_view::npos) return i;
        }
        return -1;
    };

    auto shapedKey = [&](const std::string& cand) -> std::string {
        std::string shaped = applyShapesRime(cand);
        int fv = firstVowelIdx(shaped);
        if (fv < 0) return {};
        return shaped.substr(static_cast<std::size_t>(fv));
    };

    const auto& table = getRimeMainVowelTable();
    auto matches = [&](const std::string& cand) -> bool {
        std::string key = shapedKey(cand);
        return !key.empty() && table.find(key) != table.end();
    };

    if (matches(rimeRaw)) return rimeRaw;

    // Locate vowel cluster bounds (ASCII a/e/i/o/u/y only).
    int firstV = -1, lastV = -1;
    for (int i = 0; i < static_cast<int>(rimeRaw.size()); ++i) {
        char c = rimeRaw[static_cast<std::size_t>(i)];
        if (isAsciiVowel(c)) {
            if (firstV < 0) firstV = i;
            lastV = i;
        }
    }
    if (firstV < 0) return rimeRaw;

    auto inCluster = [&](int idx) -> bool { return idx >= firstV && idx <= static_cast<int>(rimeRaw.size()); };

    std::vector<std::string> cur{rimeRaw};
    std::unordered_set<std::string> seen;
    seen.insert(rimeRaw);

    auto push = [&](const std::string& s, std::vector<std::string>& next) {
        if (seen.insert(s).second) next.push_back(s);
    };

    for (int depth = 0; depth < 2; ++depth) {
        std::vector<std::string> next;
        for (const auto& s : cur) {
            // Move duplicated a/e/o to be adjacent (hat).
            for (char v : std::string("aeo")) {
                std::vector<int> pos;
                for (int i = 0; i < static_cast<int>(s.size()); ++i) {
                    if (!inCluster(i)) continue;
                    if (s[static_cast<std::size_t>(i)] == v) pos.push_back(i);
                }
                for (std::size_t k = 1; k < pos.size(); ++k) {
                    int i = pos[k], j = pos[k - 1];
                    if (i == j + 1) continue;
                    std::string t = s;
                    t.erase(static_cast<std::size_t>(i), 1);
                    t.insert(static_cast<std::size_t>(j + 1), 1, v);
                    if (matches(t)) return t;
                    push(t, next);
                }
            }

            // Move w to after a/o/u inside cluster.
            for (int wi = 0; wi < static_cast<int>(s.size()); ++wi) {
                if (s[static_cast<std::size_t>(wi)] != 'w') continue;
                if (wi > 0 && s[static_cast<std::size_t>(wi - 1)] == 'w') continue; // keep ww
                if (!inCluster(wi)) continue;
                for (int vi = firstV; vi <= lastV; ++vi) {
                    char vc = s[static_cast<std::size_t>(vi)];
                    if (vc != 'a' && vc != 'o' && vc != 'u') continue;
                    int target = vi + 1;
                    if (target == wi) continue;
                    std::string t = s;
                    t.erase(static_cast<std::size_t>(wi), 1);
                    if (target > wi) target -= 1;
                    t.insert(static_cast<std::size_t>(target), 1, 'w');
                    if (matches(t)) return t;
                    push(t, next);
                }
            }
        }
        cur.swap(next);
    }

    return rimeRaw;
}

bool isValidSyllable(const std::string& onset, const std::string& shapedRime) {
    const auto& onsets = getOnsets();
    bool onsetOk = false;
    for (const auto& o : onsets) {
        if (o == onset) { onsetOk = true; break; }
    }
    if (!onsetOk) return false;

    std::string_view rime{shapedRime};
    if (!rime.empty() && rime.front() == 'D') rime.remove_prefix(1);  // đ + rime
    if (rime.empty()) return true;
    if (kInternalVowels.find(rime.front()) == std::string_view::npos) return false;
    for (char c : rime) {
        // Literal 'w' placeholder or a mid-rime đ never occur in Vietnamese.
        if (c == 'W' || c == 'D') return false;
    }

    const auto& table = getRimeMainVowelTable();
    std::string key{rime};
    if (table.find(key) != table.end()) return true;
    // Accept prefixes of valid rimes so partially typed syllables keep converting
    // (e.g. "tiê" while on the way to "tiên").
    for (const auto& kv : table) {
        const std::string& k = kv.first;
        if (k.size() > key.size() && k.compare(0, key.size(), key) == 0) return true;
    }
    return false;
}

void splitOnsetRimeShaped(const std::string& body, std::string& onset, std::string& rime) {
    onset.clear();
    rime.clear();
    if (body.empty()) return;

    auto isVowelPh = [](char c) {
        return kInternalVowels.find(c) != std::string_view::npos;
    };

    // 'D' (đ) has no onset; keep it in the rime so the renderer emits it.
    if (body[0] == 'D') {
        rime = body;
        return;
    }

    for (const std::string& o : getOnsets()) {
        if (o.empty()) {
            if (isVowelPh(body[0])) {
                rime = body;
                return;
            }
            continue;
        }
        if (body.size() >= o.size() && body.compare(0, o.size(), o) == 0) {
            std::string rest = body.substr(o.size());
            if (rest.empty() || isVowelPh(rest[0])) {
                onset = o;
                rime = rest;
                return;
            }
        }
    }

    if (!isVowelPh(body[0])) {
        onset = body.substr(0, 1);
        rime = body.substr(1);
        return;
    }
    rime = body;
}

VniWord translateVniWord(const std::string& word, const std::string& lower) {
    auto isDigit = [](char c) { return c >= '0' && c <= '9'; };
    VniWord res;

    // A doubled modifier digit escapes the IME: collapse each pair, output literally.
    bool hasDoubledDigit = false;
    for (std::size_t i = 0; i + 1 < lower.size(); ++i) {
        if (isDigit(lower[i]) && lower[i + 1] == lower[i]) { hasDoubledDigit = true; break; }
    }
    if (hasDoubledDigit) {
        res.kind = VniWord::Kind::LiteralEscape;
        for (std::size_t i = 0; i < word.size();) {
            if (isDigit(lower[i]) && i + 1 < word.size() && lower[i + 1] == lower[i]) {
                res.body.push_back(word[i]);
                i += 2;
            } else {
                res.body.push_back(word[i]);
                ++i;
            }
        }
        return res;
    }

    // A trailing digit run with two or more tone digits (0-5) is a literal number
    // ("nam2024", "top10"), while a shape+tone combo like "viet65" still converts.
    std::size_t trail = 0;
    int trailingToneDigits = 0;
    while (trail < lower.size() && isDigit(lower[lower.size() - 1 - trail])) {
        char c = lower[lower.size() - 1 - trail];
        if (c >= '0' && c <= '5') ++trailingToneDigits;
        ++trail;
    }
    if (trailingToneDigits >= 2) {
        res.kind = VniWord::Kind::Raw;
        return res;
    }

    std::string body;
    int tone = 0;
    for (char c : lower) {
        if (!isDigit(c)) {
            body.push_back(c);
            continue;
        }
        std::size_t pos = std::string::npos;
        switch (c) {
            case '1': tone = 1; break;
            case '2': tone = 2; break;
            case '3': tone = 3; break;
            case '4': tone = 4; break;
            case '5': tone = 5; break;
            case '0': tone = 0; break;
            case '6':  // hat: a->â, e->ê, o->ô
                pos = body.find_last_of("aeo");
                if (pos == std::string::npos) { res.kind = VniWord::Kind::Raw; return res; }
                body[pos] = body[pos] == 'a' ? 'B' : body[pos] == 'e' ? 'E' : 'O';
                break;
            case '7':  // horn: u->ư, o->ơ
                pos = body.find_last_of("uo");
                if (pos == std::string::npos) { res.kind = VniWord::Kind::Raw; return res; }
                body[pos] = body[pos] == 'u' ? 'U' : 'Q';
                break;
            case '8':  // breve: a->ă
                pos = body.find_last_of('a');
                if (pos == std::string::npos) { res.kind = VniWord::Kind::Raw; return res; }
                body[pos] = 'A';
                break;
            case '9':  // d->đ
                pos = body.find_last_of('d');
                if (pos == std::string::npos) { res.kind = VniWord::Kind::Raw; return res; }
                body[pos] = 'D';
                break;
        }
    }

    res.kind = VniWord::Kind::Converted;
    res.body = std::move(body);
    res.tone = tone;
    return res;
}

}  // namespace telebit::internal

