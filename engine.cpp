// C++ buffer engine that follows the logic of engine/ibus_engine.py's EngineViet.

#include "engine.h"
#include "vietnamese.h"

KeyResult EngineVietCpp::process_key_event(std::uint32_t keyval,
                                           std::uint32_t /*keycode*/,
                                           std::uint32_t state) {
    KeyResult res;

    bool is_press = (state & MOD_RELEASE) == 0;
    if (!is_press) {
        res.handled = false;
        return res;
    }

    // Escape: clear buffer, do not commit.
    if (keyval == KEYVAL_ESCAPE) {
        if (!buffer_.empty()) {
            buffer_.clear();
            res.handled = true;
            return res;
        }
        res.handled = false;
        return res;
    }

    // Backspace: remove last character in buffer.
    if (keyval == KEYVAL_BACKSPACE) {
        if (!buffer_.empty()) {
            buffer_.pop_back();
            res.handled = true;
            return res;
        }
        res.handled = false;
        return res;
    }

    // Enter: commit buffer if any and let the Enter key go through (so messages send immediately).
    if (keyval == KEYVAL_RETURN) {
        if (!buffer_.empty()) {
            res.commit_text = convert_buffer_for_commit();
            buffer_.clear();
        }
        res.handled = false;
        return res;
    }

    // Space: commit buffer then add a literal space.
    if (keyval == KEYVAL_SPACE) {
        if (!buffer_.empty()) {
            res.commit_text = convert_buffer_for_commit();
            buffer_.clear();
            res.commit_text.push_back(' ');
            res.handled = true;
            return res;
        }
        res.handled = false;
        return res;
    }

    // If Control or Alt pressed, flush buffer and let the key pass.
    if ((state & (MOD_CONTROL | MOD_ALT)) != 0u) {
        if (!buffer_.empty()) {
            res.commit_text = convert_buffer_for_commit();
            buffer_.clear();
        }
        res.handled = false;
        return res;
    }

    // Alphabetic a-z / A-Z go into buffer.
    if (keyval >= 'a' && keyval <= 'z') {
        buffer_.push_back(static_cast<char>(keyval));
        res.handled = true;
        return res;
    }
    if (keyval >= 'A' && keyval <= 'Z') {
        buffer_.push_back(static_cast<char>(keyval));
        res.handled = true;
        return res;
    }

    // VNI: digits are modifiers of the current word; leading digits stay literal.
    if (options_.vniMode && keyval >= '0' && keyval <= '9' && !buffer_.empty()) {
        buffer_.push_back(static_cast<char>(keyval));
        res.handled = true;
        return res;
    }

    // Other characters: commit buffer and let key go through.
    if (!buffer_.empty()) {
        res.commit_text = convert_buffer_for_commit();
        buffer_.clear();
    }
    res.handled = false;
    return res;
}

void EngineVietCpp::reset() {
    buffer_.clear();
}

std::string EngineVietCpp::convert_buffer_for_commit() const {
    if (macros_ && !macros_->empty()) {
        std::string lower = buffer_;
        for (auto& ch : lower) {
            if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        }
        auto it = macros_->find(lower);
        if (it != macros_->end()) {
            return it->second;
        }
    }
    return convert_buffer(buffer_, options_);
}

