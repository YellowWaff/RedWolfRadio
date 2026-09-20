// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <array>
#include <string>

namespace rwr {

enum class MusicAction { None, Next, Previous, PlayPause };

inline constexpr unsigned HotkeyCtrl = 1;
inline constexpr unsigned HotkeyShift = 2;
inline constexpr unsigned HotkeyAlt = 4;
inline constexpr unsigned HotkeyWin = 8;

// key is a Windows virtual-key code. A zero key disables the shortcut.
struct Hotkey {
    unsigned key = 0;
    unsigned modifiers = 0;
};

inline bool sameHotkey(const Hotkey& a, const Hotkey& b) {
    return a.key == b.key && a.modifiers == b.modifiers;
}

namespace hotkey_detail {
inline std::string normalized(const std::string& value) {
    const auto first = value.find_first_not_of(" \t\r\n\v\f");
    if (first == std::string::npos) return {};
    const auto last = value.find_last_not_of(" \t\r\n\v\f");
    std::string result = value.substr(first, last - first + 1);
    for (char& c : result) {
        if (c >= 'a' && c <= 'z') c = static_cast<char>(c - 'a' + 'A');
    }
    return result;
}

inline unsigned baseKey(const std::string& token) {
    if (token.size() == 1 && ((token[0] >= 'A' && token[0] <= 'Z') ||
                             (token[0] >= '0' && token[0] <= '9'))) {
        return static_cast<unsigned>(token[0]);
    }
    if ((token.size() == 2 || token.size() == 3) && token[0] == 'F' &&
        token[1] >= '1' && token[1] <= '9') {
        unsigned number = static_cast<unsigned>(token[1] - '0');
        if (token.size() == 3) {
            if (token[2] < '0' || token[2] > '9') return 0;
            number = number * 10 + static_cast<unsigned>(token[2] - '0');
        }
        if (number <= 24) return 0x70 + number - 1; // VK_F1 ... VK_F24
    }
    struct NamedKey { const char* name; unsigned key; };
    static constexpr NamedKey names[] = {
        {"LEFT", 0x25}, {"LEFTARROW", 0x25},
        {"UP", 0x26}, {"UPARROW", 0x26},
        {"RIGHT", 0x27}, {"RIGHTARROW", 0x27},
        {"DOWN", 0x28}, {"DOWNARROW", 0x28},
        {"SPACE", 0x20}, {"SPACEBAR", 0x20},
        {"PAGEUP", 0x21}, {"PGUP", 0x21},
        {"PAGEDOWN", 0x22}, {"PGDN", 0x22},
        {"HOME", 0x24}, {"END", 0x23},
        {"INSERT", 0x2d}, {"INS", 0x2d},
        {"DELETE", 0x2e}, {"DEL", 0x2e},
        {"ENTER", 0x0d}, {"RETURN", 0x0d},
        {"ESCAPE", 0x1b}, {"ESC", 0x1b},
        {"TAB", 0x09}, {"BACKSPACE", 0x08},
        {"MEDIANEXT", 0xb0}, {"MEDIAPREVIOUS", 0xb1},
        {"MEDIAPLAYPAUSE", 0xb3},
    };
    for (const auto& named : names) if (token == named.name) return named.key;
    return 0;
}

inline unsigned heldModifiers(const std::array<bool, 256>& down) {
    unsigned modifiers = 0;
    if (down[0x11] || down[0xa2] || down[0xa3]) modifiers |= HotkeyCtrl;
    if (down[0x10] || down[0xa0] || down[0xa1]) modifiers |= HotkeyShift;
    if (down[0x12] || down[0xa4] || down[0xa5]) modifiers |= HotkeyAlt;
    if (down[0x5b] || down[0x5c]) modifiers |= HotkeyWin;
    return modifiers;
}
} // namespace hotkey_detail

// Invalid input clears output so a failed parse cannot leave a stale binding.
inline bool parseHotkey(const std::string& text, Hotkey& output, std::string& error) {
    output = {};
    error.clear();
    const auto value = hotkey_detail::normalized(text);
    if (value == "NONE") return true;
    if (value.empty()) {
        error = "Empty shortcut; use None to disable it";
        return false;
    }
    Hotkey parsed;
    std::size_t start = 0;
    while (true) {
        const auto end = value.find('+', start);
        const auto token = hotkey_detail::normalized(value.substr(start,
            end == std::string::npos ? std::string::npos : end - start));
        if (token.empty()) {
            error = "Empty shortcut token or extra '+'";
            return false;
        }
        unsigned modifier = 0;
        if (token == "CTRL" || token == "CONTROL") modifier = HotkeyCtrl;
        else if (token == "SHIFT") modifier = HotkeyShift;
        else if (token == "ALT") modifier = HotkeyAlt;
        else if (token == "WIN" || token == "WINDOWS") modifier = HotkeyWin;
        if (modifier) {
            if (parsed.modifiers & modifier) {
                error = "Duplicate modifier: " + token;
                return false;
            }
            parsed.modifiers |= modifier;
        } else {
            const auto key = hotkey_detail::baseKey(token);
            if (!key) {
                error = "Unknown shortcut key: " + token;
                return false;
            }
            if (parsed.key) {
                error = "A shortcut must contain exactly one non-modifier key";
                return false;
            }
            parsed.key = key;
        }
        if (end == std::string::npos) break;
        start = end + 1;
    }
    if (!parsed.key) {
        error = "A shortcut needs a non-modifier key";
        return false;
    }
    output = parsed;
    return true;
}

class HotkeyTracker {
public:
    // Bindings are Next, Previous, PlayPause. The base key must be newly pressed
    // with exactly the configured modifiers. Adding modifiers to a held base
    // key never triggers an action. No global hooks or registrations are used.
    MusicAction sample(bool focused, const std::array<bool, 256>& down,
                       const std::array<Hotkey, 3>& bindings) {
        MusicAction action = MusicAction::None;
        if (focused && wasFocused_) {
            const auto modifiers = hotkey_detail::heldModifiers(down);
            constexpr MusicAction actions[] = {
                MusicAction::Next, MusicAction::Previous, MusicAction::PlayPause
            };
            for (std::size_t i = 0; i < bindings.size(); ++i) {
                const auto& binding = bindings[i];
                if (binding.key && binding.key < down.size() &&
                    binding.modifiers == modifiers && down[binding.key] &&
                    !previous_[binding.key]) {
                    action = actions[i];
                    break;
                }
            }
        }
        // Seed on every focus entry: keys already held must be released first.
        previous_ = down;
        wasFocused_ = focused;
        return action;
    }

private:
    std::array<bool, 256> previous_{};
    bool wasFocused_ = false;
};

} // namespace rwr
