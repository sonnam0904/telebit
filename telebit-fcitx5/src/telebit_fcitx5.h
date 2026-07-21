#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <unordered_set>

#include <fcitx/addonfactory.h>
#include <fcitx-config/configuration.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>
#include <fcitx-utils/key.h>

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

    void clearRollback() {
        rollbackRawAscii.clear();
        rollbackDisplay.clear();
    }

    void resetAll() {
        engine.reset();
        clearRollback();
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
                std::vector<fcitx::TelebitForcePreeditAppConfig>{[] {
                    fcitx::TelebitForcePreeditAppConfig app;
                    *app.program.mutableValue() = "firefox";
                    *app.enabled.mutableValue() = true; // chỉ khi chưa có file config
                    return app;
                }()},
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
};

class TelebitFcitx5EngineFactory : public fcitx::AddonFactory {
public:
    fcitx::AddonInstance *create(fcitx::AddonManager *manager) override {
        return new TelebitFcitx5Engine(manager);
    }
};
