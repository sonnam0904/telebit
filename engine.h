// Simple C++ engine that mimics the behavior of EngineViet in engine/ibus_engine.py
// but without IBus/GObject; this is a pure C++ buffer processor.

#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>

#include "vietnamese.h"

// Key values consistent with the Python engine (X11/IBus style).
constexpr std::uint32_t KEYVAL_SPACE = 0x20;
constexpr std::uint32_t KEYVAL_RETURN = 0xff0d;
constexpr std::uint32_t KEYVAL_BACKSPACE = 0xff08;
constexpr std::uint32_t KEYVAL_ESCAPE = 0xff1b;
constexpr std::uint32_t KEYVAL_TAB = 0xff09;

// Modifier masks (only the bits we care about).
constexpr std::uint32_t MOD_CONTROL = 1u << 2;  // IBus.ModifierType.CONTROL_MASK
constexpr std::uint32_t MOD_ALT = 1u << 3;      // IBus.ModifierType.MOD1_MASK
constexpr std::uint32_t MOD_RELEASE = 1u << 30; // IBus.ModifierType.RELEASE_MASK

struct KeyResult {
    // Whether this key event was handled (consumed) by the engine.
    bool handled = false;
    // Text committed to the application (may be empty).
    std::string commit_text;
};

class EngineVietCpp {
public:
    EngineVietCpp() = default;

    // Process a key event. This mirrors do_process_key_event:
    // - keyval: key symbol (ASCII or X11-style)
    // - keycode: hardware code (unused here)
    // - state: modifier mask (CONTROL/ALT/RELEASE)
    KeyResult process_key_event(std::uint32_t keyval,
                                std::uint32_t keycode,
                                std::uint32_t state);

    // Reset internal buffer (like do_reset).
    void reset();

    // Current raw buffer (Telex/VNI sequence).
    const std::string& buffer() const { return buffer_; }

    // Conversion options (input method, spell check, tone style).
    void setOptions(const TelexOptions& opts) { options_ = opts; }
    const TelexOptions& options() const { return options_; }

    // Abbreviation macros: lowercase abbreviation -> expansion, checked at commit.
    using MacroTable = std::unordered_map<std::string, std::string>;
    void setMacros(std::shared_ptr<const MacroTable> macros) { macros_ = std::move(macros); }

private:
    std::string buffer_;
    TelexOptions options_;
    std::shared_ptr<const MacroTable> macros_;

    std::string convert_buffer_for_commit() const;
};

