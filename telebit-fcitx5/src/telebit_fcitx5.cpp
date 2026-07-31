/*
 * telebit-fcitx5 - Vietnamese Telex input method for Fcitx5
 * Engine implementation that reuses EngineVietCpp from the core C++ logic.
 */

#include "telebit_fcitx5.h"
#include "ai_client.h"
#include "vietnamese.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <thread>
#include <utility>

// Opt-in debug logging for the AI prompt flow. These messages can include the
// assembled prompt, pasted clipboard contents and the API response text, so
// logging is OFF unless TELEBIT_AI_DEBUG is set to a non-empty value in the
// environment — a released build must never leak this to stderr/journald.
static bool telebitAiDebugEnabled() {
    static const bool enabled = [] {
        const char *v = std::getenv("TELEBIT_AI_DEBUG");
        return v && v[0] != '\0';
    }();
    return enabled;
}
#define TELEBIT_AI_LOG(...)                                   \
    do {                                                      \
        if (telebitAiDebugEnabled()) {                        \
            fprintf(stderr, "[telebit-ai] " __VA_ARGS__);     \
            fprintf(stderr, "\n");                            \
            fflush(stderr);                                   \
        }                                                     \
    } while (0)

#include <fcitx-config/iniparser.h>
#include <fcitx-utils/event.h>
#include <fcitx/addonmanager.h>
#include <fcitx/event.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputpanel.h>
#include <fcitx/instance.h>
#include <fcitx/text.h>
#include <fcitx-utils/keysym.h>

using namespace fcitx;

namespace {

// Animated "thinking" indicator shown while an AI request is in flight.
// The marker cycles through these frames on a timer.
const char *const kAiSpinnerFrames[] = {"[>---] ", "[->--] ", "[-->-] ", "[--->] "};
constexpr int kAiSpinnerFrameCount = 4;
// How often the ellipsis advances, in microseconds (~400ms).
constexpr std::uint64_t kAiSpinnerIntervalUs = 200000;

// Count UTF-8 characters (codepoints) in a string.
int utf8CharCount(const std::string &s) {
    int count = 0;
    for (std::size_t i = 0; i < s.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(s[i]);
        // Leading bytes: 0xxxxxxx, 110xxxxx, 1110xxxx, 11110xxx
        if ((c & 0x80u) == 0 || (c & 0xC0u) == 0xC0u) {
            ++count;
        }
    }
    return count;
}

// Find common prefix length in bytes, stopping only at UTF-8 codepoint boundaries.
std::size_t commonPrefixBytes(const std::string &s1, const std::string &s2) {
    std::size_t n = std::min(s1.size(), s2.size());
    std::size_t i = 0;
    while (i < n && s1[i] == s2[i]) {
        ++i;
    }
    // Backtrack if we are in the middle of a multi-byte sequence.
    while (i > 0 && (static_cast<unsigned char>(s1[i]) & 0xC0u) == 0x80u) {
        --i;
    }
    return i;
}

std::string toLowerAscii(std::string s) {
    for (auto &ch : s) {
        if (ch >= 'A' && ch <= 'Z') {
            ch = static_cast<char>(ch - 'A' + 'a');
        }
    }
    return s;
}

// Programs that need preedit mode instead of direct commit: their text fields
// report SurroundingText support but handle deleteSurroundingText unreliably, so
// rewriting a word in place corrupts it. Browsers are the main offenders (both
// Gecko and Blink), hence firefox + the Chrome/Chromium family — the exact name
// fcitx reports varies by frontend ("chrome" via the GTK/Wayland module,
// "google-chrome" from WM_CLASS under X11), so all variants are listed.
//
// Used for BOTH paths: the fresh-install default in the header, and
// recordSeenProgram() below, which is what an existing config file goes
// through — there the entry is created (enabled) the first time the program is
// focused. Once the entry exists the user's own choice wins forever after.
bool isDefaultPreeditProgram(const std::string &programLower) {
    static const std::unordered_set<std::string> kPrograms{
        "firefox", "chrome", "google-chrome", "chromium"};
    return kPrograms.count(programLower) != 0;
}

// Auto-capitalize: a sentence-ending mark that should trigger capitalizing
// the next sentence once the user presses Enter.
bool isSentenceEndChar(char c) {
    return c == '.' || c == '?' || c == '!';
}

std::uint32_t uppercaseAscii(std::uint32_t sym) {
    if (sym >= 'a' && sym <= 'z') {
        return sym - ('a' - 'A');
    }
    return sym;
}

// Default AI system prompt, used when AI_SYSTEM_PROMPT is not set.
const char kDefaultAiSystemPrompt[] =
    "Bạn viết tiếng Việt tự nhiên như một người thật, không phải máy. "
    "Trả lời NGẮN GỌN, ĐỦ Ý, đi thẳng vào việc; dùng từ đời thường, giọng gần "
    "gũi, tránh sáo rỗng và lối văn máy móc kiểu AI. Nếu có "
    "<SKILL>...</SKILL>, đó là hướng dẫn cách thực hiện yêu cầu — hãy làm theo. "
    "Nếu có <context>...</context>, hãy dựa vào đó để nắm chủ đề, cách xưng hô "
    "và mức độ trang trọng rồi trả lời cho khớp ngữ cảnh. Chỉ xuất ra đúng nội "
    "dung cần dùng — không lời dẫn, tiêu đề, giải thích hay dấu ngoặc/trích dẫn "
    "thừa. Mặc định tiếng Việt; chỉ đổi ngôn ngữ khi được yêu cầu rõ.";

// Return the env var value if set and non-empty, else the fallback.
std::string envOr(const char *name, const char *fallback) {
    const char *v = std::getenv(name);
    return (v && v[0] != '\0') ? std::string(v) : std::string(fallback);
}

// Expand a leading ~ (or ~/) to $HOME.
std::string expandUser(std::string p) {
    if (!p.empty() && p[0] == '~' && (p.size() == 1 || p[1] == '/')) {
        if (const char *home = std::getenv("HOME"); home && home[0] != '\0') {
            p = std::string(home) + p.substr(1);
        }
    }
    return p;
}

// Shrink `s` to at most `cap` bytes without splitting a UTF-8 sequence: back up
// over any trailing continuation bytes (10xxxxxx) so the result always ends on a
// complete character. A raw resize(cap) could leave a dangling partial byte that
// renders garbled in the preedit and produces invalid UTF-8 in the JSON request.
void truncateUtf8(std::string &s, std::size_t cap) {
    if (s.size() <= cap) {
        return;
    }
    std::size_t len = cap;
    while (len > 0 && (static_cast<unsigned char>(s[len]) & 0xC0) == 0x80) {
        --len;
    }
    s.resize(len);
}

// Read a text file, capped at `cap` bytes. Returns empty on failure.
std::string readTextFileCapped(const std::string &path, std::size_t cap = 16000) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return {};
    }
    std::string out;
    char buf[4096];
    while (f && out.size() <= cap) {
        f.read(buf, sizeof(buf));
        out.append(buf, static_cast<std::size_t>(f.gcount()));
    }
    truncateUtf8(out, cap);
    return out;
}

// If `raw` begins with "/<name>", where <name> is a safe skill id
// ([A-Za-z0-9_-]), split it into the skill name and the remaining text (after
// the token and following whitespace). Returns false when there is no token.
bool parseSkillToken(const std::string &raw, std::string &skillName,
                     std::string &rest) {
    if (raw.empty() || raw[0] != '/') {
        return false;
    }
    std::size_t i = 1;
    while (i < raw.size() && raw[i] != ' ' && raw[i] != '\t') {
        ++i;
    }
    std::string name = raw.substr(1, i - 1);
    if (name.empty()) {
        return false;
    }
    for (char c : name) {
        if (!(std::isalnum(static_cast<unsigned char>(c)) || c == '-' ||
              c == '_')) {
            return false;  // reject anything path-unsafe (e.g. '.', '/')
        }
    }
    while (i < raw.size() && (raw[i] == ' ' || raw[i] == '\t')) {
        ++i;
    }
    skillName = std::move(name);
    rest = raw.substr(i);
    return true;
}

// Read the system clipboard (CLIPBOARD selection) via the available CLI tool.
// Returns empty on failure. Capped so a huge clipboard can't blow up the prompt.
//
// MUST be called from a worker thread, never from the fcitx key-event handler:
// the tool performs a synchronous X/Wayland selection round-trip, and the
// selection owner is often the very application that is blocked waiting for the
// IME to return — reading it inline deadlocks the whole session. `timeout`
// bounds the call so a stuck selection owner can never hang us indefinitely.
std::string readClipboard() {
    const char *cmd;
    if (const char *wl = std::getenv("WAYLAND_DISPLAY"); wl && wl[0] != '\0') {
        cmd = "timeout 3 wl-paste --no-newline 2>/dev/null";
    } else {
        cmd = "timeout 3 xclip -selection clipboard -o 2>/dev/null";
    }
    FILE *pipe = popen(cmd, "r");
    if (!pipe) {
        return {};
    }
    std::string out;
    char buf[4096];
    std::size_t n;
    while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0) {
        out.append(buf, n);
        if (out.size() > 8000) {
            break;  // cap context size
        }
    }
    pclose(pipe);
    truncateUtf8(out, 8000);
    return out;
}

} // namespace

// Ghi nhan ứng dụng mới vào file config
TelebitFcitx5Engine::TelebitFcitx5Engine(AddonManager *manager) {
    if (manager && manager->instance()) {
        instance_ = manager->instance();
        icManager_ = &instance_->inputContextManager();
        icManager_->registerProperty(statePropertyName, &stateFactory_);
        aiDispatcher_ = std::make_shared<EventDispatcher>();
        aiDispatcher_->attach(&instance_->eventLoop());
        focusInWatcher_ = instance_->watchEvent(
            EventType::InputContextFocusIn, EventWatcherPhase::Default,
            [this](Event &event) {
                auto &ice = static_cast<InputContextEvent &>(event);
                if (auto *ic = ice.inputContext()) {
                    recordSeenProgram(ic->program());
                }
            });
    }
    reloadConfig();
}

TelebitFcitx5Engine::~TelebitFcitx5Engine() {
    // Any AI worker thread still running will post its result via aiDispatcher_;
    // flip the alive flag so that scheduled callback becomes a no-op.
    aiAlive_->store(false);
    saveConfigIfDirty();
}

void TelebitFcitx5Engine::normalizeForcePreeditApps() {
    auto *list = config_.forcePreeditApps.mutableValue();
    if (!list) {
        return;
    }

    std::unordered_set<std::string> seen;
    std::vector<fcitx::TelebitForcePreeditAppConfig> out;
    out.reserve(list->size());

    for (auto &rule : *list) {
        std::string program = toLowerAscii(rule.program.value());
        if (program.empty()) {
            continue;
        }
        if (!seen.insert(program).second) {
            continue;
        }
        *rule.program.mutableValue() = program;
        out.push_back(std::move(rule));
    }

    *list = std::move(out);
}

void TelebitFcitx5Engine::rebuildMacroIndex() {
    auto macros = std::make_shared<EngineVietCpp::MacroTable>();
    for (const auto &rule : config_.macros.value()) {
        const std::string abbrev = toLowerAscii(rule.abbrev.value());
        const std::string &expansion = rule.expansion.value();
        if (abbrev.empty() || expansion.empty()) {
            continue;
        }
        (*macros)[abbrev] = expansion;
    }
    macrosLower_ = std::move(macros);
}

void TelebitFcitx5Engine::rebuildSeenProgramsIndex() {
    seenProgramsLower_.clear();
    forcePreeditEnabledLower_.clear();
    for (const auto &rule : config_.forcePreeditApps.value()) {
        const auto &p = rule.program.value();
        if (p.empty()) {
            continue;
        }
        seenProgramsLower_.insert(p);
        if (rule.enabled.value()) {
            forcePreeditEnabledLower_.insert(p);
        }
    }
}

void TelebitFcitx5Engine::recordSeenProgram(const std::string &program) {
    if (program.empty()) {
        return;
    }

    const std::string programLower = toLowerAscii(program);
    if (programLower.empty()) {
        return;
    }
    if (seenProgramsLower_.find(programLower) != seenProgramsLower_.end()) {
        return;
    }

    auto *list = config_.forcePreeditApps.mutableValue();
    if (!list) {
        return;
    }

    // Auto-discovered apps are unchecked by default, except the known
    // preedit-only programs (browsers), which must be on from the first focus.
    const bool enabled = isDefaultPreeditProgram(programLower);

    fcitx::TelebitForcePreeditAppConfig app;
    *app.program.mutableValue() = programLower;
    *app.enabled.mutableValue() = enabled;
    list->push_back(std::move(app));

    seenProgramsLower_.insert(programLower);
    if (enabled) {
        // Mirror it into the lookup set too, so it takes effect for this very
        // focus instead of only after the next config reload.
        forcePreeditEnabledLower_.insert(programLower);
    }
    configDirty_ = true;
}

void TelebitFcitx5Engine::saveConfigIfDirty() {
    if (!configDirty_) {
        return;
    }
    safeSaveAsIni(config_, configFile);
    configDirty_ = false;
}

TelebitInputState *TelebitFcitx5Engine::stateFor(InputContext *ic) const {
    if (!ic) {
        return nullptr;
    }
    return ic->propertyFor(&stateFactory_);
}

void TelebitFcitx5Engine::resetAllInputStates() {
    if (!icManager_) {
        return;
    }
    icManager_->foreach([this](InputContext *ic) {
        if (auto *state = stateFor(ic)) {
            state->resetAll();
            ic->inputPanel().setClientPreedit(Text());
            ic->updatePreedit();
        }
        return true;
    });
}

void TelebitFcitx5Engine::reloadConfig() {
    readAsIni(config_, configFile);
    normalizeForcePreeditApps();
    rebuildSeenProgramsIndex();
    rebuildMacroIndex();
    configDirty_ = false;
    resetAllInputStates();
}

const Configuration *TelebitFcitx5Engine::getConfig() const {
    const_cast<TelebitFcitx5Engine *>(this)->saveConfigIfDirty();
    return &config_;
}

void TelebitFcitx5Engine::setConfig(const RawConfig &config) {
    config_.load(config, true);
    normalizeForcePreeditApps();
    safeSaveAsIni(config_, configFile);
    rebuildSeenProgramsIndex();
    rebuildMacroIndex();
    configDirty_ = false;
    resetAllInputStates();
}

TelexOptions TelebitFcitx5Engine::currentOptions() const {
    TelexOptions opts;
    opts.spellCheckRestore = config_.spellCheckRestore.value();
    opts.vniMode = config_.vniMode.value();
    opts.modernTone = config_.modernToneStyle.value();
    return opts;
}

// Commit whatever is pending (used when Vietnamese typing gets toggled off).
void TelebitFcitx5Engine::flushPending(InputContext *ic, TelebitInputState *state) {
    if (!state->engine.buffer().empty()) {
        ic->commitString(convert_buffer(state->engine.buffer(), currentOptions()));
    }
    state->resetAll();
    ic->inputPanel().setClientPreedit(Text());
    ic->updatePreedit();
}

void TelebitFcitx5Engine::updatePreedit(InputContext *ic, TelebitInputState *state) {
    if (!ic || !state) {
        return;
    }

    const std::string &buffer = state->engine.buffer();
    if (buffer.empty()) {
        ic->inputPanel().setClientPreedit(Text());
        ic->updatePreedit();
        return;
    }

    std::string preeditStr = convert_buffer(buffer, currentOptions());
    Text preedit;
    preedit.append(preeditStr, TextFormatFlag::Underline);
    // Place cursor at the end of the preedit text (by byte).
    preedit.setCursor(static_cast<int>(preeditStr.size()));
    ic->inputPanel().setClientPreedit(preedit);
    ic->updatePreedit();
}

void TelebitFcitx5Engine::keyEvent(const InputMethodEntry &entry, KeyEvent &keyEvent) {
    FCITX_UNUSED(entry);

    auto *ic = keyEvent.inputContext();
    auto *state = stateFor(ic);
    if (!ic || !state) {
        return;
    }

    if (!keyEvent.isRelease()) {
        const Key &key = keyEvent.key();
        // Temporarily toggle Vietnamese typing on/off (like Unikey's EN/VI switch).
        if (key.checkKeyList(config_.toggleVietnameseKey.value())) {
            if (vietnameseEnabled_) {
                flushPending(ic, state);
            }
            vietnameseEnabled_ = !vietnameseEnabled_;
            keyEvent.filterAndAccept();
            return;
        }

        // AI trigger key (configurable; default Ctrl+Shift+Space) toggles the
        // request box:
        //  - closed → open it
        //  - open   → always EXIT (even with a half-typed prompt or an in-flight
        //             request); sending is done only with Enter.
        if (config_.aiEnabled.value() &&
            key.checkKeyList(config_.aiTriggerKey.value())) {
            if (!state->aiPromptMode) {
                enterAiPromptMode(ic, state);
            } else {
                TELEBIT_AI_LOG("trigger-key exit");
                stopAiSpinner();
                state->clearAi();
                updateAiPreedit(ic, state);
            }
            keyEvent.filterAndAccept();
            return;
        }
    }

    // While the AI request box is open, all keys feed the prompt (or the
    // spinner) and never reach the normal Vietnamese pipeline.
    if (state->aiPromptMode) {
        keyEventAiPrompt(ic, state, keyEvent);
        return;
    }

    if (!vietnameseEnabled_) {
        return;
    }

    state->engine.setOptions(currentOptions());
    state->engine.setMacros(macrosLower_);

    bool useDirectRollback = config_.directCommitRollback.value();

    const std::string &program = ic->program();
    if (useDirectRollback) {
        const std::string programLower = toLowerAscii(program);
        if (!programLower.empty() &&
            forcePreeditEnabledLower_.count(programLower)) {
            useDirectRollback = false;
        }
    }

    if (useDirectRollback) {
        keyEventDirectRollback(ic, state, keyEvent);
    } else {
        keyEventPreedit(ic, state, keyEvent);
    }
}

void TelebitFcitx5Engine::keyEventPreedit(InputContext *ic, TelebitInputState *state,
                                          KeyEvent &keyEvent) {
    if (keyEvent.isRelease()) {
        return;
    }

    // If user used direct rollback mode before, ensure we don't keep stale state.
    state->clearRollback();

    const Key &key = keyEvent.key();
    std::uint32_t modState = 0;
    if (key.states().test(KeyState::Ctrl)) {
        modState |= MOD_CONTROL;
    }
    if (key.states().test(KeyState::Alt)) {
        modState |= MOD_ALT;
    }

    std::uint32_t keyval = static_cast<std::uint32_t>(key.sym());
    std::uint32_t keycode = static_cast<std::uint32_t>(key.code());

    const bool autoCapitalize = config_.autoCapitalizeSentence.value();
    if (autoCapitalize && state->capitalizeNextLetter &&
        state->engine.buffer().empty()) {
        if (keyval >= 'a' && keyval <= 'z') {
            keyval = uppercaseAscii(keyval);
        }
        if ((keyval >= 'a' && keyval <= 'z') || (keyval >= 'A' && keyval <= 'Z')) {
            state->capitalizeNextLetter = false;
        }
    }

    KeyResult result = state->engine.process_key_event(keyval, keycode, modState);

    if (result.handled) {
        keyEvent.filterAndAccept();
        if (!result.commit_text.empty()) {
            ic->commitString(result.commit_text);
            state->lastDispatchedChar = result.commit_text.back();
        }
        updatePreedit(ic, state);
    } else {
        if (!result.commit_text.empty()) {
            ic->commitString(result.commit_text);
            state->lastDispatchedChar = result.commit_text.back();
            updatePreedit(ic, state);
        }
        // modState is Ctrl/Alt only here (see above); a plain key that
        // wasn't handled by the engine is about to be forwarded verbatim.
        if (autoCapitalize && modState == 0) {
            const bool isSentenceBoundaryKey =
                (keyval == KEYVAL_RETURN || keyval == KEYVAL_SPACE);
            if (isSentenceBoundaryKey && isSentenceEndChar(state->lastDispatchedChar)) {
                state->capitalizeNextLetter = true;
            }
            if (keyval >= 0x20 && keyval <= 0x7e) {
                state->lastDispatchedChar = static_cast<char>(keyval);
            }
        }
    }
}

void TelebitFcitx5Engine::keyEventDirectRollback(InputContext *ic,
                                                 TelebitInputState *state,
                                                 KeyEvent &keyEvent) {
    if (keyEvent.isRelease()) {
        return;
    }

    // If client does not support surrounding text, fall back to preedit mode.
    if (!ic->capabilityFlags().test(CapabilityFlag::SurroundingText)) {
        state->clearRollback();
        keyEventPreedit(ic, state, keyEvent);
        return;
    }

    const Key &key = keyEvent.key();
    // Do not interfere with shortcuts.
    if (key.states().test(KeyState::Ctrl) || key.states().test(KeyState::Alt)) {
        state->clearRollback();
        return;
    }

    auto sym = key.sym();
    const bool autoCapitalize = config_.autoCapitalizeSentence.value();

    const TelexOptions opts = currentOptions();

    // Backspace: update our internal buffers and rewrite the current word.
    if (sym == FcitxKey_BackSpace) {
        if (!state->rollbackRawAscii.empty()) {
            state->rollbackRawAscii.pop_back();
            std::string newDisplay = telex_to_unicode(state->rollbackRawAscii, opts);

            std::size_t prefixLen =
                commonPrefixBytes(state->rollbackDisplay, newDisplay);
            int charsToDelete =
                utf8CharCount(state->rollbackDisplay.substr(prefixLen));
            if (charsToDelete > 0) {
                ic->deleteSurroundingText(-charsToDelete,
                                          static_cast<unsigned int>(charsToDelete));
            }
            if (newDisplay.size() > prefixLen) {
                ic->commitString(newDisplay.substr(prefixLen));
            }

            state->rollbackDisplay = newDisplay;
            keyEvent.filterAndAccept();
            return;
        }
        // Nothing tracked: let app handle backspace.
        state->clearRollback();
        return;
    }

    // In VNI mode digits modify the current word (leading digits stay literal).
    const bool vniDigit = opts.vniMode && '0' <= sym && sym <= '9' &&
                          !state->rollbackRawAscii.empty();

    // Word boundary: space / enter / tab / punctuation / digits.
    // Expand macros, clear state and allow the key to reach the application.
    if (!(('a' <= sym && sym <= 'z') || ('A' <= sym && sym <= 'Z') || vniDigit)) {
        if (!state->rollbackRawAscii.empty() && macrosLower_) {
            auto it = macrosLower_->find(toLowerAscii(state->rollbackRawAscii));
            if (it != macrosLower_->end()) {
                int chars = utf8CharCount(state->rollbackDisplay);
                if (chars > 0) {
                    ic->deleteSurroundingText(-chars,
                                              static_cast<unsigned int>(chars));
                }
                ic->commitString(it->second);
                if (autoCapitalize && !it->second.empty()) {
                    state->lastDispatchedChar = it->second.back();
                }
            }
        }
        state->clearRollback();

        // sym is still forwarded to the application unchanged after this
        // (we never call filterAndAccept in this branch), even when a
        // macro just committed its expansion — so sym always becomes the
        // real last-dispatched character for the next auto-capitalize check.
        if (autoCapitalize) {
            const bool isSentenceBoundaryKey =
                (sym == FcitxKey_Return || sym == FcitxKey_space);
            if (isSentenceBoundaryKey && isSentenceEndChar(state->lastDispatchedChar)) {
                state->capitalizeNextLetter = true;
            }
            if (sym >= 0x20 && sym <= 0x7e) {
                state->lastDispatchedChar = static_cast<char>(sym);
            }
        }
        return;
    }

    // Letter key: rewrite the word in-place.
    if (autoCapitalize && state->capitalizeNextLetter &&
        state->rollbackRawAscii.empty()) {
        if (sym >= 'a' && sym <= 'z') {
            sym = static_cast<fcitx::KeySym>(uppercaseAscii(static_cast<std::uint32_t>(sym)));
        }
        if ((sym >= 'a' && sym <= 'z') || (sym >= 'A' && sym <= 'Z')) {
            state->capitalizeNextLetter = false;
        }
    }

    state->rollbackRawAscii.push_back(static_cast<char>(sym));
    std::string newDisplay = telex_to_unicode(state->rollbackRawAscii, opts);

    std::size_t prefixLen = commonPrefixBytes(state->rollbackDisplay, newDisplay);
    int charsToDelete = utf8CharCount(state->rollbackDisplay.substr(prefixLen));
    if (charsToDelete > 0) {
        ic->deleteSurroundingText(-charsToDelete,
                                  static_cast<unsigned int>(charsToDelete));
    }
    if (newDisplay.size() > prefixLen) {
        ic->commitString(newDisplay.substr(prefixLen));
    }
    state->rollbackDisplay = newDisplay;
    if (!newDisplay.empty()) {
        state->lastDispatchedChar = newDisplay.back();
    }

    // We commit ourselves, so do not forward the original key.
    keyEvent.filterAndAccept();
}

void TelebitFcitx5Engine::enterAiPromptMode(InputContext *ic,
                                            TelebitInputState *state) {
    // Commit anything the user was mid-typing, then open an empty request box.
    // flushPending() calls resetAll(), which also clears any prior AI state.
    flushPending(ic, state);
    state->aiPromptMode = true;
    state->aiBusy = false;
    state->aiRawAscii.clear();
    updateAiPreedit(ic, state);
}

void TelebitFcitx5Engine::updateAiPreedit(InputContext *ic,
                                          TelebitInputState *state) {
    if (!ic || !state) {
        return;
    }

    // AuxUp lives in fcitx's own popup (classicui), which many apps that use
    // on-the-spot preedit never display. So everything the user must see —
    // marker, context indicator, prompt — goes into the client preedit inline.
    ic->inputPanel().setAuxUp(Text());

    if (!state->aiPromptMode) {
        ic->inputPanel().setClientPreedit(Text());
        ic->updatePreedit();
        return;
    }

    // Marker: [AI] while typing; while a request is in flight the ellipsis is
    // animated (1→2→3 dots) via aiSpinnerPhase_, driven by the spinner timer.
    // ASCII so it renders even in apps without an emoji font.
    std::string preeditStr = state->aiBusy
                                 ? kAiSpinnerFrames[aiSpinnerPhase_ %
                                                    kAiSpinnerFrameCount]
                                 : "[AI] ";

    // Inline context indicator so the user can see the clipboard was attached.
    if (state->aiContextLoading) {
        preeditStr += "(đang lấy clipboard…) ";
    } else if (!state->aiContext.empty()) {
        preeditStr += "(ngữ cảnh " +
                      std::to_string(utf8CharCount(state->aiContext)) +
                      " ký tự) ";
    }

    // The request itself, converted from raw Telex to readable Vietnamese.
    preeditStr += telex_to_unicode(state->aiRawAscii, currentOptions());

    Text preedit;
    preedit.append(preeditStr, TextFormatFlag::Underline);
    preedit.setCursor(static_cast<int>(preeditStr.size()));
    ic->inputPanel().setClientPreedit(preedit);
    ic->updatePreedit();
} 

void TelebitFcitx5Engine::keyEventAiPrompt(InputContext *ic,
                                           TelebitInputState *state,
                                           KeyEvent &keyEvent) {
    if (keyEvent.isRelease()) {
        keyEvent.filterAndAccept();
        return;
    }

    const Key &key = keyEvent.key();
    auto sym = key.sym();

    // Esc closes the box and cancels any request in flight.
    if (sym == FcitxKey_Escape) {
        stopAiSpinner();
        state->clearAi();
        updateAiPreedit(ic, state);
        keyEvent.filterAndAccept();
        return;
    }

    // While a request is in flight, swallow everything except Esc.
    if (state->aiBusy) {
        keyEvent.filterAndAccept();
        return;
    }

    // Enter sends the prompt (needs either a typed instruction or pasted context).
    if (sym == FcitxKey_Return || sym == FcitxKey_KP_Enter) {
        TELEBIT_AI_LOG("Enter pressed, raw=\"%s\" (len=%zu) ctxLen=%zu",
                       state->aiRawAscii.c_str(), state->aiRawAscii.size(),
                       state->aiContext.size());
        // If the clipboard is still being fetched, defer the send: remember the
        // intent and let loadClipboardAsync's completion fire the request, so
        // the pasted context is included without a second Enter press.
        if (state->aiContextLoading) {
            state->aiPendingSend = true;
            keyEvent.filterAndAccept();
            return;
        }
        if (!state->aiRawAscii.empty() || !state->aiContext.empty()) {
            dispatchAiRequest(ic, state);
        }
        keyEvent.filterAndAccept();
        return;
    }

    // Ctrl+V: pull the system clipboard in as context — asynchronously, so the
    // key handler returns immediately and the application unblocks (a synchronous
    // clipboard read here deadlocks the session).
    if (key.states().test(KeyState::Ctrl) &&
        (sym == FcitxKey_v || sym == FcitxKey_V)) {
        if (!state->aiContextLoading) {
            loadClipboardAsync(ic, state);
        }
        keyEvent.filterAndAccept();
        return;
    }

    if (sym == FcitxKey_BackSpace) {
        if (!state->aiRawAscii.empty()) {
            state->aiRawAscii.pop_back();
        }
        updateAiPreedit(ic, state);
        keyEvent.filterAndAccept();
        return;
    }

    // Don't feed other shortcut chords into the prompt; just swallow them.
    if (key.states().test(KeyState::Ctrl) || key.states().test(KeyState::Alt)) {
        keyEvent.filterAndAccept();
        return;
    }

    // Printable ASCII (space included) accumulates as raw Telex.
    if (sym >= 0x20 && sym <= 0x7e) {
        state->aiRawAscii.push_back(static_cast<char>(sym));
        updateAiPreedit(ic, state);
        keyEvent.filterAndAccept();
        return;
    }

    // Anything else (arrows, function keys) is swallowed to keep the box modal.
    keyEvent.filterAndAccept();
}

void TelebitFcitx5Engine::loadClipboardAsync(InputContext *ic,
                                             TelebitInputState *state) {
    state->aiContextLoading = true;
    const std::uint64_t seq = state->aiRequestSeq;
    updateAiPreedit(ic, state);

    auto alive = aiAlive_;
    auto dispatcher = aiDispatcher_;
    auto icRef = ic->watch();
    TelebitFcitx5Engine *self = this;

    // Clipboard read blocks on an X/Wayland round-trip — do it off the event
    // loop so the key handler returns and the application unblocks.
    std::thread([self, alive, dispatcher, icRef, seq]() mutable {
        std::string clip = readClipboard();
        TELEBIT_AI_LOG("clipboard worker read: %zu bytes", clip.size());
        if (!dispatcher) {
            return;
        }
        dispatcher->schedule([self, alive, icRef, seq, clip]() mutable {
            if (!alive->load()) {
                return;
            }
            InputContext *ic = icRef.get();
            if (!ic) {
                return;
            }
            TelebitInputState *st = self->stateFor(ic);
            if (!st) {
                return;
            }
            // Ignore if the box was closed/reopened while we were fetching.
            if (!st->aiPromptMode || st->aiRequestSeq != seq) {
                return;
            }
            st->aiContextLoading = false;
            if (!clip.empty()) {
                st->aiContext = std::move(clip);
            }
            // Enter was pressed while we were fetching: dispatch now that the
            // context is in place, so the user need not press Enter again.
            if (st->aiPendingSend) {
                st->aiPendingSend = false;
                if (!st->aiRawAscii.empty() || !st->aiContext.empty()) {
                    self->dispatchAiRequest(ic, st);
                    return;
                }
            }
            self->updateAiPreedit(ic, st);
        });
    }).detach();
}

void TelebitFcitx5Engine::dispatchAiRequest(InputContext *ic,
                                            TelebitInputState *state) {
    // All AI settings come from environment variables (nothing is stored in
    // the fcitx config). Sensible defaults apply when a var is unset.
    telebit::ai::AIConfig cfg;
    cfg.endpoint = envOr("AI_ENDPOINT", "https://api.openai.com/v1/chat/completions");
    cfg.model = envOr("AI_MODEL", "gpt-4.1-mini");
    if (const char *k = std::getenv("AI_API_KEY")) {
        cfg.apiKey = k;
    }
    cfg.systemPrompt = envOr("AI_SYSTEM_PROMPT", kDefaultAiSystemPrompt);
    if (const char *mt = std::getenv("AI_MAX_TOKENS"); mt && mt[0] != '\0') {
        int parsed = std::atoi(mt);
        cfg.maxTokens = parsed > 0 ? parsed : 4096;
    } else {
        cfg.maxTokens = 4096;
    }
    // Lower temperature keeps small local models obedient (they otherwise echo
    // the prompt scaffolding). Configurable via AI_TEMPERATURE; default 0.3.
    if (const char *t = std::getenv("AI_TEMPERATURE"); t && t[0] != '\0') {
        char *end = nullptr;
        double parsed = std::strtod(t, &end);
        if (end != t && parsed >= 0.0) {
            cfg.temperature = parsed;
        }
    }

    // Skill: if the prompt starts with /<name> and a skills folder is set, load
    // <folder>/<name>.md and pass it as <SKILL>...</SKILL>; the rest is the task.
    std::string instructionRaw = state->aiRawAscii;
    std::string skillContent;
    if (const std::string skillsDir = config_.aiSkillsDir.value();
        !skillsDir.empty()) {
        std::string skillName, rest;
        if (parseSkillToken(state->aiRawAscii, skillName, rest)) {
            std::string path = expandUser(skillsDir);
            if (!path.empty() && path.back() == '/') {
                path.pop_back();
            }
            path += "/" + skillName + ".md";
            std::string content = readTextFileCapped(path);
            if (!content.empty()) {
                skillContent = std::move(content);
                instructionRaw = rest;  // strip the /name token
                TELEBIT_AI_LOG("skill loaded: %s (%zu bytes)",
                               skillName.c_str(), skillContent.size());
            } else {
                TELEBIT_AI_LOG("skill not found: %s", skillName.c_str());
            }
        }
    }

    std::string instruction = telex_to_unicode(instructionRaw, currentOptions());

    // Skill content = HOW to do it → put it in the SYSTEM role, not the user
    // message. Separating the instruction (system) from the material/request
    // (user) stops small models from treating the skill as text to echo back.
    if (!skillContent.empty()) {
        cfg.systemPrompt +=
            "\n\n# HƯỚNG DẪN (SKILL) áp dụng cho yêu cầu này\n"
            "Hãy làm theo hướng dẫn dưới đây. TUYỆT ĐỐI không in lại, không "
            "nhắc tên hay nội dung phần hướng dẫn này trong kết quả:\n" +
            skillContent;
    }

    // User message: <context> (material) → the request. No skill scaffolding
    // here anymore — only what the model should act on.
    std::string prompt;
    if (!state->aiContext.empty()) {
        prompt += "<context>\n" + state->aiContext + "\n</context>\n\n";
    }
    if (!instruction.empty()) {
        prompt += "Yêu cầu: " + instruction;
    } else if (!skillContent.empty() || !state->aiContext.empty()) {
        prompt += "Xử lý nội dung trên theo yêu cầu hợp lý nhất.";
    }
    // Hard rule as the LAST line — small models weight the final instruction
    // heavily, so this is where "output only the result" lands best.
    prompt +=
        "\n\n(CHỈ trả về văn bản kết quả cuối cùng. KHÔNG lặp lại đề bài, KHÔNG "
        "in chữ SKILL / context / Yêu cầu, không thêm lời dẫn hay giải thích.)";

    state->aiBusy = true;
    const std::uint64_t seq = state->aiRequestSeq;
    updateAiPreedit(ic, state);
    startAiSpinner(ic);  // animate the [AI...] ellipsis while we wait

    TELEBIT_AI_LOG("dispatch: prompt=\"%s\" endpoint=%s model=%s keyLen=%zu seq=%llu",
                   prompt.c_str(), cfg.endpoint.c_str(), cfg.model.c_str(),
                   cfg.apiKey.size(), (unsigned long long)seq);

    auto alive = aiAlive_;
    auto dispatcher = aiDispatcher_;
    auto icRef = ic->watch();
    TelebitFcitx5Engine *self = this;

    // Blocking network call runs on a detached worker; the result is posted
    // back onto the fcitx event loop, where it is safe to touch the IC.
    std::thread([self, alive, dispatcher, icRef, seq, cfg, prompt]() mutable {
        TELEBIT_AI_LOG("worker: calling ai_generate...");
        telebit::ai::AIResult result = telebit::ai::ai_generate(cfg, prompt);
        TELEBIT_AI_LOG("worker: ai_generate returned ok=%d err=\"%s\" textLen=%zu",
                       result.ok ? 1 : 0, result.error.c_str(), result.text.size());
        if (!dispatcher) {
            return;
        }
        dispatcher->schedule([self, alive, icRef, seq, result]() mutable {
            TELEBIT_AI_LOG("callback: on event loop");
            if (!alive->load()) {
                return;  // engine destroyed — do not touch it
            }
            InputContext *ic = icRef.get();
            if (!ic) {
                TELEBIT_AI_LOG("callback: ic gone");
                return;  // input context gone
            }
            TelebitInputState *st = self->stateFor(ic);
            if (!st) {
                return;
            }
            // Drop the result if the box was closed or superseded meanwhile.
            if (!st->aiPromptMode || st->aiRequestSeq != seq) {
                TELEBIT_AI_LOG("callback: dropped (promptMode=%d seq=%llu want=%llu)",
                               st->aiPromptMode ? 1 : 0,
                               (unsigned long long)st->aiRequestSeq,
                               (unsigned long long)seq);
                return;
            }

            self->stopAiSpinner();  // request done — stop the animation
            if (result.ok) {
                TELEBIT_AI_LOG("callback: committing %zu bytes", result.text.size());
                st->clearAi();
                self->updateAiPreedit(ic, st);  // clears the box
                ic->commitString(result.text);
                if (!result.text.empty()) {
                    st->lastDispatchedChar = result.text.back();
                }
            } else {
                // Keep the box open so the user can edit and retry. Show the
                // error inline in the client preedit (AuxUp popup often hidden).
                st->aiBusy = false;
                std::string pe = "[AI lỗi: " + result.error + "] " +
                                 telex_to_unicode(st->aiRawAscii,
                                                  self->currentOptions());
                Text t;
                t.append(pe, TextFormatFlag::Underline);
                t.setCursor(static_cast<int>(pe.size()));
                ic->inputPanel().setAuxUp(Text());
                ic->inputPanel().setClientPreedit(t);
                ic->updatePreedit();
            }
        });
    }).detach();
}

void TelebitFcitx5Engine::startAiSpinner(InputContext *ic) {
    if (!instance_ || !ic) {
        return;
    }
    aiSpinnerIc_ = ic->watch();
    aiSpinnerPhase_ = 0;
    // Recurring one-shot timer: on each tick it advances the ellipsis and
    // re-arms itself. It runs on the event-loop thread, so touching the IC is
    // safe. It self-stops (does not re-arm) once the context is no longer busy.
    aiSpinnerTimer_ = instance_->eventLoop().addTimeEvent(
        CLOCK_MONOTONIC, now(CLOCK_MONOTONIC) + kAiSpinnerIntervalUs, 0,
        [this](EventSourceTime *source, uint64_t) {
            InputContext *ic = aiSpinnerIc_.get();
            TelebitInputState *st = ic ? stateFor(ic) : nullptr;
            if (!st || !st->aiBusy) {
                return true;  // stop: leave the one-shot disabled
            }
            aiSpinnerPhase_ = (aiSpinnerPhase_ + 1) % kAiSpinnerFrameCount;
            updateAiPreedit(ic, st);
            source->setNextInterval(kAiSpinnerIntervalUs);
            source->setOneShot();
            return true;
        });
}

void TelebitFcitx5Engine::stopAiSpinner() {
    aiSpinnerTimer_.reset();
    aiSpinnerIc_.unwatch();
    aiSpinnerPhase_ = 0;
}

void TelebitFcitx5Engine::reset(const InputMethodEntry &entry, InputContextEvent &event) {
    FCITX_UNUSED(entry);
    auto *ic = event.inputContext();
    if (auto *state = stateFor(ic)) {
        state->resetAll();
    }
    if (ic) {
        ic->inputPanel().setClientPreedit(Text());
        ic->updatePreedit();
    }
}

FCITX_ADDON_FACTORY(TelebitFcitx5EngineFactory);
