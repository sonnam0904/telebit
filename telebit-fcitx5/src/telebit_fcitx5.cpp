/*
 * telebit-fcitx5 - Vietnamese Telex input method for Fcitx5
 * Engine implementation that reuses EngineVietCpp from the core C++ logic.
 */

#include "telebit_fcitx5.h"
#include "vietnamese.h"

#include <fcitx-config/iniparser.h>
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

} // namespace

// Ghi nhan ứng dụng mới vào file config
TelebitFcitx5Engine::TelebitFcitx5Engine(AddonManager *manager) {
    if (manager && manager->instance()) {
        instance_ = manager->instance();
        icManager_ = &instance_->inputContextManager();
        icManager_->registerProperty(statePropertyName, &stateFactory_);
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

    fcitx::TelebitForcePreeditAppConfig app;
    *app.program.mutableValue() = programLower;
    // Auto-discovered apps should be unchecked by default.
    *app.enabled.mutableValue() = false;
    list->push_back(std::move(app));

    seenProgramsLower_.insert(programLower);
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

    KeyResult result = state->engine.process_key_event(keyval, keycode, modState);

    if (result.handled) {
        keyEvent.filterAndAccept();
        if (!result.commit_text.empty()) {
            ic->commitString(result.commit_text);
        }
        updatePreedit(ic, state);
    } else {
        if (!result.commit_text.empty()) {
            ic->commitString(result.commit_text);
            updatePreedit(ic, state);
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
            }
        }
        state->clearRollback();
        return;
    }

    // Letter key: rewrite the word in-place.
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

    // We commit ourselves, so do not forward the original key.
    keyEvent.filterAndAccept();
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
