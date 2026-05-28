#pragma once

#include <string>

#include <fcitx/addonfactory.h>
#include <fcitx-config/configuration.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>

#include "engine.h"

namespace fcitx {
class AddonManager;
class InputContextManager;
} // namespace fcitx

/// Per-input-context typing state (buffer, direct-rollback fields).
struct TelebitInputState : public fcitx::InputContextProperty {
    EngineVietCpp engine;
    std::string rollbackRawAscii;
    std::string rollbackDisplay;

    void clearRollback() {
        rollbackRawAscii.clear();
        rollbackDisplay.clear();
    }

    void resetAll() {
        engine.reset();
        clearRollback();
    }
};

class TelebitFcitx5Engine : public fcitx::InputMethodEngineV2 {
public:
    explicit TelebitFcitx5Engine(fcitx::AddonManager *manager);

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
            "Bật bộ gõ không gạch chân (preedit) - Hỗ trợ một số ứng dụng",
            true
        };
    );

    static constexpr char configFile[] = "conf/telebit-fcitx5.conf";
    static constexpr char statePropertyName[] = "telebitState";

    fcitx::FactoryFor<TelebitInputState> stateFactory_{
        [](fcitx::InputContext &) { return new TelebitInputState; }};

    fcitx::InputContextManager *icManager_ = nullptr;

    TelebitFcitx5Config config_;

    TelebitInputState *stateFor(fcitx::InputContext *ic) const;

    void resetAllInputStates();

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
