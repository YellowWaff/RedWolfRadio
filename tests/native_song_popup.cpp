// SPDX-License-Identifier: GPL-3.0-only
// Exercises the compiled adapter and formatter; does not load/inject the game.
#include "../src/music_trace.cpp"

static void require(bool ok, const char* why) {
    if (!ok) { fprintf(stderr, "FAIL: %s\n", why); exit(1); }
}
static int readable(const void* p, size_t bytes) {
    SetLastError(999);
    MEMORY_BASIC_INFORMATION region{};
    if (!p || !VirtualQuery(p, &region, sizeof(region)) || region.State != MEM_COMMIT ||
        (region.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return 0;
    const auto address = reinterpret_cast<uintptr_t>(p);
    const auto end = reinterpret_cast<uintptr_t>(region.BaseAddress) + region.RegionSize;
    return address <= end && bytes <= end - address;
}
static void logMessage(const char*, ...) { SetLastError(777); }
static const char* popupSetting = nullptr;
static int readSetting(const char*, const char*, const char* key, char* out, int cap, const char* fallback) {
    return snprintf(out, static_cast<size_t>(cap), "%s",
        !strcmp(key, "NativeSongPopup") && popupSetting ? popupSetting : fallback);
}
struct Arguments {
    void* game; int arg2; void* arg3; int arg4, arg5, type, title; const wchar_t* text; void* arg9;
};
static Arguments received{};
static unsigned calls = 0, hookRequests = 0;
static bool capture = false;
static std::wstring copied;
static void* fakeDialog(void* game, int a2, void* a3, int a4, int a5, int type, int title,
    const wchar_t* text, void* a9) {
    require(GetLastError() == 1234, "incoming last error preserved through snapshot/readability/logging");
    ++calls; received = {game, a2, a3, a4, a5, type, title, text, a9};
    if (capture) {
        require(TryAcquireSRWLockExclusive(&stateLock), "state lock released before native UI");
        ReleaseSRWLockExclusive(&stateLock);
        copied = text;
    }
    SetLastError(5678);
    return reinterpret_cast<void*>(0x12345678);
}
static int install(void* target, void* detour, void** continuation,
    const unsigned char* prefix, size_t stolen, const char*) {
    ++hookRequests;
    require(target == static_cast<unsigned char*>(host->exeModule) + kNativeDialogRva &&
        detour == reinterpret_cast<void*>(onNativeSongDialog), "hook target and adapter");
    require(stolen == 14 && !memcmp(prefix, kNativeDialogPrefix, 14), "exact whole-instruction prologue");
    // RML can expose the detour while its installation call is still running.
    SetLastError(1234);
    require(reinterpret_cast<NativeDialogFn>(detour)(reinterpret_cast<void*>(0x123), 2,
        nullptr, 4, 0, 1, 0x983e, L"Concurrent startup dialog", nullptr) == reinterpret_cast<void*>(0x12345678),
        "continuation executable before host returns (publication race)");
    *continuation = reinterpret_cast<void*>(fakeDialog); return 1;
}
static void call(const void* caller, const Arguments& a) {
    const unsigned before = calls;
    SetLastError(1234);
    const auto result = dispatchNativeSongDialog(caller, a.game, a.arg2, a.arg3, a.arg4,
        a.arg5, a.type, a.title, a.text, a.arg9);
    require(result == reinterpret_cast<void*>(0x12345678) && GetLastError() == 5678 &&
        calls == before + 1, "one continuation call with native result and last error");
    require(received.game == a.game && received.arg2 == a.arg2 && received.arg3 == a.arg3 &&
        received.arg4 == a.arg4 && received.arg5 == a.arg5 && received.type == a.type &&
        received.title == a.title && received.arg9 == a.arg9, "all eight non-text arguments forwarded unchanged");
    if (!capture) require(received.text == a.text, "unrelated text pointer forwarded unchanged");
}
static bool contains(const rwr::SongPopupText& text, const wchar_t* part) {
    return wcsstr(text.data(), part) != nullptr;
}
static unsigned frameCalls = 0, bodyCalls = 0, hitCalls = 0;
static void fakeFrame(void* game, void* dialog, int index) {
    require(GetLastError() == 1234 && game == reinterpret_cast<void*>(0x456) &&
        dialog == reinterpret_cast<void*>(0x789) && index == 12, "frame forwarding ABI");
    ++frameCalls; SetLastError(5678);
}
static void fakeBody(void* game, void* dialog) {
    require(GetLastError() == 1234 && game == reinterpret_cast<void*>(0x456) &&
        dialog == reinterpret_cast<void*>(0x789), "body forwarding ABI");
    ++bodyCalls; SetLastError(5678);
}
static bool fakeHit(void* game, void* dialog) {
    require(GetLastError() == 1234 && game == reinterpret_cast<void*>(0x456) &&
        dialog == reinterpret_cast<void*>(0x789), "hit forwarding ABI");
    ++hitCalls; SetLastError(5678); return true;
}
static void writeJump(unsigned char* where, const void* target) {
    where[0] = 0xff; where[1] = 0x25;
    const auto address = reinterpret_cast<uintptr_t>(target);
    memcpy(where + 6, &address, sizeof(address));
}
static void testPrivateUiContinuations(TsmHost& api) {
    // Real verified prologues run before stand-in bodies that unwind their
    // register saves and transfer to the C++ ABI probes.
    constexpr size_t bytes = 0x800000;
    auto* memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, bytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    require(memory != nullptr, "UI stand-in module"); api.exeModule = memory; api.exeSize = bytes;
    require(!nativePlayerPrefixMatches(kNativePlayerFrameRva, kNativePlayerFramePrefix), "changed frame refused");
    require(!nativePlayerPrefixMatches(kNativePlayerBodyRva, kNativePlayerBodyPrefix), "changed body refused");
    require(!nativePlayerPrefixMatches(kNativePlayerHitRva, kNativePlayerHitPrefix), "changed hit refused");
    memcpy(memory + kNativePlayerFrameRva, kNativePlayerFramePrefix, sizeof(kNativePlayerFramePrefix));
    memcpy(memory + kNativePlayerBodyRva, kNativePlayerBodyPrefix, sizeof(kNativePlayerBodyPrefix));
    memcpy(memory + kNativePlayerHitRva, kNativePlayerHitPrefix, sizeof(kNativePlayerHitPrefix));
    const unsigned char frameUnwind[] = {0x41,0x5f,0x41,0x5e,0x41,0x5d,0x5b,0x5d};
    const unsigned char hitUnwind[] = {0x48,0x81,0xc4,0xd0,0,0,0,0x5e,0x5b,0x5d};
    auto* frameBody = memory + kNativePlayerFrameRva + sizeof(kNativePlayerFramePrefix);
    memcpy(frameBody, frameUnwind, sizeof(frameUnwind));
    writeJump(frameBody + sizeof(frameUnwind), reinterpret_cast<void*>(fakeFrame));
    writeJump(memory + kNativePlayerBodyRva + sizeof(kNativePlayerBodyPrefix), reinterpret_cast<void*>(fakeBody));
    auto* hitBody = memory + kNativePlayerHitRva + sizeof(kNativePlayerHitPrefix);
    memcpy(hitBody, hitUnwind, sizeof(hitUnwind));
    writeJump(hitBody + sizeof(hitUnwind), reinterpret_cast<void*>(fakeHit));
    DWORD old = 0;
    require(VirtualProtect(memory, bytes, PAGE_EXECUTE_READ, &old) &&
        FlushInstructionCache(GetCurrentProcess(), memory, bytes), "UI stand-in executable");
    require(nativePlayerPrefixMatches(kNativePlayerFrameRva, kNativePlayerFramePrefix) &&
        nativePlayerPrefixMatches(kNativePlayerBodyRva, kNativePlayerBodyPrefix) &&
        nativePlayerPrefixMatches(kNativePlayerHitRva, kNativePlayerHitPrefix), "all verified UI prefixes");
    require(prepareNativePlayerContinuation(kNativePlayerFrameRva, kNativePlayerFramePrefix, previousNativePlayerFrame) &&
        prepareNativePlayerContinuation(kNativePlayerBodyRva, kNativePlayerBodyPrefix, previousNativePlayerBody) &&
        prepareNativePlayerContinuation(kNativePlayerHitRva, kNativePlayerHitPrefix, previousNativePlayerHit),
        "all private continuations published before hook installation");
    nativePlayerReady = false;
    SetLastError(1234); onNativePlayerFrame(reinterpret_cast<void*>(0x456), reinterpret_cast<void*>(0x789), 12);
    require(frameCalls == 1 && GetLastError() == 5678, "frame forwarded during partial installation");
    SetLastError(1234); onNativePlayerBody(reinterpret_cast<void*>(0x456), reinterpret_cast<void*>(0x789));
    require(bodyCalls == 1 && GetLastError() == 5678, "body forwarded during partial installation");
    SetLastError(1234); require(onNativePlayerHit(reinterpret_cast<void*>(0x456), reinterpret_cast<void*>(0x789)) &&
        hitCalls == 1 && GetLastError() == 5678, "hit result and errors forwarded during partial installation");
    VirtualFree(reinterpret_cast<void*>(previousNativePlayerFrame.exchange(nullptr)), 0, MEM_RELEASE);
    VirtualFree(reinterpret_cast<void*>(previousNativePlayerBody.exchange(nullptr)), 0, MEM_RELEASE);
    VirtualFree(reinterpret_cast<void*>(previousNativePlayerHit.exchange(nullptr)), 0, MEM_RELEASE);
    VirtualFree(memory, 0, MEM_RELEASE);
}
static void checkText(const rwr::SongPopupText& text) {
    const size_t count = wcslen(text.data());
    require(count <= 250 && !wcschr(text.data(), L'%'), "bounded total text with no format introducer");
    for (size_t i = 0; i < count; ++i) {
        const wchar_t c = text[i];
        if (c >= 0xd800 && c <= 0xdbff) {
            require(i + 1 < count && text[i + 1] >= 0xdc00 && text[i + 1] <= 0xdfff, "paired high surrogate"); ++i;
        } else require(c < 0xdc00 || c > 0xdfff, "no orphan low surrogate");
    }
}
int main() {
    auto text = rwr::songPopupText(L"Проверка музыки 100% %s %n %.*s %1$ %%\tend\r\n",
        rwr::SongPopupState::Playing, true);
    checkText(text);
    require(contains(text, L"Сейчас играет:") && contains(text, L"Проверка музыки 100[percent]") &&
        contains(text, L"[percent]s") && !contains(text, L"\r") && !contains(text, L"\t"), "Unicode and format/control sanitization");
    for (size_t n = 0; n < 300; ++n) {
        std::wstring name(n, L'Ж'); name += L"\xd83d\xdc3a"; name += L"%"; name.append(50, L'Я');
        for (bool ru : {false, true}) checkText(rwr::songPopupText(name, rwr::SongPopupState::Paused, ru));
    }
    text = rwr::songPopupText(std::wstring(500, L'%'), rwr::SongPopupState::Loading, false);
    checkText(text); require(wcslen(text.data()) == 250 && text[249] == L'\u2026', "expanded long filename truncated with ellipsis");
    checkText(rwr::songPopupText(L"\xd800x\xdc00", rwr::SongPopupState::Playing, false));

    TsmHost api{}; api.log = logMessage; api.readablePtr = readable;
    api.configString = readSetting; api.installInlineHook = install; host = &api;
    Config cfg; parseDisplayConfig(cfg); require(cfg.nativeSongPopup && !cfg.showTrackName, "player enabled for missing keys without transient HUD");
    popupSetting = "false"; parseDisplayConfig(cfg); require(!cfg.nativeSongPopup, "explicit player opt-out retained");
    popupSetting = "true"; parseDisplayConfig(cfg); require(cfg.nativeSongPopup, "editable player opt-in");
    popupSetting = "bad"; parseDisplayConfig(cfg); require(!cfg.nativeSongPopup && !cfg.showTrackName, "bad player setting does not change independent HUD setting");

    manager = ManagerState{}; manager.config.nativeSongPopup = true;
    manager.config.showTrackName = false; manager.notificationText = L"stale notification";
    manager.runtimeEnabled = true; manager.activeTrackId = "external:song"; manager.activeTrackPlaying = true;
    Track track; track.id = manager.activeTrackId; track.displayName = L"Проверка музыки 100%";
    track.preparedWide = L"C:\\cache\\abcdef.wav"; manager.catalog.push_back(track);
    text = currentSongPopupText(); checkText(text);
    require(contains(text, L"Now playing:") && contains(text, L"Проверка музыки 100[percent]") &&
        !contains(text, L"abcdef") && !contains(text, L"stale"), "active source name independent of cache and disabled/expired HUD");
    manager.userPaused = true; require(contains(currentSongPopupText(), L"Paused:"), "paused snapshot");
    manager.userPaused = false; manager.activeTrackPending = true;
    require(contains(currentSongPopupText(), L"Loading:"), "pending outranks playing");
    manager.config.displayRussian = true; require(contains(currentSongPopupText(), L"Загрузка:"), "Russian state label");
    manager.config.displayRussian = false; manager.activeTrackId.clear();
    require(contains(currentSongPopupText(), L"No track is playing."), "no active track");
    manager.runtimeEnabled = false;
    require(contains(currentSongPopupText(), L"unavailable"), "runtime unavailable snapshot");
    AcquireSRWLockExclusive(&stateLock);
    text = currentSongPopupText(); ReleaseSRWLockExclusive(&stateLock);
    require(contains(text, L"Updating."), "busy music lock never blocks native UI");
    manager.runtimeEnabled = true; manager.activeTrackId = track.id; manager.activeTrackPending = false;

    auto* memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0x700000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    require(memory != nullptr, "test module allocation"); api.exeModule = memory; api.exeSize = 0x700000;
    require(!installNativeSongPopup() && hookRequests == 0, "unknown owner refused before host hook request");
    memcpy(memory + kNativeDialogRva, kNativeDialogPrefix, 14);
    require(!installNativeSongPopup() && hookRequests == 0, "changed script caller refused before hook request");
    memcpy(memory + kScriptDialogReturnRva - 5, kScriptDialogCall, 5);
    // Fake native body jumps to the stand-in after executing the actual stolen
    // prologue. The real private trampoline is executed by the publication test.
    auto* nativeBody = memory + kNativeDialogRva + sizeof(kNativeDialogPrefix);
    nativeBody[0] = 0xff; nativeBody[1] = 0x25;
    const auto fakeAddress = reinterpret_cast<uintptr_t>(fakeDialog);
    memcpy(nativeBody + 6, &fakeAddress, sizeof(fakeAddress));
    DWORD old = 0; require(VirtualProtect(memory, 0x700000, PAGE_EXECUTE_READ, &old) &&
        FlushInstructionCache(GetCurrentProcess(), memory, 0x700000), "executable stand-in native body");
    manager.config.nativeSongPopup = false;
    require(!installNativeSongPopup() && hookRequests == 0, "disabled popup does not install");
    manager.config.nativeSongPopup = true; api.exeSize = 0x1000;
    require(!installNativeSongPopup() && hookRequests == 0, "short module refused"); api.exeSize = 0x700000;
    require(installNativeSongPopup() && hookRequests == 1 && previousNativeDialog.load() != nullptr,
        "verified optional hook installed with private continuation");

    const void* caller = memory + kScriptDialogReturnRva;
    Arguments own{reinterpret_cast<void*>(0x111), 1, nullptr, 4, 0, 1, 0x983e, rwr::kSongPopupMarker, nullptr};
    capture = true; call(caller, own);
    require(copied.find(L"Проверка музыки 100[percent]") != std::wstring::npos, "exact marker substituted");
    manager.catalog[0].displayName = L"Second song"; call(caller, own);
    require(copied.find(L"Second song") != std::wstring::npos, "each opening takes a fresh snapshot");
    capture = false; call(memory + kScriptDialogReturnRva + 1, own);
    for (unsigned variation = 0; variation < 10; ++variation) {
        auto other = own;
        switch (variation) {
        case 0: other.game = nullptr; break;
        case 1: other.arg2 = 2; break;
        case 2: other.arg3 = reinterpret_cast<void*>(0x222); break;
        case 3: other.arg4 = 5; break;
        case 4: other.arg5 = 1; break;
        case 5: other.type = 0x23; break;
        case 6: other.title = 0x983f; break;
        case 7: other.arg9 = reinterpret_cast<void*>(0x999); break;
        case 8: other.text = nullptr; break;
        case 9: other.text = reinterpret_cast<wchar_t*>(1); break;
        }
        call(caller, other);
    }
    for (const auto& otherText : {std::wstring(L"Unrelated script report"), std::wstring(rwr::kSongPopupMarker) + L"x",
        std::wstring(rwr::kSongPopupMarker).substr(0, 20)}) {
        auto other = own; other.text = otherText.c_str(); call(caller, other);
    }
    SetLastError(1234);
    require(onNativeSongDialog(own.game, 12, reinterpret_cast<void*>(0x345), 14, 15, 16, 17,
        L"ABI forwarding", reinterpret_cast<void*>(0x678)) == reinterpret_cast<void*>(0x12345678), "actual nine-argument entry ABI");
    require(received.arg2 == 12 && received.arg3 == reinterpret_cast<void*>(0x345) && received.arg4 == 14 &&
        received.arg5 == 15 && received.type == 16 && received.title == 17 &&
        received.arg9 == reinterpret_cast<void*>(0x678), "register and stack arguments remain intact");
    VirtualFree(reinterpret_cast<void*>(previousNativeDialog.load()), 0, MEM_RELEASE);
    VirtualFree(memory, 0, MEM_RELEASE); previousNativeDialog = nullptr;
    testPrivateUiContinuations(api);
    host = nullptr;
    puts("PASS: popup bounded Unicode/percent text, state snapshots, opt-in config, ownership guards, exact marker and nine-argument forwarding.");
}
