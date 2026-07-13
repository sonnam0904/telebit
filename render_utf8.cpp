#include "render_utf8.h"

#include "rime_table.h"

#include <cctype>
#include <unordered_map>

namespace telebit::internal {

// Returns the index (in shaped string) of the vowel that should receive the tone mark.
static int getMainVowelIndex(const std::string& shaped, bool modernTone) {
    if (shaped.empty()) return -1;
    int first_vowel = -1;
    std::vector<int> vowel_pos;
    vowel_pos.reserve(shaped.size());
    for (std::size_t i = 0; i < shaped.size(); ++i) {
        if (kInternalVowels.find(shaped[i]) != std::string_view::npos) {
            if (first_vowel < 0) first_vowel = static_cast<int>(i);
            vowel_pos.push_back(static_cast<int>(i));
        }
    }
    if (vowel_pos.empty()) return -1;

    std::string rime_from_vowel = shaped.substr(static_cast<std::size_t>(first_vowel));
    const auto& table = getRimeMainVowelTable();
    auto it = table.find(rime_from_vowel);
    int main_idx = (it != table.end()) ? it->second : 0;
    // Modern style: open rimes oa/oe/uy carry the tone on the second vowel.
    if (modernTone && (rime_from_vowel == "oa" || rime_from_vowel == "oe" ||
                       rime_from_vowel == "uy")) {
        main_idx = 1;
    }
    if (main_idx >= static_cast<int>(vowel_pos.size())) main_idx = 0;
    return vowel_pos[static_cast<std::size_t>(main_idx)];
}

std::string renderVowelNoTone(char v) {
    switch (v) {
        case 'a': return "a"; case 'A': return "ă"; case 'B': return "â";
        case 'e': return "e"; case 'E': return "ê";
        case 'i': return "i";
        case 'o': return "o"; case 'O': return "ô"; case 'Q': return "ơ";
        case 'u': return "u"; case 'U': return "ư";
        case 'y': return "y";
        default:  return std::string(1, v);
    }
}

std::string renderVowelWithTone(char v, int tone) {
    if (tone < 1 || tone > 5) return renderVowelNoTone(v);
    switch (v) {
        case 'a': switch (tone) { case 1: return "á"; case 2: return "à"; case 3: return "ả"; case 4: return "ã"; case 5: return "ạ"; } break;
        case 'A': switch (tone) { case 1: return "ắ"; case 2: return "ằ"; case 3: return "ẳ"; case 4: return "ẵ"; case 5: return "ặ"; } break;
        case 'B': switch (tone) { case 1: return "ấ"; case 2: return "ầ"; case 3: return "ẩ"; case 4: return "ẫ"; case 5: return "ậ"; } break;
        case 'e': switch (tone) { case 1: return "é"; case 2: return "è"; case 3: return "ẻ"; case 4: return "ẽ"; case 5: return "ẹ"; } break;
        case 'E': switch (tone) { case 1: return "ế"; case 2: return "ề"; case 3: return "ể"; case 4: return "ễ"; case 5: return "ệ"; } break;
        case 'i': switch (tone) { case 1: return "í"; case 2: return "ì"; case 3: return "ỉ"; case 4: return "ĩ"; case 5: return "ị"; } break;
        case 'o': switch (tone) { case 1: return "ó"; case 2: return "ò"; case 3: return "ỏ"; case 4: return "õ"; case 5: return "ọ"; } break;
        case 'O': switch (tone) { case 1: return "ố"; case 2: return "ồ"; case 3: return "ổ"; case 4: return "ỗ"; case 5: return "ộ"; } break;
        case 'Q': switch (tone) { case 1: return "ớ"; case 2: return "ờ"; case 3: return "ở"; case 4: return "ỡ"; case 5: return "ợ"; } break;
        case 'u': switch (tone) { case 1: return "ú"; case 2: return "ù"; case 3: return "ủ"; case 4: return "ũ"; case 5: return "ụ"; } break;
        case 'U': switch (tone) { case 1: return "ứ"; case 2: return "ừ"; case 3: return "ử"; case 4: return "ữ"; case 5: return "ự"; } break;
        case 'y': switch (tone) { case 1: return "ý"; case 2: return "ỳ"; case 3: return "ỷ"; case 4: return "ỹ"; case 5: return "ỵ"; } break;
        default: break;
    }
    return renderVowelNoTone(v);
}

std::string renderRimeUtf8(const std::string& shaped, int tone, bool modernTone) {
    std::string out;
    out.reserve(shaped.size() * 3);
    int main_idx = (tone != 0) ? getMainVowelIndex(shaped, modernTone) : -1;

    for (int i = 0; i < static_cast<int>(shaped.size()); ++i) {
        char c = shaped[static_cast<std::size_t>(i)];
        if (c == 'W') { out.push_back('w'); continue; }
        if (c == 'D') { out.append("đ"); continue; }
        if (kInternalVowels.find(c) != std::string_view::npos) {
            out.append(i == main_idx ? renderVowelWithTone(c, tone) : renderVowelNoTone(c));
            continue;
        }
        out.push_back(c);
    }
    return out;
}

std::string utf8ToUpper(const std::string& s) {
    static const std::unordered_map<std::string, std::string> kUpper = {
        {"ă","Ă"},{"â","Â"},{"ê","Ê"},{"ô","Ô"},{"ơ","Ơ"},{"ư","Ư"},{"đ","Đ"},
        {"á","Á"},{"à","À"},{"ả","Ả"},{"ã","Ã"},{"ạ","Ạ"},
        {"ắ","Ắ"},{"ằ","Ằ"},{"ẳ","Ẳ"},{"ẵ","Ẵ"},{"ặ","Ặ"},
        {"ấ","Ấ"},{"ầ","Ầ"},{"ẩ","Ẩ"},{"ẫ","Ẫ"},{"ậ","Ậ"},
        {"é","É"},{"è","È"},{"ẻ","Ẻ"},{"ẽ","Ẽ"},{"ẹ","Ẹ"},
        {"ế","Ế"},{"ề","Ề"},{"ể","Ể"},{"ễ","Ễ"},{"ệ","Ệ"},
        {"í","Í"},{"ì","Ì"},{"ỉ","Ỉ"},{"ĩ","Ĩ"},{"ị","Ị"},
        {"ó","Ó"},{"ò","Ò"},{"ỏ","Ỏ"},{"õ","Õ"},{"ọ","Ọ"},
        {"ố","Ố"},{"ồ","Ồ"},{"ổ","Ổ"},{"ỗ","Ỗ"},{"ộ","Ộ"},
        {"ớ","Ớ"},{"ờ","Ờ"},{"ở","Ở"},{"ỡ","Ỡ"},{"ợ","Ợ"},
        {"ú","Ú"},{"ù","Ù"},{"ủ","Ủ"},{"ũ","Ũ"},{"ụ","Ụ"},
        {"ứ","Ứ"},{"ừ","Ừ"},{"ử","Ử"},{"ữ","Ữ"},{"ự","Ự"},
        {"ý","Ý"},{"ỳ","Ỳ"},{"ỷ","Ỷ"},{"ỹ","Ỹ"},{"ỵ","Ỵ"},
    };

    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size();) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        if (c < 0x80u) {
            out.push_back(static_cast<char>(std::toupper(c)));
            ++i;
            continue;
        }
        std::size_t len = (c & 0xE0u) == 0xC0u ? 2 : (c & 0xF0u) == 0xE0u ? 3 : 4;
        if (i + len > s.size()) { out.append(s.substr(i)); break; }
        std::string cp = s.substr(i, len);
        auto it = kUpper.find(cp);
        out.append(it != kUpper.end() ? it->second : cp);
        i += len;
    }
    return out;
}

std::string applyWordCase(const std::string& conv, const std::string& original) {
    if (conv.empty() || original.empty()) return conv;
    bool all_upper = true;
    for (char c : original) {
        if (std::isalpha(static_cast<unsigned char>(c)) && !std::isupper(static_cast<unsigned char>(c))) {
            all_upper = false;
            break;
        }
    }
    if (all_upper) return utf8ToUpper(conv);

    if (std::isupper(static_cast<unsigned char>(original[0]))) {
        std::string upper = utf8ToUpper(conv);
        unsigned char c0 = static_cast<unsigned char>(conv[0]);
        std::size_t first_len = (c0 & 0x80u) == 0 ? 1 : (c0 & 0xE0u) == 0xC0u ? 2 : (c0 & 0xF0u) == 0xE0u ? 3 : 4;
        if (first_len <= conv.size() && first_len <= upper.size()) {
            return upper.substr(0, first_len) + conv.substr(first_len);
        }
    }
    return conv;
}

}  // namespace telebit::internal

