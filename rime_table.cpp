#include "rime_table.h"

namespace telebit::internal {

const std::vector<std::string>& getOnsets() {
    static const std::vector<std::string> kOnsets = {
        "ngh", "ng", "nh", "gh", "gi", "ch", "kh", "ph", "qu", "th", "tr",
        "b", "c", "d", "g", "h", "k", "l", "m", "n", "p", "q", "r", "s", "t", "v", "x",
        ""  // vowel-initial
    };
    return kOnsets;
}

const std::unordered_map<std::string, int>& getRimeMainVowelTable() {
    static const std::unordered_map<std::string, int> kTable = []() {
        std::unordered_map<std::string, int> m;
        auto add = [&m](const std::string& rime, int main_idx) { m[rime] = main_idx; };

        // Single vowels & diphthongs (internal form).
        // NOTE: internal placeholders: A=ă, B=â, E=ê, O=ô, Q=ơ, U=ư.
        add("a", 0); add("ai", 0); add("ay", 0); add("ao", 0); add("au", 0);
        add("A", 0); add("B", 0);
        add("e", 0); add("eo", 0); add("i", 0); add("ia", 0); add("iu", 0);
        add("E", 0);
        add("o", 0); add("oi", 0); add("oa", 0); add("oe", 0);
        add("O", 0); add("Q", 0);
        // oai/oay: tone is placed on 'a' (second vowel)
        add("oai", 1); add("oay", 1);
        add("u", 0); add("ua", 0); add("ui", 0); add("uy", 0);
        add("U", 0);
        add("y", 0);
        // Open glide rimes without a coda that also need explicit entries so the
        // spell-check gate accepts them (tone on the vowel noted in each comment).
        add("Uu", 0);   // ưu: cứu, lưu, mưu, hưu (tone on ư)
        add("UQu", 1);  // ươu: rượu, hươu, khướu (tone on ơ)
        add("uyu", 1);  // uyu: khuỷu (tone on y)
        add("oeo", 1);  // oeo: ngoèo, ngoẻo (tone on e)
        add("uBy", 1);  // uây: khuây, khuẩy (tone on â)

        // âu, ây
        add("Bu", 0); add("By", 0);
        // êu
        add("Eu", 0);
        // iêu, yêu
        add("iEu", 1); add("yEu", 1);
        // ôi, ơi
        add("Oi", 0); add("Qi", 0);
        // ưi, ưa
        add("Ui", 0); add("Ua", 0);
        // uôi, ươi (tone on ơ for ươi)
        add("uOi", 1); add("UQi", 1);
        // uê, uơ, uô
        add("uE", 1); add("uQ", 1); add("uO", 1);
        // uya
        add("uya", 1);

        // m/p finals
        add("am", 0); add("ap", 0); add("Am", 0); add("Ap", 0);
        add("Bm", 0); add("Bp", 0); add("em", 0); add("ep", 0);
        add("Em", 0); add("Ep", 0); add("im", 0); add("ip", 0);
        add("iEm", 1); add("iEp", 1);
        add("om", 0); add("op", 0); add("Om", 0); add("Op", 0);
        add("Qm", 0); add("Qp", 0); add("um", 0); add("up", 0);
        add("UQm", 1); add("UQp", 1);

        // n/t finals
        add("an", 0); add("at", 0); add("An", 0); add("At", 0);
        add("Bn", 0); add("Bt", 0); add("en", 0); add("et", 0);
        add("En", 0); add("Et", 0); add("in", 0); add("it", 0);
        add("iEn", 1); add("iEt", 1);
        add("on", 0); add("ot", 0); add("On", 0); add("Ot", 0);
        add("Qn", 0); add("Qt", 0); add("un", 0); add("ut", 0);
        // ưn/ưt allowed for any-position w + tone
        add("Un", 0); add("Ut", 0);
        add("uOn", 1); add("uOt", 1); add("UQn", 1); add("UQt", 1);
        add("oan", 1); add("oat", 1); add("oAn", 1); add("oAt", 1);
        add("uBn", 1); add("uBt", 1); add("oen", 1); add("oet", 1);
        // uyên, uyết
        add("uyEn", 2); add("uyEt", 2);
        // yên, yết (quyết, khuyết, chuyến, ...)
        add("yEn", 1); add("yEt", 1);

        // ng/c finals
        add("ang", 0); add("ac", 0); add("Ang", 0); add("Ac", 0);
        add("Bng", 0); add("Bc", 0); add("eng", 0); add("ec", 0);
        add("iEng", 1); add("iEc", 1);
        add("ong", 0); add("oc", 0); add("Ong", 0); add("Oc", 0);
        add("ung", 0); add("uc", 0); add("Ung", 0); add("Uc", 0);
        add("uOng", 1); add("uOc", 1); add("UQng", 1); add("UQc", 1);
        add("oang", 1); add("oac", 1); add("oAng", 1); add("oAc", 1);
        add("uBng", 1); add("uBc", 1);

        // nh/ch finals
        add("anh", 0); add("ach", 0); add("Enh", 0); add("Ech", 0);
        add("inh", 0); add("ich", 0); add("oanh", 1); add("oach", 1);
        add("uynh", 0); add("uych", 1);

        // uyn/uyt
        add("uyn", 0); add("uyt", 1);

        return m;
    }();
    return kTable;
}

}  // namespace telebit::internal

