// SPDX-License-Identifier: GPL-3.0-only
// File preparation and scheduler acceptance, without loading game binaries.
#include "../src/music_trace.cpp"
#include <filesystem>
static void require(bool ok, const char* message) {
    if (!ok) { fprintf(stderr, "FAIL: %s\n", message); exit(1); }
}
static void put32(std::vector<unsigned char>& bytes, size_t at, uint32_t n) {
    for (unsigned i = 0; i < 4; ++i) bytes[at + i] = static_cast<unsigned char>(n >> (8 * i));
}
static std::vector<unsigned char> fixture() {
    std::vector<unsigned char> bytes(70, 0);
    memcpy(bytes.data(), "RIFF", 4); put32(bytes, 4, 62); memcpy(bytes.data() + 8, "XWMAfmt ", 8);
    put32(bytes, 16, 18); bytes[20] = 0x61; bytes[21] = 1; bytes[22] = 2;
    put32(bytes, 24, 48000); put32(bytes, 28, 24000); bytes[32] = 2; bytes[34] = 16;
    memcpy(bytes.data() + 38, "dpds", 4); put32(bytes, 42, 8);
    put32(bytes, 46, 8); put32(bytes, 50, 16);
    memcpy(bytes.data() + 54, "data", 4); put32(bytes, 58, 8);
    // Four compressed bytes per packet, two packets.
    bytes[32] = 4;
    for (size_t i = 62; i < bytes.size(); ++i) bytes[i] = static_cast<unsigned char>(i);
    return bytes;
}
static void write(const std::wstring& path, const std::vector<unsigned char>& bytes) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE, "create fixture"); DWORD done = 0;
    require(WriteFile(file, bytes.data(), static_cast<DWORD>(bytes.size()), &done, nullptr) && done == bytes.size(), "write fixture");
    CloseHandle(file);
}
static FILETIME stamp(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA attributes{};
    require(GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &attributes), "read source timestamp");
    return attributes.ftLastWriteTime;
}
static int playing = 0;
static int fakePlaying() { SetLastError(918); return playing; }
static bool displayValuePresent = false;
static const char* displayValue = "false";
static const char* languageValue = "ru";
static int fakeConfig(const char*, const char*, const char* key, char* output, int capacity, const char* fallback) {
    const char* value = displayValuePresent ? (!strcmp(key, "Language") ? languageValue : displayValue) : fallback;
    return snprintf(output, static_cast<size_t>(capacity), "%s", value);
}
static void fakeLog(const char*, ...) {}
static int fakeReadable(const void* pointer, size_t) { return pointer != nullptr; }
static unsigned drawCalls = 0, frameCalls = 0, hookRequests = 0;
static std::wstring drawnTitle;
static void fakeDraw(void*, void*, float, float, unsigned long, const wchar_t* text) { ++drawCalls; drawnTitle = text; }
static void fakeFrame(void*) { ++frameCalls; SetLastError(317); }
static FrameClientFn registeredClient = nullptr;
static int fakeRegister(FrameClientFn client, void*) { registeredClient = client; return 1; }
static const FrameServicePrefix service{sizeof(FrameServicePrefix), 1, fakeRegister};
static const void* fakeConsume(const char* name, unsigned version) { return !strcmp(name, "ForceNodes") && version == 1 ? &service : nullptr; }
static void* drawSlot = reinterpret_cast<void*>(fakeDraw);
static void** fakeFind(void*, const char*, const char*) { return &drawSlot; }
static int fakeInstall(void*, void*, void** continuation, const unsigned char*, size_t stolen, const char*) {
    require(stolen == 14, "HUD stolen instructions end exactly at verified boundary");
    ++hookRequests; *continuation = reinterpret_cast<void*>(fakeFrame); return 1;
}
static void displayAcceptance() {
    TsmHost api{}; api.configString = fakeConfig; api.log = fakeLog; host = &api;
    Config config;
    parseDisplayConfig(config); require(!config.showTrackName && config.nativeSongPopup && !config.displayRussian,
        "old INI receives native player without enabling transient HUD");
    displayValuePresent = true; parseDisplayConfig(config);
    require(!config.showTrackName && !config.nativeSongPopup && config.displayRussian, "explicit display opt-outs and Russian label selection");
    displayValue = "true"; parseDisplayConfig(config);
    require(config.showTrackName && config.nativeSongPopup, "explicit HUD opt-in is preserved alongside player");
    displayValue = "broken"; parseDisplayConfig(config); require(!config.showTrackName, "bad display toggle disables only display");
    auto* memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, 0xa00000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    require(memory != nullptr, "fake render memory");
    uint64_t font = 0; void* fontPointer = &font;
    memcpy(memory + 0x994200, &fontPointer, sizeof(fontPointer));
    *reinterpret_cast<int*>(memory + 0x99528c) = 1280;
    *reinterpret_cast<int*>(memory + 0x995274) = 720;
    *reinterpret_cast<float*>(memory + 0x992088) = 1.f;
    api.exeModule = memory; api.readablePtr = fakeReadable; api.findIatSlot = fakeFind;
    api.installInlineHook = fakeInstall; api.consume = fakeConsume;
    manager.config.showTrackName = true; manager.config.displayRussian = true;
    manager.activeTrackId = manager.catalog[0].id; notifyTrackStartedLocked(GetTickCount64());
    require(installNotificationRenderer() && registeredClient && hookRequests == 0, "existing ForceNodes service avoids another frame hook");
    FrameContextPrefix frame{sizeof(frame), 0}; registeredClient(&frame, nullptr);
    require(drawCalls == 0, "before-frame callbacks cannot draw");
    frame.phase = 1; SetLastError(522); registeredClient(&frame, nullptr);
    require(drawCalls == 4 && drawnTitle == L"Original title" && GetLastError() == 522, "after-frame renders source title and preserves native error");
    manager.notificationUntil = GetTickCount64(); registeredClient(&frame, nullptr);
    require(drawCalls == 4, "expired notification draws nothing");
    api.consume = nullptr;
    require(!installNotificationRenderer() && hookRequests == 0, "unknown frame prefix refuses before host request");
    const unsigned char prefix[14] = {0x48,0x8b,0xc4,0x48,0x89,0x58,0x20,0x55,0x56,0x57,0x41,0x55,0x41,0x56};
    memcpy(memory + 0x2f0f10, prefix, 14);
    require(installNotificationRenderer() && hookRequests == 1, "native frame fallback requests proven prefix");
    onHudFrame(nullptr); require(frameCalls == 1 && GetLastError() == 317, "frame continuation executes once");
    manager.config.showTrackName = false;
    require(!installNotificationRenderer() && hookRequests == 1, "disabled HUD installs no hook");
    VirtualFree(memory, 0, MEM_RELEASE); host = nullptr; hudPrint = nullptr; previousFrame = nullptr;
    puts("PASS: existing INI display defaults, RU selection, optional renderer service/fallback, unknown-owner refusal, expiry, and exactly-once frame forwarding.");
}
int main(int argc, char** argv) {
    wchar_t temp[32768]{}; require(GetTempPathW(32768, temp), "temp path");
    const std::wstring root = std::wstring(temp) + L"RWR-native-test-" + std::to_wstring(GetCurrentProcessId());
    const std::wstring cache = root + L"\\cache";
    require(ensureDirectory(cache), "test directory");
    const std::wstring source = root + L"\\Original title.xwma";
    const auto bytes = fixture(); write(source, bytes);
    XwmaInfo info;
    require(validateXwma(source, &info) && info.packetCount == 2 && info.decodedBytes == 16, "valid RIFF XWMA packet metadata");
    auto padding = bytes; put32(padding, 50, 8); write(source, padding);
    require(validateXwma(source), "padding packet may repeat cumulative decoded bytes as shipped game music does");
    write(source, bytes);
    NativeAudioResult prepared; std::string error;
    FILETIME before = stamp(source);
    require(prepareNativeXwma(source, cache, prepared, error) && prepared.path == source && !prepared.copied, "ASCII XWMA uses original compressed source");
    const FILETIME after = stamp(source);
    require(CompareFileTime(&before, &after) == 0, "preparation preserves source timestamp");
    const std::wstring unicode = root + L"\\\u041f\u0435\u0441\u043d\u044f 100%.XWMA";
    write(unicode, bytes);
    require(prepareNativeXwma(unicode, cache, prepared, error) && prepared.copied && !prepared.cacheHit &&
        detectFormat(prepared.path) == TrackFormat::Xwma, "Unicode title gets compressed copy, never WAV");
    const std::wstring copy = prepared.path;
    NativeAudioResult again;
    require(prepareNativeXwma(unicode, cache, again, error) && again.cacheHit && again.path == copy, "unchanged XWMA cache reuse");
    auto corrupt = bytes; corrupt[62] ^= 1; write(copy, corrupt);
    require(prepareNativeXwma(unicode, cache, again, error) && !again.cacheHit, "changed cached content is replaced with exact source");
    const auto cacheEntries = enumerateCacheFiles(cache);
    require(cacheEntries.size() == 1 && cacheEntries[0].path == copy, "compressed copies participate in bounded cache enumeration");
    for (const auto& entry : cacheEntries)
        require(detectFormat(entry.path) == TrackFormat::Xwma, "no decoded WAV cache created");
    const std::wstring bad = root + L"\\bad.xwma";
    for (unsigned fault = 0; fault < 7; ++fault) {
        corrupt = bytes;
        if (fault == 0) corrupt.resize(69);
        if (fault == 1) corrupt[8] = 'W';
        if (fault == 2) corrupt[20] = 0x62;
        if (fault == 3) put32(corrupt, 50, 4);
        if (fault == 4) put32(corrupt, 58, 0);
        if (fault == 5) put32(corrupt, 42, 0xfffffffcu);
        if (fault == 6) corrupt[32] = 3;
        write(bad, corrupt); require(!validateXwma(bad), "malformed/unsupported XWMA rejected");
    }
    manager = ManagerState{}; manager.cacheDirectory = cache;
    Track track; track.id = "external:original title.xwma"; track.absoluteWide = source; track.format = TrackFormat::Xwma;
    track.displayName = L"Original title"; manager.catalog.push_back(track);
    require(prepareTrackAtIndex(0) && manager.catalog[0].preparedWide.empty() && manager.converted == 0,
        "direct source excluded from owned cache and conversion count");
    std::vector<Track> scanned; std::set<std::string> ids;
    collectTrackRecursively(root, L"", TrackSource::External, false, scanned, ids);
    bool found = false;
    for (const Track& item : scanned) if (item.absoluteWide == unicode) {
        require(item.displayName == L"\u041f\u0435\u0441\u043d\u044f 100%", "source case/Unicode/percent survive extension removal"); found = true;
    }
    require(found, "Unicode source discovered");
    // The notification lifecycle is optional now; enable it explicitly for this test.
    manager.config.showTrackName = true;
    manager.runtimeEnabled = true; originals[Playing] = reinterpret_cast<void*>(fakePlaying);
    volatile char loading = 1; engineLoadingFlag = &loading;
    activateTrackLocked(0); manager.deferredPlay = true; playing = 1;
    onPlaying(); require(manager.notificationUntil == 0, "no notification for deferred paused selection");
    manager.deferredPlay = false; onPlaying(); require(manager.notificationUntil == 0, "no notification during loading");
    loading = 0; onPlaying();
    const ULONGLONG deadline = manager.notificationUntil;
    require(manager.notificationText == L"Original title" && deadline >= GetTickCount64() + 4900 &&
        deadline <= GetTickCount64() + 5000, "confirmed playback starts five-second source title notification");
    onPlaying(); require(manager.notificationUntil == deadline && GetLastError() == 918, "polls never extend display or change status error");
    playing = 0; onPlaying(); require(manager.notificationText.empty(), "track end clears notification");
    manager.config.showTrackName = false; activateTrackLocked(0); playing = 1;
    onPlaying(); require(manager.notificationUntil == 0, "boolean disables notification");
    require(rwr::notificationName(L"long\nname", 6) == L"long \u2026", "safe length and control characters");
    displayAcceptance();
    const auto resolved = std::filesystem::weakly_canonical(root);
    require(resolved.parent_path() == std::filesystem::weakly_canonical(temp) &&
        resolved.filename().wstring() == L"RWR-native-test-" + std::to_wstring(GetCurrentProcessId()), "cleanup stays in exact temporary test directory");
    std::filesystem::remove_all(resolved);
    puts("PASS: native XWMA routes, compressed copy integrity/reuse, malformed packets, source preservation, Unicode filenames, and actual-start notification lifecycle.");
    // Optional local read-only acceptance of real RIFF headers. This confirms
    // containers, not audible decoding; no third-party audio is redistributed.
    for (int argument = 1; argument < argc; ++argument) {
        unsigned accepted = 0, rejected = 0;
        for (const auto& entry : std::filesystem::recursive_directory_iterator(argv[argument])) {
            if (!entry.is_regular_file() || detectFormat(entry.path().wstring()) != TrackFormat::Xwma) continue;
            if (validateXwma(entry.path().wstring())) ++accepted;
            else { ++rejected; fprintf(stderr, "Unsupported/damaged XWMA: %s\n", entry.path().string().c_str()); }
        }
        printf("Read-only XWMA inspection %s: %u accepted, %u rejected\n", argv[argument], accepted, rejected);
        require(accepted > 0 && rejected == 0, "real container fixtures match supported profile");
    }
}
