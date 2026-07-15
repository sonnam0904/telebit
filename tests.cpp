// Minimal C++ tests mirroring tests/test_vietnamese.py for the C++ port.

#include "engine.h"
#include "vietnamese.h"

#include <cassert>
#include <iostream>
#include <memory>
#include <string>

static TelexOptions vni_opts() {
    TelexOptions o;
    o.vniMode = true;
    return o;
}

static TelexOptions no_spell_check_opts() {
    TelexOptions o;
    o.spellCheckRestore = false;
    return o;
}

static TelexOptions modern_tone_opts() {
    TelexOptions o;
    o.modernTone = true;
    return o;
}

static void test_tones() {
    assert(telex_to_unicode("as") == "á");
    assert(telex_to_unicode("ar") == "ả");
    assert(telex_to_unicode("ax") == "ã");
    assert(telex_to_unicode("aj") == "ạ");
    assert(telex_to_unicode("af") == "à");
    // "áz" is not a valid syllable: spell check restores the raw keys.
    assert(telex_to_unicode("asz") == "asz");
    // Without spell check the legacy behavior remains (z literal, tone kept).
    assert(telex_to_unicode("asz", no_spell_check_opts()) == "áz");
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

    // Tone with z keeps tone, z is literal (only without the spell-check gate).
    assert(telex_to_unicode("aas") == "ấ");
    assert(telex_to_unicode("aasz") == "aasz");
    assert(telex_to_unicode("aasz", no_spell_check_opts()) == "ấz");
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
    // Non-contiguous triple: not a valid syllable, spell check keeps the raw word.
    assert(telex_to_unicode("telee") == "telee");
}

static void test_open_glide_rimes() {
    // Open rimes that the spell-check gate must recognize as valid Vietnamese.
    // ưu (Uu) — tone on ư.
    assert(telex_to_unicode("uwu") == "ưu");
    assert(telex_to_unicode("cuwus") == "cứu");
    assert(telex_to_unicode("luwu") == "lưu");
    assert(telex_to_unicode("muwu") == "mưu");
    assert(telex_to_unicode("huwu") == "hưu");
    // ươu (UQu) — tone on ơ.
    assert(telex_to_unicode("ruowuj") == "rượu");
    assert(telex_to_unicode("huowu") == "hươu");
    assert(telex_to_unicode("khuowus") == "khướu");
    // uyu — tone on y (khuỷu).
    assert(telex_to_unicode("khuyur") == "khuỷu");
    // oeo — tone on e (ngoèo).
    assert(telex_to_unicode("ngoeof") == "ngoèo");
    assert(telex_to_unicode("ngoeor") == "ngoẻo");
    // uây (uBy) — tone on â (khuây).
    assert(telex_to_unicode("khuaay") == "khuây");
    assert(telex_to_unicode("khuaayj") == "khuậy");
}

static void test_spell_check_restore() {
    // English words whose letters look like Telex modifiers must survive intact:
    // tone keys (s/f/r/x/j) may not delete real characters...
    assert(telex_to_unicode("person") == "person");
    assert(telex_to_unicode("first") == "first");
    assert(telex_to_unicode("kangaroo") == "kangaroo");
    // ...mid-word "dd" is not đ...
    assert(telex_to_unicode("add") == "add");
    assert(telex_to_unicode("daddy") == "daddy");
    assert(telex_to_unicode("sudden") == "sudden");
    assert(telex_to_unicode("riddle") == "riddle");
    assert(telex_to_unicode("address") == "address");
    // ...and double vowels in invalid syllables are not hats.
    assert(telex_to_unicode("cheese") == "cheese");
    assert(telex_to_unicode("employee") == "employee");
    assert(telex_to_unicode("coffee") != "cofê");  // ff still escapes -> "cofee"
    assert(telex_to_unicode("toolbox") == "toolbox");
    assert(telex_to_unicode("book") == "book");
    assert(telex_to_unicode("food") == "food");
    assert(telex_to_unicode("zoo") == "zoo");

    // Valid syllables still convert normally.
    assert(telex_to_unicode("tieengs vieetj") == "tiếng việt");
    assert(telex_to_unicode("nguyeenx") == "nguyễn");

    // Mid-typing prefixes of valid rimes keep converting (tiê on the way to tiên).
    assert(telex_to_unicode("tiee") == "tiê");

    // Legacy behavior is available with the gate off.
    assert(telex_to_unicode("person", no_spell_check_opts()) == "péon");
}

static void test_vni_mode() {
    const TelexOptions vni = vni_opts();
    // Tones 1-5 (sắc, huyền, hỏi, ngã, nặng), any position.
    assert(telex_to_unicode("ba1", vni) == "bá");
    assert(telex_to_unicode("cha2o", vni) == "chào");
    assert(telex_to_unicode("ba3", vni) == "bả");
    assert(telex_to_unicode("ba4", vni) == "bã");
    assert(telex_to_unicode("ban5", vni) == "bạn");
    // 0 clears the tone (mid-word; a trailing digit pair is a literal number).
    assert(telex_to_unicode("toa10n", vni) == "toan");
    assert(telex_to_unicode("ba10", vni) == "ba10");
    // 6 hat, 7 horn, 8 breve, 9 đ.
    assert(telex_to_unicode("vie65t", vni) == "việt");
    assert(telex_to_unicode("viet65", vni) == "việt");
    assert(telex_to_unicode("tua6n", vni) == "tuân");
    assert(telex_to_unicode("toi6", vni) == "tôi");
    assert(telex_to_unicode("tu7o7ng", vni) == "tương");
    assert(telex_to_unicode("a8n", vni) == "ăn");
    assert(telex_to_unicode("d9i", vni) == "đi");
    assert(telex_to_unicode("di9", vni) == "đi");
    assert(telex_to_unicode("nguye64n", vni) == "nguyễn");
    // Casing is preserved.
    assert(telex_to_unicode("Vie65t", vni) == "Việt");
    assert(telex_to_unicode("VIE65T", vni) == "VIỆT");
    // Doubled digit escapes to a literal digit.
    assert(telex_to_unicode("a11", vni) == "a1");
    // Trailing numbers and invalid syllables stay literal.
    assert(telex_to_unicode("nam2024", vni) == "nam2024");
    assert(telex_to_unicode("2024", vni) == "2024");
    assert(telex_to_unicode("box1", vni) == "box1");
    // Plain letters are never modifiers in VNI (English is safe).
    assert(telex_to_unicode("person", vni) == "person");
    assert(telex_to_unicode("address", vni) == "address");
    assert(telex_to_unicode("cheese", vni) == "cheese");
    assert(telex_to_unicode("caan", vni) == "caan");
}

static void test_modern_tone_style() {
    const TelexOptions modern = modern_tone_opts();
    // Classic placement (default): hòa, khỏe, thúy.
    assert(telex_to_unicode("hoas") == "hóa");
    assert(telex_to_unicode("khoer") == "khỏe");
    assert(telex_to_unicode("thuys") == "thúy");
    // Modern placement: hoá, khoẻ, thuý.
    assert(telex_to_unicode("hoas", modern) == "hoá");
    assert(telex_to_unicode("khoer", modern) == "khoẻ");
    assert(telex_to_unicode("thuys", modern) == "thuý");
    // Rimes with a coda are unaffected.
    assert(telex_to_unicode("hoanfg", modern) == "hoàng");
    assert(telex_to_unicode("ngoaij", modern) == "ngoại");
}

static void test_engine_macros_and_vni() {
    // Macro: raw buffer "vn" expands at commit.
    EngineVietCpp engine;
    auto macros = std::make_shared<EngineVietCpp::MacroTable>();
    (*macros)["vn"] = "Việt Nam";
    engine.setMacros(macros);
    engine.process_key_event('v', 0, 0);
    engine.process_key_event('n', 0, 0);
    KeyResult res = engine.process_key_event(KEYVAL_SPACE, 0, 0);
    assert(res.handled);
    assert(res.commit_text == "Việt Nam ");

    // VNI: digits join the buffer and convert at commit.
    EngineVietCpp vniEngine;
    vniEngine.setOptions(vni_opts());
    for (char c : std::string("vie65t")) {
        vniEngine.process_key_event(static_cast<std::uint32_t>(c), 0, 0);
    }
    res = vniEngine.process_key_event(KEYVAL_SPACE, 0, 0);
    assert(res.commit_text == "việt ");

    // A leading digit is not buffered (stays literal in the app).
    EngineVietCpp digitEngine;
    digitEngine.setOptions(vni_opts());
    res = digitEngine.process_key_event('1', 0, 0);
    assert(!res.handled);
    assert(digitEngine.buffer().empty());
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
    test_open_glide_rimes();
    test_spell_check_restore();
    test_vni_mode();
    test_modern_tone_style();
    test_engine_macros_and_vni();
    std::cout << "All C++ tests passed.\n";
    return 0;
}

