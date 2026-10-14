// SPDX-License-Identifier: GPL-3.0-only
// Native UI integration against bounded fake game objects and export callbacks.
// No game process is loaded or injected and no installed files are modified.
#include "../src/native_player_ui.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

using namespace rwr;
namespace ui = rwr::native_player_detail;

static void require(bool ok, const char* why) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}
static int readable(const void* p, size_t bytes) {
    MEMORY_BASIC_INFORMATION info{};
    if (!p || !VirtualQuery(p, &info, sizeof(info)) || info.State != MEM_COMMIT ||
        (info.Protect & (PAGE_NOACCESS | PAGE_GUARD))) return 0;
    const auto start = reinterpret_cast<uintptr_t>(p);
    const auto end = reinterpret_cast<uintptr_t>(info.BaseAddress) + info.RegionSize;
    return start <= end && bytes <= end - start;
}
static TsmHost api{};
static unsigned char* executable = nullptr;
static constexpr size_t executableSize = 0xa55300;
static std::array<unsigned char, 0x13bf8> game{};
static std::array<unsigned char, 0x70> guide{};
static std::array<unsigned char, 0x5d8> window{}, otherWindow{};
static std::array<void*, 2> windows{};
static std::array<void*, 15> textureTable{};
static std::array<void*, 11> fontTable{};
static void* texture = textureTable.data();
static uintptr_t shader = 1;
static void* font = fontTable.data();
static ui::Vec3 mouse{};
static bool mouseDown = false, mousePress = false, queueAccepts = true;
static bool whiteBound = false;
static unsigned snapshots = 0, consumed = 0, bound = 0;
static NativePlayerSnapshot liveSnapshot{};
static std::vector<MusicAction> actions;
static std::vector<std::wstring> printed;
static std::vector<ui::Rect> rectangles;
static std::vector<std::wstring> measured;

static void dimensions(void* p, float w, float h) { ui::write(p, 0x288, w); ui::write(p, 0x28c, h); }
static void position(void* p, const ui::Vec3* v) { ui::write(p, 0x290, *v); }
static void rotationCenter(void* p, const ui::Vec3* v) { ui::write(p, 0x29c, *v); }
static void color(void* p, const ui::Color* c) { ui::write(p, 0x2ac, *c); }
static void rotation(void* p, float v) { ui::write(p, 0x2a8, v); }
static void scale(void* p, float v) { ui::write(p, 0x2c0, v); }
static ui::Rect currentRectangle(void* p) {
    const auto pos = ui::read<ui::Vec3>(p, 0x290);
    const float w = ui::read<float>(p, 0x288), h = ui::read<float>(p, 0x28c);
    return {pos.x-w/2.f, pos.y-h/2.f, w, h};
}
static void draw(void* p, float a, float b, float c, float d, float e, bool immediate) {
    require(a == 0 && b == 0 && c == 1 && d == 1 && e == 0 && immediate, "native rectangle draw ABI");
    require(whiteBound, "each panel rectangle binds white texture after text changed renderer state");
    rectangles.push_back(currentRectangle(p));
}
static ui::Vec3* mousePosition(void* p, ui::Vec3* out) {
    require(p == ui::input(), "native mouse accessor object"); *out = mouse; return out;
}
static bool down(void*) { return mouseDown; }
static bool press(void*) { return mousePress; }
static bool takePress(void*) {
    if (!mousePress) return false;
    mousePress = false; ++consumed; return true;
}
static bool collision(void* p, ui::Vec3* point, float insetX, float insetY) {
    require(insetX == 0 && insetY == 0, "native collision ABI");
    const bool result = ui::contains(currentRectangle(p), *point);
    // The real Collision mutates its by-value vector argument. Every hit must
    // supply a fresh copy or later controls become dependent on earlier hits.
    point->y += 10000.f;
    return result;
}
static void print(void* manager, void* actualFont, float, float, unsigned long, const wchar_t* text) {
    require(manager == executable + 0x996fb0 && actualFont == &font, "literal native font call ABI");
    printed.emplace_back(text);
    whiteBound = false; // Native font rendering binds its own texture.
}
static ui::TextRect* measure(void* actualFont, ui::TextRect* out, float x, float y, const wchar_t* value) {
    require(actualFont == &font && x == 0 && y == 0, "NoArg font measurement hidden-result ABI");
    measured.emplace_back(value);
    *out = {0,0,static_cast<int>(wcslen(value))*8,16}; return out;
}
static void bindTexture(void* actualTexture, unsigned slot, void* actualShader) {
    require(actualTexture == &texture && slot == 0 && actualShader == &shader, "native texture dispatch ABI");
    ++bound;
    whiteBound = true;
}
static void snapshot(NativePlayerSnapshot& out) { ++snapshots; out = liveSnapshot; }
static bool queue(MusicAction action) { actions.push_back(action); return queueAccepts; }
static void song(const wchar_t* name, SongPopupState state = SongPopupState::Playing, bool ru = false) {
    liveSnapshot = {};
    wcsncpy(liveSnapshot.name.data(), name, liveSnapshot.name.size()-1);
    liveSnapshot.state = state; liveSnapshot.russian = ru;
}
static bool saw(const wchar_t* text) {
    return std::find(printed.begin(), printed.end(), text) != printed.end();
}
static ui::Layout currentLayout() {
    ui::Layout result{};
    require(ui::getLayout(window.data(), result), "valid test viewport/layout");
    return result;
}
static void pointAt(const ui::Rect& r) { mouse = {r.x+r.w/2.f, r.y+r.h/2.f, 0}; }
static void paint(bool refresh = true) {
    if (refresh) ui::context.lastSnapshotAt = GetTickCount64()-1000;
    drawNativePlayerFrame(game.data(), window.data(), 0);
}
static void reset() {
    game.fill(0); guide.fill(0); window.fill(0); otherWindow.fill(0);
    snapshots = consumed = bound = 0; actions.clear(); printed.clear(); rectangles.clear(); measured.clear();
    mouse = {}; mouseDown = mousePress = false; queueAccepts = true; whiteBound = false;
    ui::context = {};
    api.exeModule = executable; api.exeSize = executableSize; api.readablePtr = readable;
    ui::context.host = &api; ui::context.snapshot = snapshot; ui::context.queue = queue;
    ui::context.dimensions = dimensions; ui::context.position = position;
    ui::context.rotationCenter = rotationCenter; ui::context.color = color;
    ui::context.rotation = rotation; ui::context.scale = scale; ui::context.draw = draw;
    ui::context.mouse = mousePosition; ui::context.down = down;
    ui::context.press = press; ui::context.takePress = takePress;
    ui::context.collision = collision; ui::context.print = print;
    ui::write(executable, 0x99528c, 1440); ui::write(executable, 0x995274, 1080);
    ui::write(executable, 0x992088, 1.f); ui::write(executable, 0x994200, &font);
    ui::write(executable, 0x9ead08, &shader);
    textureTable[0x70 / sizeof(void*)] = reinterpret_cast<void*>(bindTexture);
    fontTable.fill(nullptr);
    ui::write(game.data(), 0xb048, &texture);
    windows = {window.data(), otherWindow.data()};
    ui::write(game.data(), 0xcac0, windows.data());
    ui::write(game.data(), 0xcac8, windows.data()+windows.size());
    ui::write(game.data(), 0xad74, -1);
    ui::write(window.data(), 4, 720.f); ui::write(window.data(), 8, 540.f);
    ui::write(window.data(), 0x23c, kNativePlayerReference);
    ui::write(window.data(), 0x34, 1); ui::write(window.data(), 0x38, 0x983e);
    ui::write(window.data(), 0x248, 4); ui::write(window.data(), 0x24c, 1);
    require(markNativePlayerWindow(window.data()), "mark verified fixture window");
    ui::context.openedAt = GetTickCount64()-1000;
    song(L"First track");
    std::memset(static_cast<unsigned char*>(ui::panel())+0x287, 0xa5, 0x3e);
}
static void checkPanelRestored() {
    const auto* p = static_cast<unsigned char*>(ui::panel())+0x287;
    for (size_t i=0; i<0x3e; ++i) require(p[i] == 0xa5, "shared panel state and adjacent sentinels restored");
}

static void testIdentityAndRefresh() {
    reset();
    require(isNativePlayerWindow(window.data()) && !isNativePlayerWindow(nullptr) &&
        !isNativePlayerWindow(reinterpret_cast<void*>(1)), "owned identity and unreadable objects");
    for (size_t offset : {size_t(0x23c),size_t(0x34),size_t(0x38),size_t(0x248),size_t(0x24c),kNativePlayerCookieOffset}) {
        otherWindow = window; otherWindow[offset] ^= 1;
        const auto untouched = otherWindow;
        require(!isNativePlayerWindow(otherWindow.data()), "every identity component is required");
        drawNativePlayerFrame(game.data(), otherWindow.data(), 1);
        require(otherWindow == untouched && snapshots == 0 && rectangles.empty(), "foreign window is never modified or drawn");
    }
    paint();
    require(saw(L"First track") && saw(L"Red Wolf Radio") && !saw(L"Next") && snapshots == 1 && bound >= 1,
        "icon player reads source title without persistent button captions");
    printed.clear(); song(L"Проверка 100% %s %n", SongPopupState::Paused, true); paint(false);
    require(snapshots == 1 && saw(L"First track"), "snapshot throttle reuses last complete display inside refresh interval");
    printed.clear(); pointAt(currentLayout().buttons[2]); paint();
    require(saw(L"Проверка 100% %s %n") && saw(L"Играть") && !saw(L"First track"),
        "existing window refreshes literal title and localized hover action when paused");
    printed.clear(); song(L"", SongPopupState::Busy); paint();
    require(saw(L"Проверка 100% %s %n") && saw(L"Играть"), "busy snapshot retains last complete display");
    checkPanelRestored();
}

static void testControlsAndGating() {
    constexpr MusicAction expected[] = {MusicAction::PreviousTrack, MusicAction::Restart, MusicAction::PlayPause, MusicAction::Next};
    for (size_t button=0; button<4; ++button) {
        reset(); pointAt(currentLayout().buttons[button]); mouseDown = mousePress = true; paint();
        require(actions == std::vector<MusicAction>{expected[button]} && consumed == 1 && !mousePress,
            "each native button consumes one click and queues its explicit action");
        paint(); require(actions.size() == 1 && consumed == 1, "held mouse cannot repeat a control action");
        checkPanelRestored();
    }
    for (unsigned gate=0; gate<8; ++gate) {
        reset(); pointAt(currentLayout().buttons[3]); mouseDown = mousePress = true;
        if (gate == 0) std::swap(windows[0], windows[1]);
        if (gate == 1) ui::write<unsigned char>(game.data(), 0xfc1, 1);
        if (gate == 2) ui::write(game.data(), 0xad74, 0);
        if (gate == 3) ui::write<unsigned char>(window.data(), 0, 1);
        if (gate == 4) ui::context.openedAt = GetTickCount64();
        if (gate == 5) ui::write(game.data(),0xcac8,windows.data());
        if (gate == 6) ui::write(game.data(),0xcac0,reinterpret_cast<void*>(1));
        if (gate == 7) {
            ui::write(game.data(),0x13bf0,guide.data());
            ui::write<unsigned char>(guide.data(),0x14,1);
            ui::write(guide.data(),0x68,window.data());
        }
        paint(); require(actions.empty() && consumed == 0 && mousePress,
            "background, native input blocks, closed window or opening cooldown cannot consume controls");
        checkPanelRestored();
    }
    reset(); pointAt(currentLayout().buttons[0]); mouseDown = mousePress = true; queueAccepts = false; paint();
    require(actions.size() == 1 && consumed == 1 && !mousePress, "rejected queue click remains consumed rather than falling through");
    reset(); const auto l = currentLayout(); mouse = {l.frame.x+20, l.frame.y+120, 0}; mousePress = true; paint();
    require(actions.empty() && consumed == 1 && !mousePress, "empty player body consumes foreground click without an action");
    require(ui::read<unsigned char>(game.data(), 0xfc3) == 1 && ui::read<void*>(game.data(), 0xcb80) == window.data(),
        "hover advertises native window ownership to world input bookkeeping");
    reset(); ui::write<unsigned char>(window.data(),1,1); pointAt(currentLayout().buttons[3]);
    mouseDown = mousePress = true;
    require(!nativePlayerHit(game.data(),window.data()), "minimized lower body does not capture native hit");
    paint();
    require(actions.empty() && consumed == 0 && mousePress && !rectangles.empty() &&
        rectangles.front().h == ui::kTitleHeight && ui::read<unsigned char>(game.data(),0xfc3) == 0,
        "minimized window draws only header and leaves lower body input free");
}

static void testDragClampClose() {
    reset(); pointAt(currentLayout().title); mouseDown = mousePress = true; paint();
    require(ui::context.dragIdentity == reinterpret_cast<uintptr_t>(window.data()) && consumed == 1,
        "title press starts drag and consumes initiating click");
    mouse = {-5000,-5000,0};
    require(nativePlayerHit(game.data(),window.data()), "drag retains native hit ownership outside frame");
    paint(); auto l = currentLayout();
    require(l.frame.x == 8 && l.frame.y == 8, "drag clamps to visible top-left margins");
    mouse = {5000,5000,0}; paint(); l = currentLayout();
    require(l.frame.x+l.frame.w <= 1432 && l.frame.y+l.frame.h <= 1072, "drag clamps bottom-right frame to viewport");
    mouseDown = false; paint();
    require(ui::context.dragIdentity == 0 && !nativePlayerHit(game.data(),window.data()), "mouse release ends drag ownership");
    ui::write(executable,0x99528c,640); ui::write(executable,0x995274,480); paint(); l = currentLayout();
    require(l.frame.x+l.frame.w <= 632 && l.frame.y+l.frame.h <= 472, "resolution shrink clamps the existing player onscreen");
    checkPanelRestored();
    pointAt(l.close); mouseDown = mousePress = true; const unsigned before = consumed; paint();
    require(ui::read<unsigned char>(window.data(),0) == 1 && consumed == before+1 &&
        ui::context.dragIdentity == 0 && actions.empty() && ui::read<float>(game.data(),0xfc8) == -.5f,
        "close consumes click, flags owned window and arms native click-through cooldown");
    require(!nativePlayerHit(game.data(),window.data()), "closed window cannot capture native hit");
    checkPanelRestored();
}

static void testTextBoundsAndLayout() {
    reset();
    for (float w : {320.f,640.f,1440.f,2560.f}) {
        const auto l = ui::layout(-500,-500,2.f,w,200.f);
        require(l.frame.x >= 8 && l.frame.y >= 8 && l.frame.x+l.frame.w <= w-8+.01f &&
            l.frame.y+l.frame.h <= 192.01f, "compact layout fits supported small viewports");
    }
    for (size_t columns=2; columns<80; ++columns) {
        const auto lines = ui::nameLines(std::wstring(100,L'Ж') + L"\xd83d\xdc3a\t%\n" + std::wstring(160,L'Я'), columns);
        for (const auto& line : lines) {
            require(wcslen(line.data()) <= columns, "wrapped native name line remains bounded");
            for (size_t i=0; line[i]; ++i) {
                const wchar_t c = line[i];
                if(c>=0xd800 && c<=0xdbff) {
                    require(line[i+1]>=0xdc00 && line[i+1]<=0xdfff, "wrapping cannot split surrogate pair"); ++i;
                } else require(c<0xdc00 || c>0xdfff, "wrapping has no orphan surrogate");
                require(c>=32, "filename control characters become harmless spaces");
            }
        }
    }
    fontTable[0x50 / sizeof(void*)] = reinterpret_cast<void*>(measure);
    const auto exact = ui::measuredNameLines(L"100% %s %n",160,2);
    require(std::wstring(exact[0].data()) == L"100% %s %n" && exact[1][0] == 0 &&
        std::find(measured.begin(),measured.end(),L"100% %s %n") != measured.end(),
        "NoArg measured wrapping preserves raw percent and uses actual font width");
    const auto narrow = ui::measuredNameLines(L"ABCDE\xd83d\xdc3a" L"FGHIJKLMNOP",48,100);
    require(ui::textWidth(narrow[0].data())<=48 && ui::textWidth(narrow[1].data())<=48 &&
        std::wstring(narrow[1].data()).find(L'\u2026')!=std::wstring::npos,
        "measured long-name lines fit panel and mark truncation");
    measured.clear(); paint();
    require(std::find(measured.begin(),measured.end(),L"First track") != measured.end(),
        "first rendered name measures native font");
    measured.clear(); paint(false);
    require(measured.empty(), "unchanged icon frame reuses cached name measurements without caption measurements");
    song(L"Different song"); paint();
    require(std::find(measured.begin(),measured.end(),L"Different song") != measured.end(),
        "changed song invalidates cached native font measurements");
}

static void testCompactIconsAndHover() {
    reset();
    auto l = currentLayout();
    require(l.frame.w == 360 && l.frame.h == 128 && l.frame.w*l.frame.h <= 500*180*.52f,
        "smaller frame occupies about half the text-button area");
    for (const auto& button : l.buttons)
        require(button.w >= 40 && button.h >= 28 && button.y+button.h <= l.frame.y+l.frame.h-8,
            "icon retains a usable button target and bottom margin");
    constexpr const wchar_t* english[] = {L"Previous",L"Restart",L"Pause",L"Next"};
    constexpr const wchar_t* russian[] = {L"Предыдущая",L"Сначала",L"Пауза",L"Следующая"};
    for (bool ru : {false,true}) for (size_t i=0;i<4;++i) {
        printed.clear(); song(L"Track",SongPopupState::Playing,ru); pointAt(l.buttons[i]); paint();
        require(saw(ru?russian[i]:english[i]) && !saw(L"Red Wolf Radio"), "hover action replaces header in selected language");
    }
    rectangles.clear(); printed.clear(); song(L"Track",SongPopupState::Playing); paint();
    const size_t playingShapes = rectangles.size();
    rectangles.clear(); song(L"Track",SongPopupState::Paused); pointAt(l.buttons[2]); paint();
    require(saw(L"Play") && rectangles.size() != playingShapes, "pause state changes transport geometry to Play and hover action");
    for (const auto& r : rectangles)
        require(r.x >= l.frame.x && r.y >= l.frame.y && r.x+r.w <= l.frame.x+l.frame.w+.01f &&
            r.y+r.h <= l.frame.y+l.frame.h+.01f, "all icon geometry stays inside compact frame");
    mouse={}; fontTable[0x50 / sizeof(void*)] = reinterpret_cast<void*>(measure);
    printed.clear(); song(L"",SongPopupState::Unavailable); paint();
    require(saw(L"Song information unavailable"), "empty song displays localized state instead of blank player");
    checkPanelRestored();
}

int main() {
    executable = static_cast<unsigned char*>(VirtualAlloc(nullptr,executableSize,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));
    require(executable != nullptr, "allocate bounded fake executable globals");
    testIdentityAndRefresh(); testDragClampClose(); testTextBoundsAndLayout(); testControlsAndGating(); testCompactIconsAndHover();
    ui::context = {}; VirtualFree(executable,0,MEM_RELEASE);
    puts("PASS: native player ownership, live refresh, literal text, native input gating/consumption, controls, drag/clamp/close and shared panel restoration.");
}
