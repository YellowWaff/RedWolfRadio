// SPDX-License-Identifier: GPL-3.0-only
// Exercises the real scheduler and control coordinator with stand-in source
// voices. No game process is injected and no game or music file is modified.
#include "../src/music_trace.cpp"
#include <map>

static void require(bool value, const char* message) {
    if (!value) { fprintf(stderr, "FAIL: %s\n", message); exit(1); }
}

static int engineStops = 0;
static int enginePlays = 0;
static bool playedLoop = false;
static int playedDelay = 0;
static int playingResult = 0;
static int volumePauses = 0;
static int volumeResumes = 0;
static int voiceStarts = 0;
static int voiceStops = 0;
static HRESULT voiceResult = S_OK;
static void* expectedVoice = nullptr;
static std::map<std::string, std::string> iniValues;

static void fakeEngineStop() { ++engineStops; }
static void fakeEnginePlay(bool loop, int delay) {
    ++enginePlays;
    playedLoop = loop;
    playedDelay = delay;
}
static int fakeEnginePlaying() {
    SetLastError(3129);
    return playingResult;
}
static void fakeVolumePause() { ++volumePauses; }
static void fakeVolumeResume() { ++volumeResumes; }
static HRESULT WINAPI fakeVoiceStart(void* voice, UINT32 flags, UINT32 operation) {
    require(voice == expectedVoice && flags == 0 && operation == 0,
        "voice Start receives the expected source with immediate operation");
    ++voiceStarts;
    return voiceResult;
}
static HRESULT WINAPI fakeVoiceStop(void* voice, UINT32 flags, UINT32 operation) {
    require(voice == expectedVoice && flags == 0 && operation == 0,
        "voice Stop preserves buffers with zero flags and immediate operation");
    ++voiceStops;
    return voiceResult;
}
static int fakeConfigString(const char*, const char* section, const char* key,
    char* buffer, int capacity, const char* fallback) {
    const auto found = iniValues.find(std::string(section) + "." + key);
    return snprintf(buffer, static_cast<size_t>(capacity), "%s",
        found == iniValues.end() ? fallback : found->second.c_str());
}

struct FakeVoice { void** vtable; };
static void* voiceVtable[21]{};
static FakeVoice sourceVoice{voiceVtable};
static FakeVoice replacementVoice{voiceVtable};
static unsigned char* engineMemory = nullptr;
static volatile char loading = 0;
static TsmHost fakeHost{};

static void installVoice(FakeVoice* voice) {
    void* value = voice;
    memcpy(engineMemory + rwr::kMusicSourceVoiceRva, &value, sizeof(value));
    expectedVoice = voice;
}

static void resetFixture() {
    manager = ManagerState{};
    manager.runtimeEnabled = true;
    manager.trackSelectionDirty = false;
    manager.config.prefetchTracks = 2;
    for (size_t i = 0; i < 6; ++i) {
        Track track;
        track.id = "external:test" + std::to_string(i) + ".wav";
        track.relativeUtf8 = "test" + std::to_string(i) + ".wav";
        track.absoluteWide = L"C:\\Source\\test" + std::to_wstring(i) + L".wav";
        track.preparedWide = L"C:\\Cache\\test" + std::to_wstring(i) + L".wav";
        track.preparedUtf8 = "C:\\Cache\\test" + std::to_string(i) + ".wav";
        track.format = TrackFormat::Wav;
        track.nativePlayable = true;
        track.preparation = PreparationState::Ready;
        manager.catalog.push_back(track);
        manager.playBag.push_back(i);
    }
    engineStops = volumePauses = volumeResumes = voiceStarts = voiceStops = 0;
    enginePlays = playedDelay = playingResult = 0;
    playedLoop = false;
    voiceResult = S_OK;
    loading = 0;
    engineLoadingFlag = &loading;
    installVoice(&sourceVoice);
    host = &fakeHost;
    originals[Stop] = reinterpret_cast<void*>(fakeEngineStop);
    originals[Play] = reinterpret_cast<void*>(fakeEnginePlay);
    originals[Playing] = reinterpret_cast<void*>(fakeEnginePlaying);
    originals[Pause] = reinterpret_cast<void*>(fakeVolumePause);
    originals[Resume] = reinterpret_cast<void*>(fakeVolumeResume);
    pendingTrackRecovery.store(0);
    activeTrackRecovery.store(0);
    recoveryDeadlineTicks.store(0);
    recording.store(false);
    musicThreadId.store(0);
}

static size_t selectAndPlay() {
    const Track* track = pickTrackLocked();
    require(track != nullptr, "scheduler selects an available fixture track");
    const size_t index = static_cast<size_t>(track - manager.catalog.data());
    manager.activeTrackPending = false;
    manager.activeTrackPlaying = true;
    recordPlayingHistoryLocked();
    return index;
}

static void testHotkeyConfigIsolation() {
    iniValues.clear();
    Config config;
    config.originalsMode = OriginalsMode::Selected;
    config.excludedIds = {"original:test.xwma"};
    config.externalPath = L"D:\\Music";
    config.cacheMaxSizeMiB = 789;
    iniValues["Hotkeys.Next"] = "Ctrl++Right";
    parseHotkeyConfig(config);
    require(config.hotkeysEnabled && config.hotkeys[0].key == 0 &&
        config.hotkeys[1].key == VK_LEFT && config.hotkeys[2].key == VK_SPACE,
        "bad binding disables only its own shortcut");
    require(config.hotkeyWarnings.size() == 1, "bad binding produces a warning");
    require(config.originalsMode == OriginalsMode::Selected &&
        config.excludedIds == std::vector<std::string>{"original:test.xwma"} &&
        config.externalPath == L"D:\\Music" && config.cacheMaxSizeMiB == 789,
        "bad shortcut cannot change original exclusions, music path, or cache settings");
    iniValues["Hotkeys.Next"] = "Ctrl+N";
    iniValues["Hotkeys.Previous"] = "n + Control";
    parseHotkeyConfig(config);
    require(config.hotkeys[0].key == 0 && config.hotkeys[1].key == 0 &&
        config.hotkeys[2].key == VK_SPACE && config.hotkeyWarnings.size() == 1,
        "equivalent duplicate bindings both disable without affecting play/pause");
    iniValues["Hotkeys.Next"] = "None";
    iniValues["Hotkeys.Previous"] = "None";
    parseHotkeyConfig(config);
    require(config.hotkeyWarnings.empty(), "two disabled shortcuts are not duplicates");
    iniValues["Hotkeys.Enabled"] = "possibly";
    parseHotkeyConfig(config);
    require(!config.hotkeysEnabled && config.hotkeyWarnings.size() == 1,
        "invalid global shortcut setting fails closed with a warning");
    iniValues.clear();
}

static void testHistoryAndCache() {
    resetFixture();
    require(selectAndPlay() == 0 && selectAndPlay() == 1 && selectAndPlay() == 2,
        "initial playback follows prepared shuffle order");
    require(manager.history == std::vector<size_t>({0, 1, 2}) && manager.historyCursor == 2,
        "only played tracks enter history in playback order");
    recordPlayingHistoryLocked();
    require(manager.history.size() == 3, "repeated playing status does not duplicate history");
    const size_t bagCursor = manager.bagCursor;
    manager.navigationPending = true;
    manager.navigationHistoryCursor = 1;
    require(selectAndPlay() == 1 && manager.historyCursor == 1 &&
        manager.history.size() == 3 && manager.bagCursor == bagCursor,
        "previous selects history without consuming shuffled tracks");
    require(cachePathProtectedLocked(cachePathKey(manager.catalog[0].preparedWide)) &&
        cachePathProtectedLocked(cachePathKey(manager.catalog[1].preparedWide)) &&
        cachePathProtectedLocked(cachePathKey(manager.catalog[2].preparedWide)),
        "previous, current, and forward history are protected from eviction");
    require(!cachePathProtectedLocked(cachePathKey(manager.catalog[5].preparedWide)),
        "history protection does not retain unrelated tracks outside prefetch");
    require(selectAndPlay() == 2 && manager.historyCursor == 2 && manager.bagCursor == bagCursor,
        "next after previous walks existing forward history");
    require(selectAndPlay() == 3 && manager.bagCursor == bagCursor + 1,
        "at newest history entry playback resumes the shuffle bag");

    require(selectAndPlay() == 4, "advance beyond retained history window");
    require(!cachePathProtectedLocked(cachePathKey(manager.catalog[0].preparedWide)),
        "old history beyond three prior tracks can be evicted");
    for (size_t index : {1u, 2u, 3u, 4u}) {
        require(cachePathProtectedLocked(cachePathKey(manager.catalog[index].preparedWide)),
            "current and three prior tracks are retained by default");
    }
    manager.config.historyTracks = 0;
    require(!cachePathProtectedLocked(cachePathKey(manager.catalog[3].preparedWide)) &&
        cachePathProtectedLocked(cachePathKey(manager.catalog[4].preparedWide)),
        "zero retained history releases prior tracks while retaining current song");
    manager.config.historyTracks = 64;
    require(cachePathProtectedLocked(cachePathKey(manager.catalog[0].preparedWide)),
        "larger history setting retains older available songs");
    manager.config.historyTracks = 3;
    const auto protectedPaths = protectedCachePathsSnapshot();
    require(protectedPaths.count(cachePathKey(manager.catalog[0].preparedWide)) == 0 &&
        protectedPaths.count(cachePathKey(manager.catalog[1].preparedWide)) == 1 &&
        protectedPaths.count(cachePathKey(manager.catalog[4].preparedWide)) == 1,
        "cache eviction snapshot uses the same retention policy as the locked check");

    manager.navigationPending = true;
    manager.navigationHistoryCursor = 0;
    manager.catalog[0].nativePlayable = false;
    manager.catalog[0].preparation = PreparationState::Pending;
    manager.catalog[5].nativePlayable = false;
    manager.catalog[5].preparation = PreparationState::Pending;
    require(nextPrefetchCandidateLocked() == 0,
        "evicted requested history is prepared ahead of normal prefetch");
    const size_t waitingCursor = manager.bagCursor;
    require(pickTrackLocked() == nullptr && manager.bagCursor == waitingCursor,
        "unprepared history cannot silently select an unrelated ready song");
    manager.catalog[0].preparation = PreparationState::Rejected;
    manager.catalog[0].nativePlayable = false;
    manager.catalog[5].nativePlayable = true;
    manager.catalog[5].preparation = PreparationState::Ready;
    require(pickTrackLocked() != nullptr,
        "failed historical conversion cannot permanently trap scheduling");

    resetFixture();
    for (size_t i = 0; i < kHistoryLimit + 10; ++i) {
        activateTrackLocked(i % manager.catalog.size());
        recordPlayingHistoryLocked();
    }
    require(manager.history.size() == kHistoryLimit && manager.historyCursor == kHistoryLimit - 1 &&
        manager.history.back() == (kHistoryLimit + 9) % manager.catalog.size(),
        "history remains bounded while retaining most recent playback");
}

static void testPreviousTiming() {
    resetFixture();
    for (size_t i = 0; i < 4; ++i) require(selectAndPlay() == i, "prepare four-song history");
    const size_t bagCursor = manager.bagCursor;
    require(processMusicControls(rwr::MusicAction::Previous, 1, false, 1000) == 0 &&
        selectAndPlay() == 3, "first previous restarts fourth song");
    require(processMusicControls(rwr::MusicAction::Previous, 1, false, 1100) == 0 &&
        selectAndPlay() == 2, "second quick previous moves to third song");
    require(processMusicControls(rwr::MusicAction::Previous, 1, false, 1200) == 0 &&
        selectAndPlay() == 1, "third quick previous moves to second song");
    require(processMusicControls(rwr::MusicAction::Previous, 1, false, 2301) == 0 &&
        selectAndPlay() == 1, "previous after more than a second restarts current song");
    require(processMusicControls(rwr::MusicAction::Previous, 1, false, 2400) == 0 &&
        selectAndPlay() == 0, "new quick sequence can move to first song");
    require(processMusicControls(rwr::MusicAction::Previous, 1, false, 2500) == 0 &&
        selectAndPlay() == 0, "further previous presses stop at beginning of available history");
    require(manager.bagCursor == bagCursor && manager.history.size() == 4,
        "previous sequence changes neither shuffle cursor nor history size");

    resetFixture();
    selectAndPlay(); selectAndPlay(); selectAndPlay();
    processMusicControls(rwr::MusicAction::Previous, 1, false, 1000);
    require(manager.awaitingNavigationLoad, "first restart is awaiting game load");
    processMusicControls(rwr::MusicAction::Previous, 0, false, 1100);
    require(selectAndPlay() == 1,
        "quick second previous before restarted load retargets preceding song");

    resetFixture();
    selectAndPlay(); selectAndPlay(); selectAndPlay();
    processMusicControls(rwr::MusicAction::Previous, 1, false, 1000);
    const Track* restarting = pickTrackLocked();
    require(restarting && restarting->id == manager.catalog[2].id,
        "restart selection begins loading current song");
    loading = 1;
    const int stoppedBeforeLoad = engineStops;
    processMusicControls(rwr::MusicAction::Previous, 1, true, 1100);
    require(engineStops == stoppedBeforeLoad && manager.navigationPending,
        "quick previous during restarted load is retained without stopping loader");
    loading = 0;
    manager.activeTrackPending = false;
    manager.activeTrackPlaying = true;
    recordPlayingHistoryLocked();
    require(processMusicControls(rwr::MusicAction::None, 1, false, 1200) == 0 &&
        selectAndPlay() == 1, "quick previous applies to prior song when restarted load finishes");
}

static void testPlayPause() {
    resetFixture();
    selectAndPlay();
    const auto active = manager.activeTrackId;
    require(processMusicControls(rwr::MusicAction::PlayPause, 1, false) == 1 &&
        manager.userPaused && manager.voicePaused && voiceStops == 1,
        "play/pause stops source voice while preserving playing status");
    require(manager.activeTrackId == active && manager.history.size() == 1 && engineStops == 0 &&
        volumePauses == 0 && volumeResumes == 0,
        "pause preserves active track and never calls destructive stop or volume-only exports");
    require(processMusicControls(rwr::MusicAction::None, 1, false) == 1 && voiceStops == 1,
        "paused status poll does not repeatedly stop voice");
    require(processMusicControls(rwr::MusicAction::PlayPause, 1, false) == 1 &&
        !manager.userPaused && !manager.voicePaused && voiceStarts == 1,
        "second play/pause resumes the same source voice");

    resetFixture();
    selectAndPlay();
    loading = 1;
    processMusicControls(rwr::MusicAction::PlayPause, 1, true);
    require(manager.userPaused && !manager.voicePaused && voiceStops == 0 && voiceStarts == 0,
        "pause request during loading is retained without touching the voice");
    loading = 0;
    require(processMusicControls(rwr::MusicAction::None, 1, false) == 1 &&
        manager.voicePaused && voiceStops == 1,
        "deferred pause applies once loading finishes");

    resetFixture();
    selectAndPlay();
    loading = 1;
    processMusicControls(rwr::MusicAction::PlayPause, 1, true);
    processMusicControls(rwr::MusicAction::PlayPause, 1, true);
    loading = 0;
    processMusicControls(rwr::MusicAction::None, 1, false);
    require(!manager.userPaused && !manager.voicePaused && voiceStops == 0 && voiceStarts == 0,
        "two pause toggles during loading cancel without a spurious voice call");

    resetFixture();
    selectAndPlay();
    voiceResult = E_FAIL;
    processMusicControls(rwr::MusicAction::PlayPause, 1, false);
    require(!manager.userPaused && !manager.voicePaused && voiceStops == 1,
        "failed voice pause restores requested state to actual playing state");
    processMusicControls(rwr::MusicAction::None, 1, false);
    require(voiceStops == 1, "failed voice command is not retried on every status poll");

    resetFixture();
    selectAndPlay();
    installVoice(nullptr);
    processMusicControls(rwr::MusicAction::PlayPause, 1, false);
    require(!manager.userPaused && !manager.voicePaused && voiceStops == 0,
        "missing voice safely rejects pause without dereferencing a null source");

    resetFixture();
    selectAndPlay();
    processMusicControls(rwr::MusicAction::PlayPause, 1, false);
    installVoice(&replacementVoice);
    processMusicControls(rwr::MusicAction::PlayPause, 1, false);
    require(manager.userPaused && manager.voicePaused && voiceStarts == 0,
        "resume cannot accidentally start a replaced source voice");
}

static void testNavigation() {
    resetFixture();
    require(selectAndPlay() == 0, "first navigation fixture track");
    require(processMusicControls(rwr::MusicAction::Next, 1, false) == 0 && engineStops == 1,
        "next stops current track when next selection is ready");
    require(manager.awaitingNavigationLoad && manager.activeTrackId.empty(),
        "next transitions to awaiting-load state and clears active track");
    require(selectAndPlay() == 1 && manager.history == std::vector<size_t>({0, 1}),
        "next song enters history after beginning playback");
    const size_t bagCursor = manager.bagCursor;
    require(processMusicControls(rwr::MusicAction::Previous, 1, false, 1000) == 0 && engineStops == 2,
        "first previous press triggers current-track restart");
    require(selectAndPlay() == 1 && manager.historyCursor == 1 && manager.bagCursor == bagCursor,
        "first previous press restarts current song without consuming shuffle bag");
    require(processMusicControls(rwr::MusicAction::Previous, 1, false, 1100) == 0 && engineStops == 3,
        "quick second previous press triggers a preceding-track transition");
    require(selectAndPlay() == 0 && manager.historyCursor == 0 && manager.bagCursor == bagCursor,
        "previous replays preceding history without consuming shuffle bag");
    require(processMusicControls(rwr::MusicAction::Next, 1, false) == 0,
        "next can return from previous history");
    require(selectAndPlay() == 1 && manager.bagCursor == bagCursor && manager.history.size() == 2,
        "next retraces forward history without duplicating it");

    resetFixture();
    selectAndPlay();
    require(processMusicControls(rwr::MusicAction::Previous, 1, false, 1000) == 0,
        "previous at beginning restarts current track");
    require(selectAndPlay() == 0 && manager.history.size() == 1 && manager.bagCursor == 1,
        "restart neither appends history nor consumes next track");

    resetFixture();
    selectAndPlay();
    loading = 1;
    processMusicControls(rwr::MusicAction::Next, 1, true);
    require(manager.navigationPending && engineStops == 0,
        "navigation during async loading is deferred without stopping loader");
    loading = 0;
    require(processMusicControls(rwr::MusicAction::None, 1, false) == 0 && engineStops == 1,
        "deferred navigation applies on a safe subsequent status poll");

    resetFixture();
    selectAndPlay();
    for (size_t i = 1; i < manager.catalog.size(); ++i) {
        manager.catalog[i].nativePlayable = false;
        manager.catalog[i].preparation = PreparationState::Pending;
    }
    require(processMusicControls(rwr::MusicAction::Next, 1, false) == 1 &&
        manager.navigationPending && engineStops == 0 && manager.activeTrackPlaying,
        "next keeps current song playing until another track is prepared");
    manager.catalog[1].nativePlayable = true;
    manager.catalog[1].preparation = PreparationState::Ready;
    require(processMusicControls(rwr::MusicAction::None, 1, false) == 0 && engineStops == 1,
        "waiting next advances once worker has prepared a track");

    resetFixture();
    selectAndPlay();
    processMusicControls(rwr::MusicAction::PlayPause, 1, false);
    require(processMusicControls(rwr::MusicAction::Next, 1, false) == 0 && manager.userPaused,
        "next while paused preserves the requested pause state");
    require(selectAndPlay() == 1 && manager.userPaused, "next selection retains pause preference");
    installVoice(&replacementVoice);
    processMusicControls(rwr::MusicAction::None, 1, false);
    require(manager.voicePaused && voiceStops == 2 && voiceStarts == 0,
        "new source voice receives pause after a paused transition");
    processMusicControls(rwr::MusicAction::PlayPause, 1, false);
    require(!manager.userPaused && !manager.voicePaused && voiceStarts == 1,
        "resume after navigation targets the new source voice");
}

static size_t selectPaused(bool loop, int delay) {
    const Track* track = pickTrackLocked();
    require(track != nullptr, "select a track while keeping playback paused");
    const size_t index = static_cast<size_t>(track - manager.catalog.data());
    pendingTrackRecovery.store(1);
    SetLastError(1735);
    onPlay(loop, delay);
    require(GetLastError() == 1735, "deferring native Play preserves caller last-error");
    require(manager.deferredPlay && manager.activeTrackPending && !manager.activeTrackPlaying &&
        manager.userPaused && enginePlays == 0,
        "paused selection defers native Play entirely, without an audible start");
    require(pendingTrackRecovery.load() == 1 && activeTrackRecovery.load() == 0,
        "load recovery waits for actual native Play instead of timing a paused selection");
    return index;
}

static void testDeferredPlay() {
    resetFixture();
    manager.config.hotkeysEnabled = false;
    musicThreadId.store(GetCurrentThreadId());
    manager.userPaused = true;
    require(selectPaused(true, 174) == 0 && manager.history == std::vector<size_t>{0},
        "first paused selection is navigable history even before native playback");
    const std::string selected = manager.activeTrackId;
    for (int i = 0; i < 8; ++i) {
        require(onPlaying() == 1 && GetLastError() == 3129,
            "native stopped status is held while deferred and preserves native last-error");
    }
    require(manager.deferredPlay && manager.activeTrackPending && !manager.activeTrackPlaying &&
        manager.activeTrackId == selected && manager.history.size() == 1 &&
        manager.bannedTrackIds.empty() && manager.rejected == 0 && engineStops == 0,
        "status polls neither fail a deferred song nor duplicate history nor declare actual playback");
    require(processMusicControls(rwr::MusicAction::PlayPause, 1, false) == 1 && enginePlays == 1 &&
        playedLoop && playedDelay == 174 && !manager.deferredPlay && !manager.userPaused,
        "resume calls native Play once using the saved bool and integer arguments");
    require(pendingTrackRecovery.load() == 0 && activeTrackRecovery.load() == 1 &&
        recoveryDeadlineTicks.load() > 0,
        "resuming deferred track activates normal recovery timing");
    playingResult = 1;
    require(onPlaying() == 1 && manager.activeTrackPlaying && !manager.activeTrackPending &&
        manager.history.size() == 1 && enginePlays == 1,
        "successful resumed status confirms playback without duplicate native Play or history");

    resetFixture();
    manager.config.hotkeysEnabled = false;
    musicThreadId.store(GetCurrentThreadId());
    manager.userPaused = true;
    selectPaused(false, -29);
    loading = 1;
    require(processMusicControls(rwr::MusicAction::PlayPause, 1, true) == 1 &&
        !manager.userPaused && manager.deferredPlay && enginePlays == 0,
        "resume during loading waits instead of starting a partially loaded selection");
    require(onPlaying() == 1 && enginePlays == 0 && manager.deferredPlay,
        "status polls keep requested resume deferred while loading remains active");
    loading = 0;
    require(onPlaying() == 1 && enginePlays == 1 && !playedLoop && playedDelay == -29 &&
        !manager.deferredPlay && activeTrackRecovery.load() == 1,
        "first safe game-thread status poll starts deferred playback with exact arguments");
    playingResult = 1;
    require(onPlaying() == 1 && enginePlays == 1 && manager.history.size() == 1,
        "subsequent polls cannot start the same deferred request again");

    resetFixture();
    manager.userPaused = true;
    selectPaused(false, 0);
    onStop();
    require(!manager.deferredPlay && !manager.userPaused && manager.activeTrackId.empty() &&
        pendingTrackRecovery.load() == 0 && enginePlays == 0 && engineStops == 1,
        "game Stop cancels deferred playback and recovery without accidentally starting it");
    manager.userPaused = true;
    selectPaused(false, 7);
    activateTrackLocked(5);
    require(!manager.deferredPlay,
        "activating a different selection clears the previous deferred Play request");
}

static void testDeferredNavigation() {
    resetFixture();
    manager.config.hotkeysEnabled = false;
    musicThreadId.store(GetCurrentThreadId());
    manager.userPaused = true;
    require(selectPaused(false, 10) == 0, "initial silent navigation selection");
    require(processMusicControls(rwr::MusicAction::Next, 1, false, 1000) == 0 &&
        !manager.deferredPlay && manager.userPaused && engineStops == 1,
        "next cancels deferred current Play but retains user's pause preference");
    require(selectPaused(false, 20) == 1 && onPlaying() == 1,
        "next song remains silent with native Play still deferred");
    require(processMusicControls(rwr::MusicAction::Next, 1, false, 2000) == 0 &&
        selectPaused(true, 30) == 2 && onPlaying() == 1,
        "second next advances while still paused without an audible playback call");
    const size_t bagCursor = manager.bagCursor;
    require(processMusicControls(rwr::MusicAction::Previous, 1, false, 3000) == 0 &&
        selectPaused(false, 40) == 2,
        "first previous restarts a deferred current selection silently");
    require(processMusicControls(rwr::MusicAction::Previous, 1, false, 3100) == 0 &&
        selectPaused(true, 50) == 1,
        "quick second previous moves through paused selection history");
    require(processMusicControls(rwr::MusicAction::Next, 1, false, 3200) == 0 &&
        selectPaused(false, 60) == 2,
        "next walks forward history while continuing to defer native playback");
    require(manager.bagCursor == bagCursor && manager.history == std::vector<size_t>({0, 1, 2}) &&
        manager.historyCursor == 2 && enginePlays == 0 && voiceStarts == 0 && voiceStops == 0,
        "paused navigation preserves shuffle/history and never starts or blips a voice");
    require(processMusicControls(rwr::MusicAction::PlayPause, 1, false, 3300) == 1 &&
        enginePlays == 1 && !playedLoop && playedDelay == 60,
        "resume starts only the last selected track using its own most recent arguments");
    playingResult = 1;
    require(onPlaying() == 1 && manager.history.size() == 3 && manager.bannedTrackIds.empty(),
        "resumed navigation target plays normally without false failures or extra history");
}

static void testFailedOriginalReplay() {
    resetFixture();
    manager.config.hotkeysEnabled = false;
    musicThreadId.store(GetCurrentThreadId());
    manager.catalog[1].id = "original:test1.xwma";
    manager.catalog[1].relativeUtf8 = "music/test1.xwma";
    manager.catalog[1].source = TrackSource::Original;
    manager.catalog[1].format = TrackFormat::Xwma;
    selectAndPlay(); selectAndPlay(); selectAndPlay();
    const size_t bagCursor = manager.bagCursor;
    manager.historyCursor = 0;
    const Track* replay = pickTrackLocked();
    require(replay && replay->id == "original:test1.xwma" && manager.selectedHistoryCursor == 1,
        "forward history can select an original XWMA replay");
    playingResult = 0;
    require(onPlaying() == 0 && manager.historyCursor == 1 &&
        manager.selectedHistoryCursor == kNoHistory && manager.activeTrackId.empty(),
        "failed original replay is marked attempted and clears pending playback state");
    require(manager.bannedTrackIds.empty() && manager.config.originalsMode == OriginalsMode::All,
        "original replay failure does not permanently ban or disable original game music");
    require(selectAndPlay() == 2 && manager.bagCursor == bagCursor && manager.history.size() == 3,
        "history advances past failed original instead of endlessly replaying the same failed entry");
}

int main() {
    engineMemory = static_cast<unsigned char*>(VirtualAlloc(nullptr,
        rwr::kMusicSourceVoiceRva + sizeof(void*), MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    require(engineMemory != nullptr, "allocate fake engine source-voice slot");
    fakeHost.engineModule = engineMemory;
    fakeHost.configString = fakeConfigString;
    fakeHost.exeModule = GetModuleHandleW(nullptr);
    voiceVtable[rwr::kMusicVoiceStartSlot] = reinterpret_cast<void*>(fakeVoiceStart);
    voiceVtable[rwr::kMusicVoiceStopSlot] = reinterpret_cast<void*>(fakeVoiceStop);
    QueryPerformanceCounter(&epoch);
    QueryPerformanceFrequency(&frequency);
    resetFixture();
    testHotkeyConfigIsolation();
    testHistoryAndCache();
    testPlayPause();
    testNavigation();
    testPreviousTiming();
    testDeferredPlay();
    testDeferredNavigation();
    testFailedOriginalReplay();
    host = nullptr;
    engineLoadingFlag = nullptr;
    VirtualFree(engineMemory, 0, MEM_RELEASE);
    puts("All music control integration tests passed.");
}
