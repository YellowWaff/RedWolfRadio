// SPDX-License-Identifier: GPL-3.0-only
// Tests the actual compiled wrappers against stand-in engine functions, without
// injecting a process or modifying game files. This is not an in-game test.
#include "../src/music_trace.cpp"
#include <stdlib.h>
#include <initializer_list>

static int called[8]{};
static char* expectedPath;
static bool expectedBool;
static int expectedInt;
static uint32_t expectedFloat;
static int expectedStatus;
static const char* expectedSource;
static void require(bool condition, const char* message) {
    if (!condition) { fprintf(stderr, "FAIL: %s\n", message); exit(1); }
}
static void checkInputError() { require(GetLastError() == 1234, "incoming last-error preserved"); }
#define FAKE_VOID(fn, k) static void fn() { checkInputError(); ++called[k]; SetLastError(5678); }
FAKE_VOID(fakeInit, Init)
FAKE_VOID(fakePause, Pause)
FAKE_VOID(fakeResume, Resume)
FAKE_VOID(fakeStop, Stop)
static void fakeLoad(char* path) {
    checkInputError(); require(path == expectedPath, "load pointer forwarded identically");
    ++called[Load]; SetLastError(5678);
}
static void fakePlay(bool a, int b) {
    checkInputError(); require(a == expectedBool && b == expectedInt, "play arguments forwarded identically");
    ++called[Play]; SetLastError(5678);
}
static void fakeVolume(float value) {
    checkInputError(); uint32_t bits; memcpy(&bits, &value, sizeof(bits));
    require(bits == expectedFloat, "float argument bits preserved"); ++called[Volume]; SetLastError(5678);
}
static int fakePlaying() { checkInputError(); ++called[Playing]; SetLastError(5678); return expectedStatus; }
static void fakePath(char* destination, const char* source) {
    checkInputError(); require(source == expectedSource, "ordinary path source forwarded identically");
    strcpy(destination, "ORIGINAL"); SetLastError(5678);
}
static void checkOutputError() { require(GetLastError() == 5678, "outgoing last-error preserved"); }
static void prepare() { SetLastError(1234); }
static int concurrentPlaying() { SetLastError(5678); return 1; }
static void fakeTimeoutStop() {
    require(GetLastError() == 5678, "timeout stop receives status-call last-error");
    ++called[Stop]; SetLastError(9999);
}
static DWORD WINAPI poll(void*) {
    for (int i = 0; i < 10000; ++i) { prepare(); require(onPlaying() == 1, "concurrent status forwarding"); checkOutputError(); }
    return 0;
}
int main() {
    require(SetEnvironmentVariableW(L"RWR_TEST_PROFILE", L"C:\\Users\\Example") != 0,
        "set path-expansion test environment variable");
    std::wstring expandedPath;
    require(expandEnvironmentPath(L"%RWR_TEST_PROFILE%\\Music", expandedPath) &&
        expandedPath == L"C:\\Users\\Example\\Music",
        "environment variables expand in configured music paths");
    require(SetEnvironmentVariableW(L"RWR_TEST_PROFILE", nullptr) != 0,
        "clear path-expansion test environment variable");
    TsmHost testHost{};
    testHost.exeModule = GetModuleHandleW(nullptr);
    testHost.exeBase = reinterpret_cast<unsigned char*>(0x10000000);
    host = &testHost;
    HMODULE dll = LoadLibraryW(L"RedWolfRadio.dll");
    require(dll != nullptr, "built DLL loads with Windows dependencies");
    auto version = reinterpret_cast<unsigned (*)()>(GetProcAddress(dll, "TsmPluginApiVersion"));
    auto init = reinterpret_cast<int (*)(const TsmHost*, TsmPluginInfo*)>(GetProcAddress(dll, "TsmPluginInit"));
    require(version && init && GetProcAddress(dll, "TsmPluginStart"), "built DLL exports complete lifecycle");
    require(version() == 4 && init(nullptr, nullptr) != 0, "built DLL ABI query and invalid-host rejection");
    FreeLibrary(dll);
    originals[Init] = reinterpret_cast<void*>(fakeInit); originals[Load] = reinterpret_cast<void*>(fakeLoad);
    originals[Play] = reinterpret_cast<void*>(fakePlay); originals[Pause] = reinterpret_cast<void*>(fakePause);
    originals[Resume] = reinterpret_cast<void*>(fakeResume); originals[Stop] = reinterpret_cast<void*>(fakeStop);
    originals[Volume] = reinterpret_cast<void*>(fakeVolume); originals[Playing] = reinterpret_cast<void*>(fakePlaying);
    QueryPerformanceCounter(&epoch); QueryPerformanceFrequency(&frequency);
    require(TsmPluginApiVersion() == 4, "API 4 export");
    TsmPluginInfo info{}; require(TsmPluginInit(nullptr, &info) != 0, "reject null host");
    TsmHost bad{}; require(TsmPluginInit(&bad, &info) != 0, "reject invalid host ABI");
    for (bool enabled : {false, true}) {
        recording.store(enabled);
        prepare(); onInitialize(); checkOutputError();
        prepare(); onPause(); checkOutputError();
        prepare(); onResume(); checkOutputError();
        prepare(); onStop(); checkOutputError();
        char path[] = "music/Arbitrary Name.xwma";
        for (char* p : {path, static_cast<char*>(nullptr), reinterpret_cast<char*>(1)}) {
            expectedPath = p; prepare(); onLoad(p); checkOutputError();
        }
        {
            manager.catalog.clear();
            manager.playBag.clear();
            manager.config.originalsMode = OriginalsMode::None;
            manager.runtimeEnabled = true;
            manager.silenceTrackReady = true;
            manager.silenceTrackPath = "C:\\Users\\Example\\Music\\MusicTrace-silent.wav";
            manager.silenceUntil = 0;
            manager.trackSelectionDirty = false;
            redirectReady.store(true, std::memory_order_relaxed);
            char gamePath[] = "music/any_original.xwma";
            expectedPath = const_cast<char*>(kSentinelPath);
            int loadsBefore = called[Load];
            prepare(); onLoad(gamePath); checkOutputError();
            require(called[Load] == loadsBefore + 1, "originals-none fallback rewrites load to sentinel");
            require(strcmp(manager.silenceTrackPath.c_str(), "C:\\Users\\Example\\Music\\MusicTrace-silent.wav") == 0, "test silence cache path preserved");
            require(manager.silenceUntil > 0, "no-originals cooldown starts");
            expectedStatus = 0;
            prepare(); require(onPlaying() == 1, "no-originals window forces a short playing hold");
            checkOutputError();
            Sleep(1600);
            expectedStatus = 1;
            prepare(); require(onPlaying() == 1, "cooldown expiry returns observed status");
            checkOutputError();
            manager.runtimeEnabled = false;
            manager.silenceTrackReady = false;
            expectedPath = gamePath;
            loadsBefore = called[Load];
            prepare(); onLoad(gamePath); checkOutputError();
            require(called[Load] == loadsBefore + 1, "runtime-disabled mode forwards original path");
            manager.runtimeEnabled = true;
            manager.silenceTrackReady = true;
            manager.config.originalsMode = OriginalsMode::All;
        }
        for (bool b : {false, true}) for (int value : {-1, 0, 1234567}) {
            expectedBool = b; expectedInt = value; prepare(); onPlay(b, value); checkOutputError();
        }
        for (uint32_t bits : {0u, 0x80000000u, 0x3f800000u, 0x3e99999au, 0x7fc01234u}) {
            expectedFloat = bits; float value; memcpy(&value, &bits, 4);
            prepare(); onVolume(value); checkOutputError();
        }
        for (int status : {0, 1, 1, -1, 17}) {
            expectedStatus = status; prepare(); require(onPlaying() == status, "status result unchanged"); checkOutputError();
        }
    }
    require(called[Init] == 2 && called[Pause] == 2 && called[Resume] == 2 && called[Stop] == 2, "exactly-once void calls");
    require(called[Load] == 10 && called[Play] == 12 && called[Volume] == 10 && called[Playing] == 14, "exactly-once argument calls");
    require(head > 0 && tail == 0, "events queued without file I/O");
    bool invalidSeen = false, nullSeen = false;
    for (unsigned i = 0; i < head; ++i) if (queue[i].kind == Load && queue[i].phase == 'B') {
        invalidSeen |= (queue[i].bits & 1) != 0; nullSeen |= (queue[i].bits & 4) != 0;
    }
    require(invalidSeen && nullSeen, "bad diagnostic paths bounded and flagged");
    unsigned beforeHead = head; auto beforeDrop = dropped.load();
    AcquireSRWLockExclusive(&queueLock); prepare(); onStop(); checkOutputError(); ReleaseSRWLockExclusive(&queueLock);
    require(head == beforeHead && dropped.load() == beforeDrop + 2, "contended queue drops without blocking engine call");
    while (head - tail < kCapacity) { Event e{}; enqueue(e); }
    beforeDrop = dropped.load(); prepare(); onStop(); checkOutputError();
    require(dropped.load() == beforeDrop + 2, "full queue preserves forwarding");
    originals[Playing] = reinterpret_cast<void*>(concurrentPlaying);
    auto beforeCount = counts[Playing].load();
    HANDLE threads[4];
    for (auto& thread : threads) { thread = CreateThread(nullptr, 0, poll, nullptr, 0, nullptr); require(thread != nullptr, "create test thread"); }
    WaitForMultipleObjects(4, threads, TRUE, INFINITE);
    for (auto thread : threads) CloseHandle(thread);
    require(counts[Playing].load() == beforeCount + 40000, "concurrent poll counts exact");
    char escaped[100]; escapePath("a\tb\n\\c", escaped, sizeof(escaped));
    require(strcmp(escaped, "a\\x09b\\x0a\\x5cc") == 0, "TSV paths escaped");
    require(checkModule(GetModuleHandleW(nullptr), kExeHash) == false, "wrong module hash rejected");
    manager.catalog.clear();
    for (const char* id : {"external:a.wav", "external:b.wav", "external:c.wav"}) {
        Track track; track.id = id; track.nativePlayable = true; track.absoluteUtf8 = "C:\\cache\\track.wav";
        manager.catalog.push_back(track);
    }
    manager.bannedTrackIds.clear(); manager.lastPlayedId.clear(); manager.trackSelectionDirty = true;
    std::set<std::string> firstCycle, secondCycle;
    std::string boundaryLast;
    for (int i = 0; i < 6; ++i) {
        const Track* picked = pickTrackLocked();
        require(picked != nullptr, "shuffle bag remains available across cycles");
        if (i < 3) firstCycle.insert(picked->id); else secondCycle.insert(picked->id);
        if (i == 2) boundaryLast = picked->id;
        if (i == 3) require(picked->id != boundaryLast, "cycle reshuffle prevents a boundary repeat");
        manager.lastPlayedId = picked->id;
    }
    require(firstCycle.size() == 3 && secondCycle.size() == 3, "each shuffled cycle contains every eligible track exactly once");
    manager.catalog.clear(); manager.playBag.clear(); manager.trackSelectionDirty = true;
    redirectReady.store(true);
    originalPathResolver = fakePath;
    strcpy_s(currentRedirectPath, sizeof(currentRedirectPath), "C:\\cache\\canonical.wav");
    char resolved[256]{};
    prepare(); onResolvePath(resolved, kSentinelPath);
    require(GetLastError() == 1234, "redirected path preserves incoming last-error");
    require(strcmp(resolved, currentRedirectPath) == 0, "sentinel maps to selected canonical cache path");
    char ordinary[] = "music/ordinary.xwma"; expectedSource = ordinary; resolved[0] = 0;
    prepare(); onResolvePath(resolved, ordinary); checkOutputError();
    require(strcmp(resolved, "ORIGINAL") == 0, "ordinary path uses original resolver");
    // A healthy cached WAV clears the engine loading flag and must continue
    // past the watchdog deadline. A genuinely stuck load keeps the flag set;
    // the wrapper stops only that case and returns false to the scheduler.
    head = tail = 0; recording.store(true);
    originals[Stop] = reinterpret_cast<void*>(fakeTimeoutStop);
    originals[Playing] = reinterpret_cast<void*>(fakePlaying);
    volatile char fakeLoadingFlag = 0; engineLoadingFlag = &fakeLoadingFlag;
    expectedStatus = 1; activeTrackRecovery.store(1); recoveryDeadlineTicks.store(0);
    int stopsBeforeTimeout = called[Stop];
    prepare(); require(onPlaying() == 1, "healthy cached WAV continues after watchdog deadline");
    require(GetLastError() == 5678, "healthy playback preserves status-call last-error");
    require(called[Stop] == stopsBeforeTimeout && activeTrackRecovery.load() == 0 && fakeLoadingFlag == 0,
        "watchdog disarms without stopping a track whose loading flag cleared");

    head = tail = 0;
    fakeLoadingFlag = 1;
    expectedStatus = 1; activeTrackRecovery.store(1); recoveryDeadlineTicks.store(0);
    stopsBeforeTimeout = called[Stop];
    prepare(); require(onPlaying() == 0, "expired failed-track recovery reports stopped to scheduler");
    require(GetLastError() == 5678, "timeout recovery preserves status-call last-error");
    require(called[Stop] == stopsBeforeTimeout + 1 && activeTrackRecovery.load() == 0 && fakeLoadingFlag == 0,
        "expired failed-track recovery clears loading state and stops exactly once");
    bool timeoutSeen = false;
    for (unsigned i = tail; i < head; ++i) {
        const Event& event = queue[i % kCapacity];
        timeoutSeen |= event.kind == Stop && event.phase == 'T' && event.arg0 == 1 && event.arg1 == 1;
    }
    require(timeoutSeen, "timeout recovery is recorded in trace queue");
    originals[Stop] = reinterpret_cast<void*>(fakeStop);
    // Execute a synthetic function with the same prologue shape to prove that
    // the private continuation preserves a RIP-relative load before jumping
    // back to the original function body.
    auto* synthetic = static_cast<unsigned char*>(VirtualAlloc(nullptr, 4096,
        MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    require(synthetic != nullptr, "allocate executable relocation fixture");
    unsigned char* target = synthetic + 0x100;
    unsigned char* continuation = synthetic + 0x200;
    auto* value = reinterpret_cast<uint64_t*>(synthetic + 0x300);
    *value = 0x1122334455667788ull;
    static const unsigned char syntheticPrefix[kPathStolenBytes] = {
        0x40,0x55,0x41,0x56,0x48,0x81,0xec,0x18,0x02,0x00,0x00,0x48,0x8b,0x05,0,0,0,0
    };
    memcpy(target, syntheticPrefix, sizeof(syntheticPrefix));
    const int64_t sourceDelta = reinterpret_cast<unsigned char*>(value) - (target + kPathStolenBytes);
    require(sourceDelta >= INT32_MIN && sourceDelta <= INT32_MAX, "synthetic source disp32 fits");
    const int32_t sourceDisp32 = static_cast<int32_t>(sourceDelta);
    memcpy(target + 14, &sourceDisp32, sizeof(sourceDisp32));
    static const unsigned char syntheticTail[] = {
        0x48,0x81,0xc4,0x18,0x02,0x00,0x00, // add rsp, 0x218
        0x41,0x5e,                           // pop r14
        0x5d,                                // pop rbp
        0xc3                                 // ret
    };
    memcpy(target + kPathStolenBytes, syntheticTail, sizeof(syntheticTail));
    require(buildRelocatedPathContinuation(continuation, 64, target, target),
        "build relocated RIP-relative continuation");
    require(FlushInstructionCache(GetCurrentProcess(), synthetic, 4096) != 0,
        "flush executable relocation fixture");
    using SyntheticFn = uint64_t (*)();
    require(reinterpret_cast<SyntheticFn>(continuation)() == *value,
        "relocated continuation reads original RIP-relative target and returns safely");
    require(!buildRelocatedPathContinuation(continuation, kPathContinuationBytes - 1, target, target),
        "undersized continuation buffer rejected");
    unsigned char wrongPrefix[kPathStolenBytes]; memcpy(wrongPrefix, target, sizeof(wrongPrefix));
    wrongPrefix[12] ^= 1;
    require(!buildRelocatedPathContinuation(continuation, 64, wrongPrefix, target),
        "unexpected RIP-relative instruction rejected");
    VirtualFree(synthetic, 0, MEM_RELEASE);
    // Exercise the real asynchronous file writer in this test executable only.
    recording.store(false); head = tail = 0;
    selfModule = GetModuleHandleW(nullptr);
    require(startLog(), "asynchronous trace file can be opened");
    recording.store(true); prepare(); onStop(); checkOutputError();
    recording.store(false); closeLogBeforeHooks();
    require(tail == head && written > 500, "writer drains queued events and header");
    puts("PASS: environment-path expansion, DLL loading/exports, ABI rejection, exactly-once forwarding, reshuffled complete playlist cycles without boundary repeats, six-second failed-track recovery, scoped cache-path redirection, executable RIP-relative continuation relocation, args/return/last-error preservation, invalid paths, bounded queue, 40,000 concurrent status calls, and asynchronous runtime logging.");
    return 0;
}
