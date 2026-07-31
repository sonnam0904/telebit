#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <fcitx/addonfactory.h>
#include <fcitx-config/configuration.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <fcitx-utils/event.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/trackableobject.h>

#include "engine.h"

namespace fcitx {
class AddonManager;
class InputContextManager;
class Instance;
} // namespace fcitx

namespace fcitx {

FCITX_CONFIGURATION(
    TelebitForcePreeditAppConfig,
    fcitx::Option<std::string> program{
        this,
        "Program",
        "Tên ứng dụng, ví dụ: firefox / chromium / code",
        ""
    };
    fcitx::Option<bool> enabled{
        this,
        "Enabled",
        "Ép dùng preedit mode cho ứng dụng này",
        false // app thêm tay trong configtool
    };
);

FCITX_CONFIGURATION(
    TelebitMacroConfig,
    fcitx::Option<std::string> abbrev{
        this,
        "Abbrev",
        "Từ viết tắt (vd: vn)",
        ""
    };
    fcitx::Option<std::string> expansion{
        this,
        "Expansion",
        "Nội dung thay thế (vd: Việt Nam)",
        ""
    };
);

} // namespace fcitx

/// Per-input-context typing state (buffer, direct-rollback fields).
struct TelebitInputState : public fcitx::InputContextProperty {
    EngineVietCpp engine;
    std::string rollbackRawAscii;
    std::string rollbackDisplay;

    // Auto-capitalize: last plain-ASCII character actually sent to the
    // application, and whether the next word typed should start uppercase.
    char lastDispatchedChar = '\0';
    bool capitalizeNextLetter = false;

    // AI prompt mode: while active, keystrokes accumulate into a prompt shown
    // in the preedit (never committed to the app) until the user presses Enter
    // to send it to the LLM. `aiRawAscii` holds the raw Telex keystrokes so the
    // preedit can display the converted Vietnamese. `aiRequestSeq` tags each
    // in-flight request so a stale async result is ignored after cancel/reset.
    bool aiPromptMode = false;
    bool aiBusy = false;
    // True while the clipboard is being fetched on a worker thread.
    bool aiContextLoading = false;
    // Set when Enter is pressed while the clipboard is still loading; the
    // request is dispatched automatically once the fetch completes.
    bool aiPendingSend = false;
    std::string aiRawAscii;
    // Optional clipboard context pasted with Ctrl+V, sent alongside the prompt.
    std::string aiContext;
    std::uint64_t aiRequestSeq = 0;

    void clearRollback() {
        rollbackRawAscii.clear();
        rollbackDisplay.clear();
    }

    void clearAi() {
        aiPromptMode = false;
        aiBusy = false;
        aiContextLoading = false;
        aiPendingSend = false;
        aiRawAscii.clear();
        aiContext.clear();
        ++aiRequestSeq;  // invalidate any in-flight response
    }

    void resetAll() {
        engine.reset();
        clearRollback();
        clearAi();
        lastDispatchedChar = '\0';
        capitalizeNextLetter = false;
    }
};

class TelebitFcitx5Engine : public fcitx::InputMethodEngineV2 {
public:
    explicit TelebitFcitx5Engine(fcitx::AddonManager *manager);
    ~TelebitFcitx5Engine() override;

    void keyEvent(const fcitx::InputMethodEntry &entry,
                  fcitx::KeyEvent &keyEvent) override;

    void reset(const fcitx::InputMethodEntry &entry,
               fcitx::InputContextEvent &event) override;

    void reloadConfig() override;
    const fcitx::Configuration *getConfig() const override;
    void setConfig(const fcitx::RawConfig &config) override;

private:
    FCITX_CONFIGURATION(
        TelebitFcitx5Config,
        fcitx::Option<bool> directCommitRollback{
            this,
            "DirectCommitRollback",
            "Bật chế độ direct rollback (SurroundingText) nếu ứng dụng hỗ trợ",
            true
        };

        fcitx::Option<bool> spellCheckRestore{
            this,
            "SpellCheckRestore",
            "Kiểm tra chính tả: tự khôi phục phím gốc với từ không phải tiếng Việt",
            true
        };

        fcitx::Option<bool> vniMode{
            this,
            "VNIMode",
            "Dùng kiểu gõ VNI (a1=á, a6=â, u7=ư, d9=đ) thay vì Telex",
            false
        };

        fcitx::Option<bool> modernToneStyle{
            this,
            "ModernToneStyle",
            "Đặt dấu kiểu mới (hoà, khoẻ, thuý) thay vì kiểu cũ (hòa, khỏe, thúy)",
            false
        };

        fcitx::Option<bool> autoCapitalizeSentence{
            this,
            "AutoCapitalizeSentence",
            "Tự động viết hoa chữ đầu câu sau dấu . ? ! rồi dấu cách hoặc Enter",
            true
        };

        fcitx::KeyListOption toggleVietnameseKey{
            this,
            "ToggleVietnameseKey",
            "Phím bật/tắt gõ tiếng Việt tạm thời",
            {fcitx::Key("Control+Shift+z")},
            fcitx::KeyListConstrain()
        };

        // Trợ lý AI. Mọi tham số (API key, endpoint, model, system prompt,
        // max tokens) đều lấy từ BIẾN MÔI TRƯỜNG (AI_API_KEY, AI_ENDPOINT,
        // AI_MODEL, AI_SYSTEM_PROMPT, AI_MAX_TOKENS) — không lưu trong config
        // này để tránh rò rỉ key. Phím mở ô nhập cố định là Ctrl+Shift+Space.
        fcitx::Option<bool, fcitx::NoConstrain<bool>,
                      fcitx::DefaultMarshaller<bool>, fcitx::ToolTipAnnotation>
            aiEnabled{
                this,
                "AIEnabled",
                "Bật trợ lý AI: mở ô bằng Ctrl+Shift+Space, gõ yêu cầu rồi "
                "Enter để sinh văn bản \n Cách cấu hình: https://github.com/sonnam0904/telebit/blob/main/docs/ai-assistant.md",
                false,
                fcitx::NoConstrain<bool>(),
                fcitx::DefaultMarshaller<bool>(),
                fcitx::ToolTipAnnotation(
                    "Cấu hình qua biến môi trường: AI_API_KEY, AI_ENDPOINT, "
                    "AI_MODEL, AI_SYSTEM_PROMPT, AI_MAX_TOKENS. \n Hướng dẫn: "
                    "https://github.com/sonnam0904/telebit/blob/main/docs/ai-assistant.md")};

        fcitx::KeyListOption aiTriggerKey{
            this,
            "AITriggerKey",
            "Phím mở ô nhập yêu cầu AI (bấm lại để thoát; Enter để gửi)",
            {fcitx::Key("Control+Shift+space")},
            fcitx::KeyListConstrain()
        };

        fcitx::Option<std::string> aiSkillsDir{
            this,
            "AISkillsDir",
            "Thư mục chứa file skill (.md). Trong ô AI gõ /tên-skill ở đầu để "
            "nạp file tên-skill.md làm hướng dẫn (vd: /reply-email). Hỗ trợ ~.",
            ""
        };

        fcitx::Option<std::vector<fcitx::TelebitMacroConfig>,
                      fcitx::NoConstrain<std::vector<fcitx::TelebitMacroConfig>>,
                      fcitx::DefaultMarshaller<std::vector<fcitx::TelebitMacroConfig>>,
                      fcitx::ListDisplayOptionAnnotation>
            macros{
                this,
                "Macros",
                "Gõ tắt: từ viết tắt sẽ được thay bằng nội dung khi kết thúc từ",
                std::vector<fcitx::TelebitMacroConfig>{},
                fcitx::NoConstrain<std::vector<fcitx::TelebitMacroConfig>>(),
                fcitx::DefaultMarshaller<std::vector<fcitx::TelebitMacroConfig>>(),
                fcitx::ListDisplayOptionAnnotation("Abbrev")
            };

        fcitx::Option<std::vector<fcitx::TelebitForcePreeditAppConfig>,
                      fcitx::NoConstrain<std::vector<fcitx::TelebitForcePreeditAppConfig>>,
                      fcitx::DefaultMarshaller<std::vector<fcitx::TelebitForcePreeditAppConfig>>,
                      fcitx::ListDisplayOptionAnnotation>
            forcePreeditApps{
                this,
                "ForcePreeditApps",
                "Danh sách ứng dụng sử dụng telebit",
                // Chỉ áp dụng khi chưa có file config. Máy đã có config sẽ được
                // bật khi ứng dụng được focus lần đầu — xem
                // defaultPreeditPrograms() trong .cpp.
                [] {
                    std::vector<fcitx::TelebitForcePreeditAppConfig> apps;
                    for (const char *program :
                         {"firefox", "chrome", "google-chrome", "chromium"}) {
                        fcitx::TelebitForcePreeditAppConfig app;
                        *app.program.mutableValue() = program;
                        *app.enabled.mutableValue() = true;
                        apps.push_back(std::move(app));
                    }
                    return apps;
                }(),
                fcitx::NoConstrain<std::vector<fcitx::TelebitForcePreeditAppConfig>>(),
                fcitx::DefaultMarshaller<std::vector<fcitx::TelebitForcePreeditAppConfig>>(),
                fcitx::ListDisplayOptionAnnotation("Program")
            };
    );

    static constexpr char configFile[] = "conf/telebit-fcitx5.conf";
    static constexpr char statePropertyName[] = "telebitState";

    fcitx::FactoryFor<TelebitInputState> stateFactory_{
        [](fcitx::InputContext &) { return new TelebitInputState; }};

    fcitx::InputContextManager *icManager_ = nullptr;
    fcitx::Instance *instance_ = nullptr;
    std::unique_ptr<fcitx::HandlerTableEntry<fcitx::EventHandler>> focusInWatcher_;

    // Posts AI results back from worker threads onto the fcitx event loop.
    std::shared_ptr<fcitx::EventDispatcher> aiDispatcher_;
    // Cleared on destruction so a late scheduled callback becomes a no-op
    // instead of touching a dead engine (both run on the event-loop thread).
    std::shared_ptr<std::atomic<bool>> aiAlive_ =
        std::make_shared<std::atomic<bool>>(true);

    // Animated "thinking" indicator: while a request is in flight, a recurring
    // timer cycles the ellipsis ([AI.] → [AI..] → [AI...]). The timer targets
    // one input context (the one that dispatched) and self-stops once that
    // context is no longer busy.
    std::unique_ptr<fcitx::EventSourceTime> aiSpinnerTimer_;
    fcitx::TrackableObjectReference<fcitx::InputContext> aiSpinnerIc_;
    int aiSpinnerPhase_ = 0;

    TelebitFcitx5Config config_;
    std::unordered_set<std::string> seenProgramsLower_;
    std::unordered_set<std::string> forcePreeditEnabledLower_;
    std::shared_ptr<const EngineVietCpp::MacroTable> macrosLower_;
    bool configDirty_ = false;
    bool vietnameseEnabled_ = true;

    TelebitInputState *stateFor(fcitx::InputContext *ic) const;

    void resetAllInputStates();
    void rebuildSeenProgramsIndex();
    void rebuildMacroIndex();
    void normalizeForcePreeditApps();
    void recordSeenProgram(const std::string &program);
    void saveConfigIfDirty();

    TelexOptions currentOptions() const;
    void flushPending(fcitx::InputContext *ic, TelebitInputState *state);

    void updatePreedit(fcitx::InputContext *ic, TelebitInputState *state);

    void keyEventPreedit(fcitx::InputContext *ic, TelebitInputState *state,
                         fcitx::KeyEvent &keyEvent);
    void keyEventDirectRollback(fcitx::InputContext *ic, TelebitInputState *state,
                                fcitx::KeyEvent &keyEvent);

    // AI prompt mode.
    void enterAiPromptMode(fcitx::InputContext *ic, TelebitInputState *state);
    void keyEventAiPrompt(fcitx::InputContext *ic, TelebitInputState *state,
                          fcitx::KeyEvent &keyEvent);
    void updateAiPreedit(fcitx::InputContext *ic, TelebitInputState *state);
    void loadClipboardAsync(fcitx::InputContext *ic, TelebitInputState *state);
    void dispatchAiRequest(fcitx::InputContext *ic, TelebitInputState *state);
    void startAiSpinner(fcitx::InputContext *ic);
    void stopAiSpinner();
};

class TelebitFcitx5EngineFactory : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override {
        return new TelebitFcitx5Engine(manager);
    }
};
