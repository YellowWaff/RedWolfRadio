// SPDX-License-Identifier: GPL-3.0-only
#include "../src/music_hotkeys.h"
#include <cstdio>
#include <cstdlib>
#include <initializer_list>

static void require(bool value, const char* message) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

static rwr::Hotkey parse(const std::string& text) {
    rwr::Hotkey result;
    std::string error;
    require(rwr::parseHotkey(text, result, error), text.c_str());
    require(error.empty(), "successful parse clears old errors");
    return result;
}

static std::array<bool, 256> keys(std::initializer_list<unsigned> held) {
    std::array<bool, 256> result{};
    for (auto key : held) result[key] = true;
    return result;
}

int main() {
    using namespace rwr;
    auto next = parse(" Ctrl + sHiFt + Right ");
    require(next.key == 0x27 && next.modifiers == (HotkeyCtrl | HotkeyShift),
        "case and surrounding whitespace are accepted");
    require(sameHotkey(next, parse("RightArrow+Control+Shift")),
        "aliases and modifier order compare equal for conflict detection");
    require(!sameHotkey(next, parse("Ctrl+Right")), "different modifiers differ");
    require(!sameHotkey(next, parse("Ctrl+Shift+Left")), "different keys differ");
    require(parse("none").key == 0 && parse("None").modifiers == 0,
        "None disables a shortcut");
    for (char c = 'A'; c <= 'Z'; ++c) {
        require(parse(std::string(1, c)).key == static_cast<unsigned>(c), "letter keys");
    }
    for (char c = '0'; c <= '9'; ++c) {
        require(parse(std::string(1, c)).key == static_cast<unsigned>(c), "number keys");
    }
    for (unsigned i = 1; i <= 24; ++i) {
        require(parse("F" + std::to_string(i)).key == 0x70 + i - 1, "F1 through F24");
    }
    struct Example { const char* text; unsigned key; };
    const Example examples[] = {
        {"Left", 0x25}, {"Up", 0x26}, {"Right", 0x27}, {"Down", 0x28},
        {"Space", 0x20}, {"PageUp", 0x21}, {"PageDown", 0x22},
        {"Home", 0x24}, {"End", 0x23}, {"Insert", 0x2d}, {"Delete", 0x2e},
        {"Enter", 0x0d}, {"Escape", 0x1b}, {"Tab", 0x09}, {"Backspace", 0x08},
        {"MediaNext", 0xb0}, {"MediaPrevious", 0xb1}, {"MediaPlayPause", 0xb3},
    };
    for (const auto& example : examples) {
        require(parse(example.text).key == example.key, example.text);
    }
    require(parse("Win+Alt+N").modifiers == (HotkeyWin | HotkeyAlt),
        "Win and Alt modifier parsing");
    const char* invalid[] = {
        "", "   ", "+", "+N", "N+", "Ctrl++N", "Ctrl+ +N",
        "Ctrl", "Ctrl+Shift", "Ctrl+Control+N", "Shift+shift+N",
        "Alt+Alt+N", "Windows+Win+N", "N+P", "Ctrl+N+N",
        "None+N", "Ctrl+None", "F0", "F01", "F25", "F100", "F2x",
        "Ctrl N", "Next", "Ctrl+Unknown", "Ctrl+Space extra", "Ctrl+;"
    };
    for (const char* text : invalid) {
        Hotkey result{0x27, HotkeyCtrl};
        std::string error;
        require(!parseHotkey(text, result, error), text);
        require(!error.empty(), "invalid shortcut has a useful error");
        require(result.key == 0 && result.modifiers == 0,
            "invalid input never retains a previous binding");
    }
    Hotkey disabled{0x27, HotkeyCtrl};
    std::string oldError = "previous failure";
    require(parseHotkey("None", disabled, oldError) && oldError.empty() && disabled.key == 0,
        "disabled binding resets output and error");

    const std::array<Hotkey, 3> bindings = {
        parse("Ctrl+Shift+Right"), parse("Ctrl+Shift+Left"), parse("Ctrl+Shift+Space")
    };
    const auto released = keys({});
    const auto modifiers = keys({0x11, 0x10});
    const auto nextHeld = keys({0x11, 0x10, 0x27});
    HotkeyTracker tracker;
    require(tracker.sample(true, released, bindings) == MusicAction::None,
        "initial sample only establishes key state");
    require(tracker.sample(true, nextHeld, bindings) == MusicAction::Next,
        "pressing a correctly modified base key triggers next");
    for (int i = 0; i < 100; ++i) {
        require(tracker.sample(true, nextHeld, bindings) == MusicAction::None,
            "held shortcut never repeats");
    }
    require(tracker.sample(true, modifiers, bindings) == MusicAction::None,
        "releasing base key does not trigger");
    require(tracker.sample(true, nextHeld, bindings) == MusicAction::Next,
        "second physical press triggers once");
    tracker.sample(true, released, bindings);
    require(tracker.sample(true, keys({0xa3, 0xa1, 0x25}), bindings) == MusicAction::Previous,
        "right-hand physical modifiers work");
    tracker.sample(true, released, bindings);
    require(tracker.sample(true, keys({0xa2, 0xa0, 0x20}), bindings) == MusicAction::PlayPause,
        "left-hand physical modifiers work");

    tracker.sample(true, released, bindings);
    require(tracker.sample(true, keys({0x11, 0x10, 0x12, 0x27}), bindings) == MusicAction::None,
        "extra Alt suppresses a binding");
    require(tracker.sample(true, nextHeld, bindings) == MusicAction::None,
        "removing extra modifier while base held cannot trigger");
    tracker.sample(true, released, bindings);
    require(tracker.sample(true, keys({0x11, 0x10, 0x5c, 0x27}), bindings) == MusicAction::None,
        "extra Windows modifier suppresses a binding");
    tracker.sample(true, released, bindings);
    require(tracker.sample(true, keys({0x27}), bindings) == MusicAction::None,
        "base key without required modifiers does not trigger");
    require(tracker.sample(true, nextHeld, bindings) == MusicAction::None,
        "adding modifiers to a held key cannot trigger");

    tracker.sample(false, released, bindings);
    require(tracker.sample(false, nextHeld, bindings) == MusicAction::None,
        "no shortcut fires while game unfocused");
    require(tracker.sample(true, nextHeld, bindings) == MusicAction::None,
        "held key is suppressed upon focus return");
    require(tracker.sample(true, nextHeld, bindings) == MusicAction::None,
        "held key remains suppressed after focus return");
    tracker.sample(true, modifiers, bindings);
    require(tracker.sample(true, nextHeld, bindings) == MusicAction::Next,
        "focus-return key must be released before new action");
    HotkeyTracker firstFocus;
    require(firstFocus.sample(true, nextHeld, bindings) == MusicAction::None,
        "key held on startup is suppressed");
    require(firstFocus.sample(true, nextHeld, bindings) == MusicAction::None,
        "startup suppression persists until release");
    firstFocus.sample(true, released, bindings);
    require(firstFocus.sample(true, nextHeld, bindings) == MusicAction::Next,
        "startup suppression ends after release");

    const std::array<Hotkey, 3> media = {
        parse("None"), parse("MediaPrevious"), parse("MediaPlayPause")
    };
    tracker.sample(true, released, media);
    require(tracker.sample(true, keys({0}), media) == MusicAction::None,
        "disabled shortcut never triggers, even at key zero");
    require(tracker.sample(true, keys({0xb1}), media) == MusicAction::Previous,
        "unmodified media key triggers action");
    tracker.sample(true, released, media);
    require(tracker.sample(true, keys({0xa4, 0xb3}), media) == MusicAction::None,
        "unmodified media key is suppressed when a modifier is held");

    const std::array<Hotkey, 3> simultaneous = {
        parse("N"), parse("P"), parse("Space")
    };
    tracker.sample(true, released, simultaneous);
    require(tracker.sample(true, keys({'N', 'P', 0x20}), simultaneous) == MusicAction::Next,
        "simultaneous actions resolve predictably to next");
    require(tracker.sample(true, keys({'N', 'P', 0x20}), simultaneous) == MusicAction::None,
        "other simultaneous keys cannot fire on a subsequent held sample");
    const std::array<Hotkey, 3> outOfBounds = {Hotkey{256, 0}, Hotkey{}, Hotkey{}};
    tracker.sample(true, released, outOfBounds);
    require(tracker.sample(true, keys({1}), outOfBounds) == MusicAction::None,
        "invalid programmatic virtual-key value never indexes out of bounds");

    std::puts("All hotkey parser and key-state tests passed.");
}
