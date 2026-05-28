// Minimal C++ tests mirroring tests/test_vietnamese.py for the C++ port.

#include "vietnamese.h"

#include <cassert>
#include <iostream>
#include <string>

static void test_tones() {
    assert(telex_to_unicode("as") == "á");
    assert(telex_to_unicode("ar") == "ả");
    assert(telex_to_unicode("ax") == "ã");
    assert(telex_to_unicode("aj") == "ạ");
    assert(telex_to_unicode("af") == "à");
    // z is literal, does not delete tone.
    assert(telex_to_unicode("asz") == "áz");
}

static void test_vowels() {
    assert(telex_to_unicode("aw") == "ă");
    assert(telex_to_unicode("aa") == "â");
    assert(telex_to_unicode("ee") == "ê");
    assert(telex_to_unicode("oo") == "ô");
    assert(telex_to_unicode("ow") == "ơ");
    assert(telex_to_unicode("uw") == "ư");
    // "ươ" can be typed as "uow" in addition to "uw" + context.
    assert(telex_to_unicode("uow") == "ươ");
    assert(telex_to_unicode("dd") == "đ");

    // Tone with z keeps tone, z is literal.
    assert(telex_to_unicode("aas") == "ấ");
    assert(telex_to_unicode("aasz") == "ấz");
}

static void test_special_gif() {
    assert(telex_to_unicode("gif") == "gì");
    assert(telex_to_unicode("Gif") == "Gì");
    assert(telex_to_unicode("GIF") == "GÌ");
}

static void test_combined() {
    assert(telex_to_unicode("tieengs") == "tiếng");
    assert(telex_to_unicode("vieetj") == "việt");
    assert(telex_to_unicode("hoas") == "hóa");
    assert(telex_to_unicode("vaof") == "vào");
    assert(telex_to_unicode("nguyeenx") == "nguyễn");
    assert(telex_to_unicode("dd") == "đ");
    assert(telex_to_unicode("tw") == "tư");
}

static void test_uy_tone_placement() {
    // "uy" -> tone on "u" (thúy)
    assert(telex_to_unicode("thuys") == "thúy");
}

static void test_english_with_w() {
    // sunwworld -> sunworld, Telex rules for uww/uw.
    assert(telex_to_unicode("sunwworld") == "sunworld");
    assert(telex_to_unicode("uw") == "ư");
    assert(telex_to_unicode("uww") == "uw");
}

static void test_convert_buffer() {
    assert(convert_buffer("tieengs vieetj") == "tiếng việt");
}

static void test_uppercase() {
    // Uppercase behavior is approximated: keep letters uppercase.
    assert(telex_to_unicode("AS") == "Á");
    assert(telex_to_unicode("VIEETJ NAM") == "VIỆT NAM");
    assert(telex_to_unicode("DDI") == "ĐI");
}

static void test_title_case() {
    assert(telex_to_unicode("Vieetj") == "Việt");
    assert(telex_to_unicode("Nguyeenx") == "Nguyễn");
    assert(convert_buffer("Tieengs Vieetj") == "Tiếng Việt");
}

static void test_english_passthrough() {
    // Pure English / technical strings should be unchanged.
    assert(telex_to_unicode("Hello, test!") == "Hello, test!");
    assert(telex_to_unicode("C++") == "C++");
    assert(telex_to_unicode("C6H12O6") == "C6H12O6");
}

static void test_mixed_vietnamese_english() {
    // Vietnamese + English / numbers mixed in one line.
    assert(telex_to_unicode("nguyeenx C++") == "nguyễn C++");
    assert(telex_to_unicode("Vieetj Nam 2024") == "Việt Nam 2024");
    assert(convert_buffer("nguyeenx 2024") == "nguyễn 2024");
    assert(convert_buffer("nguyeenx C6H12") == "nguyễn C6H12");
}

static void test_triple_vowels_english() {
    // English triple vowels should collapse without turning into Vietnamese vowels.
    assert(telex_to_unicode("leeech") == "leech");
    assert(telex_to_unicode("cooool") == "coool");
    assert(telex_to_unicode("baaad") == "baad");
    // English double-d: allow typing "dd" literally via "ddd" escape (eddy).
    assert(telex_to_unicode("edddy") == "eddy");
    // Non-contiguous triple with local double at cursor should escape IME.
    assert(telex_to_unicode("telee") == "tele");
}

static void test_all_vowel_tone_combinations() {
    // Simple vowels with all tones
    assert(telex_to_unicode("as") == "á");
    assert(telex_to_unicode("af") == "à");
    assert(telex_to_unicode("ar") == "ả");
    assert(telex_to_unicode("ax") == "ã");
    assert(telex_to_unicode("aj") == "ạ");

    assert(telex_to_unicode("es") == "é");
    assert(telex_to_unicode("ef") == "è");
    assert(telex_to_unicode("er") == "ẻ");
    assert(telex_to_unicode("ex") == "ẽ");
    assert(telex_to_unicode("ej") == "ẹ");

    assert(telex_to_unicode("is") == "í");
    assert(telex_to_unicode("if") == "ì");
    assert(telex_to_unicode("ir") == "ỉ");
    assert(telex_to_unicode("ix") == "ĩ");
    assert(telex_to_unicode("ij") == "ị");

    assert(telex_to_unicode("os") == "ó");
    assert(telex_to_unicode("of") == "ò");
    assert(telex_to_unicode("or") == "ỏ");
    assert(telex_to_unicode("ox") == "õ");
    assert(telex_to_unicode("oj") == "ọ");

    assert(telex_to_unicode("us") == "ú");
    assert(telex_to_unicode("uf") == "ù");
    assert(telex_to_unicode("ur") == "ủ");
    assert(telex_to_unicode("ux") == "ũ");
    assert(telex_to_unicode("uj") == "ụ");

    assert(telex_to_unicode("ys") == "ý");
    assert(telex_to_unicode("yf") == "ỳ");
    assert(telex_to_unicode("yr") == "ỷ");
    assert(telex_to_unicode("yx") == "ỹ");
    assert(telex_to_unicode("yj") == "ỵ");

    // ă (aw) with tones
    assert(telex_to_unicode("aws") == "ắ");
    assert(telex_to_unicode("awf") == "ằ");
    assert(telex_to_unicode("awr") == "ẳ");
    assert(telex_to_unicode("awx") == "ẵ");
    assert(telex_to_unicode("awj") == "ặ");

    // â (aa) with tones
    assert(telex_to_unicode("aas") == "ấ");
    assert(telex_to_unicode("aaf") == "ầ");
    assert(telex_to_unicode("aar") == "ẩ");
    assert(telex_to_unicode("aax") == "ẫ");
    assert(telex_to_unicode("aaj") == "ậ");

    // ê (ee) with tones
    assert(telex_to_unicode("ees") == "ế");
    assert(telex_to_unicode("eef") == "ề");
    assert(telex_to_unicode("eer") == "ể");
    assert(telex_to_unicode("eex") == "ễ");
    assert(telex_to_unicode("eej") == "ệ");

    // ô (oo) with tones
    assert(telex_to_unicode("oos") == "ố");
    assert(telex_to_unicode("oof") == "ồ");
    assert(telex_to_unicode("oor") == "ổ");
    assert(telex_to_unicode("oox") == "ỗ");
    assert(telex_to_unicode("ooj") == "ộ");

    // ơ (ow) with tones
    assert(telex_to_unicode("ows") == "ớ");
    assert(telex_to_unicode("owf") == "ờ");
    assert(telex_to_unicode("owr") == "ở");
    assert(telex_to_unicode("owx") == "ỡ");
    assert(telex_to_unicode("owj") == "ợ");

    // ư (uw) with tones
    assert(telex_to_unicode("uws") == "ứ");
    assert(telex_to_unicode("uwf") == "ừ");
    assert(telex_to_unicode("uwr") == "ử");
    assert(telex_to_unicode("uwx") == "ữ");
    assert(telex_to_unicode("uwj") == "ự");
}

static void test_basic_word_tones() {
    // "ba" with all tones
    assert(telex_to_unicode("bas") == "bá");
    assert(telex_to_unicode("baf") == "bà");
    assert(telex_to_unicode("bar") == "bả");
    assert(telex_to_unicode("bax") == "bã");
    assert(telex_to_unicode("baj") == "bạ");

    // "be" with all tones
    assert(telex_to_unicode("bes") == "bé");
    assert(telex_to_unicode("bef") == "bè");
    assert(telex_to_unicode("ber") == "bẻ");
    assert(telex_to_unicode("bex") == "bẽ");
    assert(telex_to_unicode("bej") == "bẹ");

    // "bo" with all tones
    assert(telex_to_unicode("bos") == "bó");
    assert(telex_to_unicode("bof") == "bò");
    assert(telex_to_unicode("bor") == "bỏ");
    assert(telex_to_unicode("box") == "bõ");
    assert(telex_to_unicode("boj") == "bọ");

    // "bu" with all tones
    assert(telex_to_unicode("bus") == "bú");
    assert(telex_to_unicode("buf") == "bù");
    assert(telex_to_unicode("bur") == "bủ");
    assert(telex_to_unicode("bux") == "bũ");
    assert(telex_to_unicode("buj") == "bụ");
}

static void test_additional_words() {
    // "má/mà/mả/mã/mạ"
    assert(telex_to_unicode("mas") == "má");
    assert(telex_to_unicode("maf") == "mà");
    assert(telex_to_unicode("mar") == "mả");
    assert(telex_to_unicode("max") == "mã");
    assert(telex_to_unicode("maj") == "mạ");

    // "chí/chì/chỉ/chĩ/chị"
    assert(telex_to_unicode("chis") == "chí");
    assert(telex_to_unicode("chif") == "chì");
    assert(telex_to_unicode("chir") == "chỉ");
    assert(telex_to_unicode("chix") == "chĩ");
    assert(telex_to_unicode("chij") == "chị");

    // "quyết"
    assert(telex_to_unicode("quyeets") == "quyết");
}

static void test_word_shapes_with_tones() {
    // Words using â/ă/ê/ô/ơ/ư inside syllables
    assert(telex_to_unicode("baws") == "bắ");
    assert(telex_to_unicode("bawf") == "bằ");
    assert(telex_to_unicode("baas") == "bấ");
    assert(telex_to_unicode("baaf") == "bầ");
    assert(telex_to_unicode("bees") == "bế");
    assert(telex_to_unicode("beef") == "bề");
    assert(telex_to_unicode("boos") == "bố");
    assert(telex_to_unicode("boof") == "bồ");
    assert(telex_to_unicode("bows") == "bớ");
    assert(telex_to_unicode("buws") == "bứ");
}

static void test_any_position_modifiers() {
    // Any-position tone/hat keys should be canonicalized and recomputed.
    assert(telex_to_unicode("tusaan") == "tuấn");
    assert(telex_to_unicode("tuanas") == "tuấn");
    assert(telex_to_unicode("ngojc") == "ngọc");
    assert(telex_to_unicode("hoanfg") == "hoàng");
    assert(telex_to_unicode("hongof") == "hồng");
    assert(telex_to_unicode("buowir") == "bưởi");
    assert(telex_to_unicode("uoirw") == "ưởi");
    assert(telex_to_unicode("uoiwr") == "ưởi");
    // Double tone escape: whole word is returned literally, with the pair collapsed.
    assert(telex_to_unicode("ass") == "as");
    assert(telex_to_unicode("tieengssabc") == "tieengsabc");
    assert(telex_to_unicode("tieengwwabc") == "tieengwabc");
    // Triple vowel escape: whole word literal, with triple collapsed to double.
    assert(telex_to_unicode("tieengaaabc") == "tieengaabc");
    // ây via delayed hat: "aya" pattern
    assert(telex_to_unicode("vayaj") == "vậy");
    // iêu/yêu via delayed hat across 'u'
    assert(telex_to_unicode("lieuej") == "liệu");
    assert(telex_to_unicode("kieuer") == "kiểu");
    // iêu with tone key inside: "ieu" + tone + "e"
    assert(telex_to_unicode("lieuje") == "liệu");
    assert(telex_to_unicode("huaws") == "hứa");
    assert(telex_to_unicode("chuaw") == "chưa");
    assert(telex_to_unicode("hopwj") == "hợp");
    assert(telex_to_unicode("hungws") == "hứng");
    assert(telex_to_unicode("bapws") == "bắp");
    // Rime-based: un + w + tone -> ứn/ữn; ua + w + tone -> ứa
    assert(telex_to_unicode("unws") == "ứn");
    assert(telex_to_unicode("unwx") == "ữn");
    assert(telex_to_unicode("uasw") == "ứa");
    // Tone placement for ươn/ương: tone must be on 'ư' (ướn, ướng).
    assert(telex_to_unicode("uowns") == "ướn");
    assert(telex_to_unicode("uowngs") == "ướng");

    // Tone key 'r' before 'n' should still work when Telex shaping is present (oo -> ô).
    assert(telex_to_unicode("oorn") == "ổn");

    // Tone placement for oai/oay: tone must be on 'a' (ngoại, xoáy).
    assert(telex_to_unicode("ngoaij") == "ngoại");
    assert(telex_to_unicode("xoays") == "xoáy");

}

static bool has_non_ascii(const std::string& s) {
    for (unsigned char c : s) {
        if (c & 0x80u) {
            return true;
        }
    }
    return false;
}

static void test_incremental_typing_detection() {
    // Simulate typing a pseudo-Vietnamese-looking but invalid word "ktaps":
    // with validation gate disabled, we may see Vietnamese Unicode.
    const std::string invalid_vn = "ktaps";
    for (std::size_t i = 1; i <= invalid_vn.size(); ++i) {
        std::string prefix = invalid_vn.substr(0, i);
        std::string out = telex_to_unicode(prefix);
        (void)out;
    }

    // Simulate typing a real Vietnamese Telex word "tieengs":
    // at some point, output should start containing Vietnamese Unicode.
    const std::string vn_telex = "tieengs";
    bool seen_vietnamese = false;
    for (std::size_t i = 1; i <= vn_telex.size(); ++i) {
        std::string prefix = vn_telex.substr(0, i);
        std::string out = telex_to_unicode(prefix);
        if (has_non_ascii(out)) {
            seen_vietnamese = true;
        }
    }
    assert(seen_vietnamese);
}

int main() {
    test_tones();
    test_vowels();
    test_special_gif();
    test_combined();
    test_uy_tone_placement();
    test_english_with_w();
    test_convert_buffer();
    test_uppercase();
    test_title_case();
    test_english_passthrough();
    test_mixed_vietnamese_english();
    test_triple_vowels_english();
    test_all_vowel_tone_combinations();
    test_basic_word_tones();
    test_additional_words();
    test_word_shapes_with_tones();
    test_any_position_modifiers();
    test_incremental_typing_detection();
    std::cout << "All C++ tests passed.\n";
    return 0;
}

