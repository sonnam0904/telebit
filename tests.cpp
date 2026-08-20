// Minimal C++ tests mirroring tests/test_vietnamese.py for the C++ port.

// Every check here is an assert, and the default build type is Release, which
// defines NDEBUG and would compile all of them away — the suite then "passes"
// without testing anything. Drop NDEBUG before <cassert> so the asserts stay
// live no matter how the caller configured the build.
#undef NDEBUG

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
    // "ươ" can be typed as "uow" in addition to "uw" + context — but only where
    // something follows it. Bare "ươ" is no Vietnamese rime, so a syllable-final
    // "uow" is uơ (huơ, quơ, thuở); see test_uo_vs_uouw.
    assert(telex_to_unicode("uow") == "uơ");
    assert(telex_to_unicode("uowi") == "ươi");
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

static void test_trailing_hat_escape() {
    // The hat key may land after the coda, so "data" reads as "dât". Repeating it
    // is the undo, and a trailing "aa" restores the literal letters.
    assert(telex_to_unicode("data") == "dât");
    assert(telex_to_unicode("dataa") == "data");
    assert(telex_to_unicode("gamaa") == "gama");
    // Casing survives the escape.
    assert(telex_to_unicode("Dataa") == "Data");
    assert(telex_to_unicode("DATAA") == "DATA");
    // "aaa" still escapes to a literal "aa" (checked before this rule).
    assert(telex_to_unicode("dataaa") == "dataa");

    // Adjacent "aa" is plain Telex: the prefix has no hat yet, so nothing escapes.
    assert(telex_to_unicode("caa") == "câ");
    assert(telex_to_unicode("aa") == "â");
    assert(telex_to_unicode("baa") == "bâ");
    // Only a *trailing* "aa" is an escape: English words that merely contain one
    // are untouched, and so are Vietnamese words still being typed.
    assert(telex_to_unicode("salaam") == "salaam");
    assert(telex_to_unicode("bazaar") == "bazaar");
    assert(telex_to_unicode("aardvark") == "aardvark");
    assert(telex_to_unicode("Isaac") == "Isaac");
    assert(telex_to_unicode("Canaan") == "Canaan");
    assert(telex_to_unicode("graal") == "graal");
    assert(telex_to_unicode("caan") == "cân");
    assert(telex_to_unicode("khuaay") == "khuây");
    // A trailing "aa" whose prefix never grew a hat stays literal: "sofa" is a
    // tone ("sòa"), so it keeps using the doubled-tone-key escape instead.
    assert(telex_to_unicode("sofaa") == "sofaa");
    assert(telex_to_unicode("soffa") == "sofa");
    assert(telex_to_unicode("javaa") == "javaa");
    assert(telex_to_unicode("pandaa") == "pandaa");

    // Same rule for the other two hat vowels: "mongo" reads as "mông", so the
    // repeated "o" is the undo that restores the literal "mongo".
    assert(telex_to_unicode("mongo") == "mông");
    assert(telex_to_unicode("mongoo") == "mongo");
    assert(telex_to_unicode("Mongoo") == "Mongo");
    assert(telex_to_unicode("bongoo") == "bongo");
    assert(telex_to_unicode("congoo") == "congo");
    // "ooo"/"eee" escape first, so one more key gives the literal double.
    assert(telex_to_unicode("mongooo") == "mongoo");
    // A trailing double whose prefix never grew a hat keeps plain Telex: these
    // are the English words the rule must not touch.
    assert(telex_to_unicode("zoo") == "zoo");
    assert(telex_to_unicode("igloo") == "igloo");
    assert(telex_to_unicode("voodoo") == "voodoo");
    assert(telex_to_unicode("kangaroo") == "kangaroo");
    assert(telex_to_unicode("agree") == "agree");
    assert(telex_to_unicode("moo") == "mô");
    assert(telex_to_unicode("tree") == "trê");
    // Only a *trailing* double escapes: "mongoose" keeps its raw keys.
    assert(telex_to_unicode("mongoose") == "mongoose");
    assert(telex_to_unicode("mongoos") == "mongoos");
}

// Rimes the spell-check table used to be missing: the conversion was correct but
// the gate rejected it, so the user got their raw keys back instead of the word.
static void test_rimes_from_dictionary() {
    // uôm
    assert(telex_to_unicode("buoomf") == "buồm");
    assert(telex_to_unicode("nhuoomj") == "nhuộm");
    assert(telex_to_unicode("luoomj thuoomj") == "luộm thuộm");
    // oam / oăm / oap / oăp
    assert(telex_to_unicode("ngoamj") == "ngoạm");
    assert(telex_to_unicode("khoawmf") == "khoằm");
    assert(telex_to_unicode("soajp") == "soạp");
    // uênh / uêch
    assert(telex_to_unicode("hueenh hoang") == "huênh hoang");
    assert(telex_to_unicode("khueechs ddaji") == "khuếch đại");
    assert(telex_to_unicode("ngueechj ngoacj") == "nguệch ngoạc");
    // êng (gi + êng) và yêm/yêng
    assert(telex_to_unicode("gieengs") == "giếng");
    assert(telex_to_unicode("gieeng") == "giêng");
    assert(telex_to_unicode("yeems") == "yếm");
    assert(telex_to_unicode("yeengr") == "yểng");
    // ưm — an interjection rime, not a standard one
    assert(telex_to_unicode("huwmf") == "hừm");
    assert(telex_to_unicode("uwmf") == "ừm");
    // oao / oem / oeng / uyp
    assert(telex_to_unicode("ngoaos") == "ngoáo");
    assert(telex_to_unicode("ngoems") == "ngoém");
    assert(telex_to_unicode("xoengr") == "xoẻng");
    assert(telex_to_unicode("tuyps") == "tuýp");
}

// uy + coda carries the tone on 'y'; the uynh entry used to point at 'u'.
static void test_uynh_tone_placement() {
    assert(telex_to_unicode("huynhf") == "huỳnh");
    assert(telex_to_unicode("khuynhr") == "khuỷnh");
    assert(telex_to_unicode("luynhs") == "luýnh");
    assert(telex_to_unicode("huychs") == "huých");
    // "qu" + y + coda is q + uy..., so these route through the same entries.
    assert(telex_to_unicode("quynhf") == "quỳnh");
    assert(telex_to_unicode("quyts") == "quýt");
    assert(telex_to_unicode("quytj") == "quỵt");
    // Every uy + coda rime agrees on where the tone goes, uyn included.
    assert(telex_to_unicode("quyns") == "quýn");
    assert(telex_to_unicode("quynf") == "quỳn");
    assert(telex_to_unicode("quyps") == "quýp");
    assert(telex_to_unicode("quychs") == "quých");
    // ...while bare "quy" keeps the "qu" onset: classic style writes "quý".
    assert(telex_to_unicode("quys") == "quý");
    assert(telex_to_unicode("quyf") == "quỳ");
    assert(telex_to_unicode("quyeenr") == "quyển");
    // "ya"/"yu" are not codas, so those are not syllables and stay raw rather
    // than growing a tone through the rejoin.
    assert(telex_to_unicode("quyaf") == "quyaf");
    assert(telex_to_unicode("quyuf") == "quyuf");
}

// The gi/qu rejoin lives in a helper both splitters call, so VNI must agree
// with Telex; it used to drop the tone on "gi" exactly as Telex once did.
static void test_vni_matches_telex_for_gi_and_qu() {
    const TelexOptions vni = vni_opts();
    assert(telex_to_unicode("gi1", vni) == "gí");
    assert(telex_to_unicode("gi2", vni) == "gì");
    assert(telex_to_unicode("gi3", vni) == "gỉ");
    assert(telex_to_unicode("gi4", vni) == "gĩ");
    assert(telex_to_unicode("gi5", vni) == "gị");
    assert(telex_to_unicode("quynh2", vni) == "quỳnh");
    assert(telex_to_unicode("quyt1", vni) == "quýt");
    assert(telex_to_unicode("huynh2", vni) == "huỳnh");
    // Same words, Telex spelling — the two must not disagree.
    assert(telex_to_unicode("gis") == "gí");
    assert(telex_to_unicode("quynhf") == "quỳnh");
    assert(telex_to_unicode("quyts") == "quýt");
}

// Bare "ươ" is not a rime, so a syllable-final "uow" must land on uơ.
static void test_uo_vs_uouw() {
    assert(telex_to_unicode("huow") == "huơ");
    assert(telex_to_unicode("khuow") == "khuơ");
    assert(telex_to_unicode("quow") == "quơ");
    assert(telex_to_unicode("thuowr") == "thuở");
    assert(telex_to_unicode("uowr") == "uở");
    // Anything after the "uow" is still ươ.
    assert(telex_to_unicode("nguowif") == "người");
    assert(telex_to_unicode("ruowuj") == "rượu");
    assert(telex_to_unicode("uownf") == "ườn");
    assert(telex_to_unicode("thuowngf") == "thường");
    // Decided on the finished rime, not by looking ahead mid-scan, so the other
    // keystroke order that reaches the same rime agrees instead of splitting.
    assert(telex_to_unicode("uwow") == telex_to_unicode("uow"));
    assert(telex_to_unicode("huwow") == telex_to_unicode("huow"));
    assert(telex_to_unicode("nguwow") == telex_to_unicode("nguow"));
}

// The trailing-hat undo eats the key that applied the hat, so a word that
// genuinely ends in the doubled vowel needs one more press — the same
// escalation as "chesss" -> chess. Pinned because the middle step looks like
// data loss and someone will otherwise "fix" it by reverting e/o.
static void test_trailing_hat_escalation() {
    assert(telex_to_unicode("tepe") == "têp");
    assert(telex_to_unicode("tepee") == "tepe");
    assert(telex_to_unicode("tepeee") == "tepee");
    assert(telex_to_unicode("lessee") == "lesse");
    assert(telex_to_unicode("lesseee") == "lessee");
    assert(telex_to_unicode("epeee") == "epee");
    // Without the rule "mongo" would have no spelling at all: the hat lands on
    // it unconditionally and no number of trailing o's would take it back off.
    assert(telex_to_unicode("mongo") == "mông");
    assert(telex_to_unicode("mongoo") == "mongo");
}

// "gi" is an onset, but with nothing left it is really g + i and the tone needs
// that 'i' to land on. Only "gif" used to be special-cased; the rest fell off.
static void test_gi_onset_keeps_tone() {
    assert(telex_to_unicode("gi") == "gi");
    assert(telex_to_unicode("gif") == "gì");
    assert(telex_to_unicode("gis") == "gí");
    assert(telex_to_unicode("gir") == "gỉ");
    assert(telex_to_unicode("gix") == "gĩ");
    assert(telex_to_unicode("gij") == "gị");
    assert(telex_to_unicode("Gif") == "Gì");
    assert(telex_to_unicode("GIF") == "GÌ");
    // The multi-vowel "gi" syllables are untouched.
    assert(telex_to_unicode("gia") == "gia");
    assert(telex_to_unicode("giaf") == "già");
    assert(telex_to_unicode("gioongs") == "giống");
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

static void test_double_tone_key_needs_a_conversion() {
    // The doubled tone key is an undo, and there is only something to undo when the
    // raw keys would have converted. "sofa" is a syllable ("sòa"), so "soffa" must
    // still collapse to the literal "sofa"...
    assert(telex_to_unicode("sofa") == "sòa");
    assert(telex_to_unicode("soffa") == "sofa");
    assert(telex_to_unicode("ass") == "as");
    assert(telex_to_unicode("chuss") == "chus");
    // ...while an English word that merely carries a syllable-shaped prefix keeps
    // every letter: nothing converted, so nothing needs undoing.
    assert(telex_to_unicode("coffee") == "coffee");
    assert(telex_to_unicode("toffee") == "toffee");
    assert(telex_to_unicode("office") == "office");
    assert(telex_to_unicode("mission") == "mission");
    assert(telex_to_unicode("essay") == "essay");
    assert(telex_to_unicode("assign") == "assign");
    assert(telex_to_unicode("afford") == "afford");
    assert(telex_to_unicode("affair") == "affair");
    assert(telex_to_unicode("suffer") == "suffer");
    assert(telex_to_unicode("difference") == "difference");
    // The rule leans on the spell-check restore to hand back the raw keys; with the
    // restore off, the old unconditional escape is still the best available answer.
    assert(telex_to_unicode("coffee", no_spell_check_opts()) == "cofee");
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
    // ...but only while the raw keys would otherwise have converted. "tieengssabc"
    // is no syllable, so the spell-check restore keeps every key the user typed
    // (see test_double_tone_key_needs_a_conversion). Without the restore the old
    // literal-passthrough behavior stands.
    assert(telex_to_unicode("tieengssabc") == "tieengssabc");
    assert(telex_to_unicode("tieengssabc", no_spell_check_opts()) == "tieengsabc");
    // The "ww" escape is unconditional: 'w' is never a plain Vietnamese letter.
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

static void test_dong_progressive_rimes() {
    // đ is only ever produced by a deliberate leading "dd", so an đ-initial
    // syllable in progress must keep its đ instead of bouncing back to "dd"
    // while the rime is still a bare vowel cluster (bug: "ddie" showed "ddie").
    assert(telex_to_unicode("ddie") == "đie");   // on the way to điếc/điết
    assert(telex_to_unicode("dduo") == "đuo");   // on the way to đuốc/được/đười
    // Final syllables for the rimes iêc/iết/uôc/ước/ươi/ươu still convert.
    assert(telex_to_unicode("ddieecs") == "điếc");
    assert(telex_to_unicode("ddieets") == "điết");
    assert(telex_to_unicode("dduoocs") == "đuốc");
    assert(telex_to_unicode("dduowcj") == "được");
    assert(telex_to_unicode("dduowi") == "đươi");
    assert(telex_to_unicode("dduowu") == "đươu");
    // The relaxation is đ-only: other onsets and English words are untouched.
    assert(telex_to_unicode("tie") == "tie");
    assert(telex_to_unicode("bie") == "bie");
    assert(telex_to_unicode("daddy") == "daddy");
    // An đ-initial rime with a coda that is not a valid syllable still restores.
    assert(telex_to_unicode("ddog") == "ddog");
    assert(telex_to_unicode("ddz") == "ddz");
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
    test_trailing_hat_escape();
    test_double_tone_key_needs_a_conversion();
    test_all_vowel_tone_combinations();
    test_basic_word_tones();
    test_additional_words();
    test_word_shapes_with_tones();
    test_any_position_modifiers();
    test_incremental_typing_detection();
    test_open_glide_rimes();
    test_rimes_from_dictionary();
    test_uynh_tone_placement();
    test_uo_vs_uouw();
    test_trailing_hat_escalation();
    test_vni_matches_telex_for_gi_and_qu();
    test_gi_onset_keeps_tone();
    test_spell_check_restore();
    test_vni_mode();
    test_modern_tone_style();
    test_engine_macros_and_vni();
    test_dong_progressive_rimes();
    std::cout << "All C++ tests passed.\n";
    return 0;
}

