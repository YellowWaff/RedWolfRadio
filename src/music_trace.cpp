// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <bcrypt.h>
#include <shlobj.h>
#include <shellapi.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wchar.h>
#include <atomic>
#include <algorithm>
#include <cctype>
#include <cwctype>
#include <random>
#include <set>
#include <string>
#include <utility>
#include <vector>
#include "audio_converter.h"
#include "rml_compat.h"
#include "music_hotkeys.h"
#include "music_command_queue.h"
#include "music_voice_control.h"
#include "track_notification.h"
#include "native_song_popup.h"
#include "native_player_ui.h"
#include "default_ini.h"

namespace {
constexpr char kVersion[] = "1.0.0";
constexpr size_t kNoHistory = static_cast<size_t>(-1);
constexpr size_t kHistoryLimit = 64;
constexpr ULONGLONG kPreviousRepeatMilliseconds = 1000;
enum class TrackFormat { Wav, Flac, Mp3, Xwma, Unknown };
enum class TrackSource { External, PluginLocal, Original };
enum class OriginalsMode { All, Selected, None };
enum class PreparationState { Ready, Pending, Preparing, Rejected };
struct Config {
    bool externalEnabled = true;
    bool externalUseDefault = true;
    std::wstring externalPath;
    bool externalRecursive = true;
    bool pluginLocalEnabled = false;
    bool pluginLocalRecursive = true;
    uint64_t cacheMaxSizeMiB = 4096;
    uint32_t prefetchTracks = 5;
    uint32_t historyTracks = 3;
    bool hotkeysEnabled = true;
    bool showTrackName = false;
    bool nativeSongPopup = true;
    bool displayRussian = false;
    std::array<rwr::Hotkey, 3> hotkeys{{
        {VK_RIGHT, rwr::HotkeyCtrl | rwr::HotkeyShift},
        {VK_LEFT, rwr::HotkeyCtrl | rwr::HotkeyShift},
        {VK_DOWN, rwr::HotkeyCtrl | rwr::HotkeyShift}}};
    std::vector<std::string> hotkeyWarnings;
    OriginalsMode originalsMode = OriginalsMode::All;
    std::vector<std::string> excludedIds;
};
struct Track {
    std::string id;
    TrackSource source = TrackSource::External;
    std::string relativeUtf8;
    std::wstring absoluteWide;
    std::wstring displayName;
    std::wstring preparedWide;
    std::string preparedUtf8;
    TrackFormat format = TrackFormat::Unknown;
    bool nativePlayable = false;
    PreparationState preparation = PreparationState::Pending;
};
struct ManagerState {
    Config config;
    std::vector<Track> catalog;
    std::vector<size_t> playBag;
    std::set<std::string> excludedOriginalIds;
    std::set<std::string> bannedTrackIds;
    bool trackSelectionDirty = true;
    bool runtimeEnabled = false;
    bool configMalformed = false;
    std::string activeTrackId;
    TrackSource activeTrackSource = TrackSource::Original;
    ULONGLONG activeLoadStart = 0;
    bool activeTrackPending = false;
    bool activeTrackPlaying = false;
    std::wstring notificationText;
    ULONGLONG notificationUntil = 0;
    bool hooksInstalled = false;
    ULONGLONG silenceUntil = 0;
    bool silenceTrackReady = false;
    std::string silenceTrackPath;
    std::wstring cacheDirectory;
    size_t bagCursor = 0;
    std::string lastPlayedId;
    std::vector<size_t> history;
    size_t historyCursor = kNoHistory;
    size_t selectedHistoryCursor = kNoHistory;
    bool historyRecorded = false;
    bool navigationPending = false;
    bool awaitingNavigationLoad = false;
    size_t navigationHistoryCursor = kNoHistory;
    bool userPaused = false;
    bool voicePaused = false;
    bool deferredPlay = false;
    bool deferredPlayLoop = false;
    int deferredPlayDelay = 0;
    void* pausedVoice = nullptr;
    rwr::MusicAction pendingNavigationAction = rwr::MusicAction::None;
    ULONGLONG previousPressAt = 0;
    size_t previousSequenceCursor = kNoHistory;
    size_t previousBurstDepth = 0;
    unsigned converted = 0;
    unsigned cacheHits = 0;
    unsigned rejected = 0;
    unsigned nativeXwma = 0;
};
constexpr char kSentinelPath[] = "music/_red_wolf_radio_track";
constexpr char kStorageFolder[] = "RedWolfRadio";
constexpr char kIniName[] = "RedWolfRadio.ini";
constexpr uint32_t kCatalogSchemaVersion = 1;
constexpr char kSilenceCacheFolder[] = "cache\\v1";
constexpr char kSilenceTrackFile[] = "red_wolf_radio_silent.wav";
constexpr uint32_t kSilenceSeconds = 1;
constexpr int64_t kCatalogRecoveryMilliseconds = 6000;
constexpr char kExeHash[] = "296644a9f207d609031fc2ae73fed2dcb34619a1d55a35d1c7b51965ce6841b8";
constexpr char kEngineHash[] = "65fcb6845c3cb7c25a22121f25904a855ea88657e31a3b50a15adac26e231516";
constexpr char kRuntimeHash[] = "761ca3aee81c77034059258310e5eb4e3df14f94d21d030a96b9665879fd19d0";
const TsmHost* host = nullptr;
HMODULE selfModule;
ManagerState manager;
SRWLOCK stateLock = SRWLOCK_INIT;
char currentRedirectPath[2048]{};
HANDLE output = INVALID_HANDLE_VALUE, stopEvent = nullptr, writer = nullptr;
HANDLE prefetchEvent = nullptr, prefetchWorker = nullptr;
SRWLOCK queueLock = SRWLOCK_INIT;
std::atomic<bool> recording{false};
std::atomic<unsigned long long> nextCall{0}, dropped{0}, counts[8]{};
std::atomic<int> lastPlaying{-2147483647};
std::atomic<uint32_t> lastVolume{0xffffffffu};
std::atomic<unsigned> pendingTrackRecovery{0}, activeTrackRecovery{0};
std::atomic<long long> recoveryDeadlineTicks{0};
std::atomic<bool> redirectReady{false};
std::atomic<DWORD> musicThreadId{0};
rwr::HotkeyTracker hotkeyTracker;
volatile char* engineLoadingFlag = nullptr;
LARGE_INTEGER epoch{}, frequency{};

enum Kind { Init, Load, Play, Pause, Resume, Stop, Volume, Playing };
const char* names[] = {"Initialize", "LoadMusic", "Play", "Pause", "Resume", "Stop", "SetVolume", "IsPlayingMusic"};
const char* imports[] = {
    "?C3DMusicXWMA_Initialize@@YAXXZ", "?C3DMusicXWMA_LoadMusic@@YAXPEAD@Z",
    "?C3DMusicXWMA_Play@@YAX_NH@Z", "?C3DMusicXWMA_Pause@@YAXXZ",
    "?C3DMusicXWMA_Resume@@YAXXZ", "?C3DMusicXWMA_Stop@@YAXXZ",
    "?C3DMusicXWMA_SetVolume@@YAXM@Z", "?C3DMusicXWMA_IsPlayingMusic@@YAHXZ"
};
void* originals[8]{};
using VoidFn = void (*)();
using LoadFn = void (*)(char*);
using PlayFn = void (*)(bool, int);
using VolumeFn = void (*)(float);
using PlayingFn = int (*)();
using PathFn = void (*)(char*, const char*);
PathFn originalPathResolver = nullptr;

constexpr size_t kPathStolenBytes = 18;
constexpr size_t kPathContinuationBytes = kPathStolenBytes + 14;
static const unsigned char expectedPathPrologue[kPathStolenBytes] = {
    0x40,0x55,0x41,0x56,0x48,0x81,0xec,0x18,0x02,0x00,0x00,0x48,0x8b,0x05,0xb6,0x64,0x15,0x00
};

// RML 1.0.1's Tesmio compatibility trampoline copies stolen instructions
// verbatim. This prologue contains a RIP-relative security-cookie load, so its
// displacement must be rewritten when the instruction moves. The continuation
// is allocated near the engine target so the corrected disp32 remains valid.
bool buildRelocatedPathContinuation(unsigned char* continuation, size_t capacity,
                                    const unsigned char* prologue,
                                    const unsigned char* target) {
    if (!continuation || !prologue || !target || capacity < kPathContinuationBytes ||
        prologue[11] != 0x48 || prologue[12] != 0x8b || prologue[13] != 0x05) return false;
    int32_t originalDisplacement = 0;
    memcpy(&originalDisplacement, prologue + 14, sizeof(originalDisplacement));
    const int64_t referenced = static_cast<int64_t>(reinterpret_cast<intptr_t>(target)) +
        static_cast<int64_t>(kPathStolenBytes) + originalDisplacement;
    const int64_t relocatedEnd = static_cast<int64_t>(reinterpret_cast<intptr_t>(continuation)) +
        static_cast<int64_t>(kPathStolenBytes);
    const int64_t relocatedDisplacement = referenced - relocatedEnd;
    if (relocatedDisplacement < INT32_MIN || relocatedDisplacement > INT32_MAX) return false;

    memcpy(continuation, prologue, kPathStolenBytes);
    const int32_t corrected = static_cast<int32_t>(relocatedDisplacement);
    memcpy(continuation + 14, &corrected, sizeof(corrected));
    static const unsigned char absoluteJump[6] = {0xff, 0x25, 0x00, 0x00, 0x00, 0x00};
    memcpy(continuation + kPathStolenBytes, absoluteJump, sizeof(absoluteJump));
    const uint64_t returnAddress = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(target + kPathStolenBytes));
    memcpy(continuation + kPathStolenBytes + sizeof(absoluteJump), &returnAddress, sizeof(returnAddress));
    return true;
}

bool toWide(const char* utf8, std::wstring& wide) {
    if (!utf8) { wide.clear(); return true; }
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    if (n <= 0) return false;
    wide.resize(static_cast<size_t>(n));
    if (MultiByteToWideChar(CP_UTF8, 0, utf8, -1, wide.data(), n) <= 0) return false;
    wide.pop_back();
    return true;
}

bool toUtf8(const std::wstring& wide, std::string& utf8) {
    if (wide.empty()) { utf8.clear(); return true; }
    const int n = WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 0) return false;
    utf8.resize(static_cast<size_t>(n));
    if (WideCharToMultiByte(CP_UTF8, 0, wide.c_str(), -1, utf8.data(), n, nullptr, nullptr) <= 0) return false;
    utf8.pop_back();
    return true;
}

void toLower(std::string& text) {
    for (char& c : text) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
}

void toLower(std::wstring& text) {
    for (wchar_t& c : text) c = static_cast<wchar_t>(towlower(c));
}

bool isSeparator(wchar_t c) { return c == L'\\' || c == L'/'; }

void trimPathTrail(std::wstring& path) {
    while (!path.empty() && isSeparator(path.back())) path.pop_back();
}

bool normalizeFullPath(const std::wstring& in, std::wstring& out) {
    WCHAR buf[MAX_PATH * 4]{};
    const DWORD needed = GetFullPathNameW(in.c_str(), static_cast<DWORD>(std::size(buf)), buf, nullptr);
    if (!needed || needed >= std::size(buf)) return false;
    out.assign(buf, needed);
    trimPathTrail(out);
    return true;
}

bool getModuleRelativePath(HMODULE module, const wchar_t* relative, std::wstring& path) {
    path.clear();
    // A missing handle must not silently resolve to this process's executable.
    if (!module || !relative) return false;
    std::vector<wchar_t> buffer(32768, 0);
    const DWORD length = GetModuleFileNameW(module, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (!length || length >= buffer.size()) return false;
    const wchar_t* slash = wcsrchr(buffer.data(), L'\\');
    if (!slash) return false;
    path.assign(buffer.data(), static_cast<size_t>(slash - buffer.data()) + 1);
    path += relative;
    return true;
}

bool expandEnvironmentPath(const std::wstring& in, std::wstring& out) {
    if (in.empty()) { out.clear(); return true; }
    const DWORD needed = ExpandEnvironmentStringsW(in.c_str(), nullptr, 0);
    if (!needed || needed > 32768) return false;
    std::vector<wchar_t> buffer(needed);
    const DWORD written = ExpandEnvironmentStringsW(in.c_str(), buffer.data(), needed);
    if (!written || written > needed) return false;
    out.assign(buffer.data(), written - 1);
    return true;
}

TrackFormat detectFormat(const std::wstring& path) {
    const wchar_t* dot = wcsrchr(path.c_str(), L'.');
    if (!dot) return TrackFormat::Unknown;
    std::wstring ext(dot + 1);
    toLower(ext);
    if (ext == L"wav") return TrackFormat::Wav;
    if (ext == L"flac") return TrackFormat::Flac;
    if (ext == L"mp3") return TrackFormat::Mp3;
    if (ext == L"xwma") return TrackFormat::Xwma;
    return TrackFormat::Unknown;
}

std::string normalizeIdText(std::string text) {
    for (auto& c : text) {
        if (c == '\\') c = '/';
    }
    toLower(text);
    return text;
}

std::string makeTrackId(TrackSource source, const std::string& rel) {
    std::string id = normalizeIdText(rel);
    switch (source) {
    case TrackSource::External: return "external:" + id;
    case TrackSource::PluginLocal: return "plugin-local:" + id;
    case TrackSource::Original: return "original:" + id;
    }
    return "track:" + id;
}

bool getStorageRoot(std::wstring& root) {
    WCHAR local[32768]{};
    if (!GetEnvironmentVariableW(L"LOCALAPPDATA", local, static_cast<DWORD>(std::size(local)))) return false;
    root.assign(local);
    trimPathTrail(root);
    root.push_back(L'\\');
    for (const char* c = kStorageFolder; *c; ++c) root.push_back(static_cast<wchar_t>(*c));
    return true;
}

bool getKnownMusicFolder(std::wstring& folder) {
    WCHAR buffer[MAX_PATH];
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_MYMUSIC, nullptr, SHGFP_TYPE_CURRENT, buffer))) return false;
    folder.assign(buffer);
    trimPathTrail(folder);
    return true;
}

void putLe32(unsigned char* out, uint32_t value) {
    out[0] = static_cast<unsigned char>(value & 0xff);
    out[1] = static_cast<unsigned char>((value >> 8) & 0xff);
    out[2] = static_cast<unsigned char>((value >> 16) & 0xff);
    out[3] = static_cast<unsigned char>((value >> 24) & 0xff);
}

void putLe16(unsigned char* out, uint16_t value) {
    out[0] = static_cast<unsigned char>(value & 0xff);
    out[1] = static_cast<unsigned char>((value >> 8) & 0xff);
}

bool ensureDirectory(std::wstring directory) {
    if (directory.empty()) return false;
    const DWORD code = SHCreateDirectoryExW(nullptr, directory.c_str(), nullptr);
    return code == ERROR_SUCCESS || code == ERROR_ALREADY_EXISTS || code == ERROR_FILE_EXISTS;
}

bool prepareSilenceTrack(const std::wstring& cacheFolder) {
    std::wstring silencePath = cacheFolder;
    silencePath.push_back(L'\\');
    for (const char* c = kSilenceTrackFile; *c; ++c) silencePath.push_back(static_cast<wchar_t>(*c));
    manager.silenceTrackPath.clear();
    if (!toUtf8(silencePath, manager.silenceTrackPath)) return false;

    const uint32_t sampleRate = 48000;
    const uint16_t channels = 2;
    const uint16_t bitsPerSample = 16;
    const uint32_t duration = kSilenceSeconds;
    const uint16_t blockAlign = static_cast<uint16_t>((channels * bitsPerSample) / 8);
    const uint32_t byteRate = sampleRate * blockAlign;
    const uint32_t dataBytes = byteRate * duration;
    const uint32_t chunkSize = 36 + dataBytes;

    HANDLE existing = CreateFileW(silencePath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (existing != INVALID_HANDLE_VALUE) {
        LARGE_INTEGER size{}; bool keep = false;
        if (GetFileSizeEx(existing, &size) && size.QuadPart == (LONGLONG)(dataBytes + 44)) keep = true;
        CloseHandle(existing);
        if (keep) {
            manager.silenceTrackReady = true;
            return true;
        }
    }
    HANDLE output = CreateFileW(silencePath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) return false;
    unsigned char header[44] = {};
    memcpy(header, "RIFF", 4);
    putLe32(header + 4, chunkSize);
    memcpy(header + 8, "WAVE", 4);
    memcpy(header + 12, "fmt ", 4);
    putLe32(header + 16, 16);
    putLe16(header + 20, 1);
    putLe16(header + 22, channels);
    putLe32(header + 24, sampleRate);
    putLe32(header + 28, byteRate);
    putLe16(header + 32, blockAlign);
    putLe16(header + 34, bitsPerSample);
    memcpy(header + 36, "data", 4);
    putLe32(header + 40, dataBytes);
    DWORD written = 0;
    bool ok = WriteFile(output, header, sizeof(header), &written, nullptr) && written == sizeof(header);
    if (ok) {
        std::vector<unsigned char> zeros(static_cast<size_t>(dataBytes), 0);
        ok = WriteFile(output, zeros.data(), static_cast<DWORD>(zeros.size()), &written, nullptr) && written == zeros.size();
    }
    CloseHandle(output);
    manager.silenceTrackReady = ok;
    return ok;
}

std::string trimAscii(std::string value) {
    size_t first = 0;
    while (first < value.size() && std::isspace(static_cast<unsigned char>(value[first]))) ++first;
    size_t last = value.size();
    while (last > first && std::isspace(static_cast<unsigned char>(value[last - 1]))) --last;
    return value.substr(first, last - first);
}

bool getPluginIniPath(std::wstring& path) {
    return getModuleRelativePath(selfModule, L"RedWolfRadio.ini", path);
}

bool createDefaultIniIfMissing() {
    std::wstring iniPath;
    if (!getPluginIniPath(iniPath)) return false;
    HANDLE file = CreateFileW(iniPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_EXISTS;
    const std::string text = kDefaultIni;
    DWORD written = 0;
    const bool ok = WriteFile(file, text.data(), static_cast<DWORD>(text.size()), &written, nullptr) &&
        written == text.size();
    CloseHandle(file);
    if (!ok) DeleteFileW(iniPath.c_str());
    return ok;
}

bool readIniValue(const char* section, const char* key, const char* fallback, std::string& value) {
    if (!host || !host->configString) return false;
    std::vector<char> buffer(32768, 0);
    host->configString(kIniName, section, key, buffer.data(), static_cast<int>(buffer.size()), fallback);
    value.assign(buffer.data());
    return true;
}

bool parseIniBool(const std::string& raw, bool& value) {
    std::string text = trimAscii(raw);
    toLower(text);
    if (text == "true" || text == "yes" || text == "on" || text == "1") { value = true; return true; }
    if (text == "false" || text == "no" || text == "off" || text == "0") { value = false; return true; }
    return false;
}

bool parseIniUnsigned(const std::string& raw, uint64_t minimum, uint64_t maximum, uint64_t& value) {
    const std::string text = trimAscii(raw);
    if (text.empty() || text.front() == '-') return false;
    char* end = nullptr;
    const unsigned long long parsed = strtoull(text.c_str(), &end, 10);
    if (!end || *end != 0 || parsed < minimum || parsed > maximum) return false;
    value = static_cast<uint64_t>(parsed);
    return true;
}

void parseHotkeyConfig(Config& config) {
    config.hotkeyWarnings.clear();
    std::string value;
    if (!readIniValue("Hotkeys", "Enabled", "true", value) ||
        !parseIniBool(value, config.hotkeysEnabled)) {
        config.hotkeysEnabled = false;
        config.hotkeyWarnings.push_back("Hotkeys.Enabled must be true or false; shortcuts disabled.");
    }
    const char* keys[] = {"Next", "Previous", "PlayPause"};
    const char* defaults[] = {"Ctrl+Shift+Right", "Ctrl+Shift+Left", "Ctrl+Shift+Down"};
    for (size_t i = 0; i < config.hotkeys.size(); ++i) {
        std::string error;
        if (!readIniValue("Hotkeys", keys[i], defaults[i], value) ||
            !rwr::parseHotkey(value, config.hotkeys[i], error)) {
            config.hotkeys[i] = {};
            config.hotkeyWarnings.push_back(std::string("Hotkeys.") + keys[i] +
                " disabled: " + error);
        }
    }
    std::array<bool, 3> duplicate{};
    for (size_t i = 0; i < config.hotkeys.size(); ++i) {
        for (size_t j = i + 1; j < config.hotkeys.size(); ++j) {
            if (config.hotkeys[i].key && rwr::sameHotkey(config.hotkeys[i], config.hotkeys[j])) {
                duplicate[i] = duplicate[j] = true;
                config.hotkeyWarnings.push_back(std::string("Duplicate Hotkeys.") + keys[i] +
                    " and Hotkeys." + keys[j] + "; both bindings disabled.");
            }
        }
    }
    for (size_t i = 0; i < duplicate.size(); ++i) if (duplicate[i]) config.hotkeys[i] = {};
}

void parseDisplayConfig(Config& config) {
    std::string value;
    // A display setting must never disable otherwise valid music playback.
    if (!readIniValue("Display", "ShowTrackName", "false", value) ||
        !parseIniBool(value, config.showTrackName)) {
        config.showTrackName = false;
        config.hotkeyWarnings.push_back("Display.ShowTrackName must be true or false; notification disabled.");
    }
    if (!readIniValue("Display", "NativeSongPopup", "true", value) ||
        !parseIniBool(value, config.nativeSongPopup)) {
        config.nativeSongPopup = false;
        config.hotkeyWarnings.push_back("Display.NativeSongPopup must be true or false; native player disabled.");
    }
    readIniValue("Display", "Language", "en", value);
    value = trimAscii(value); toLower(value);
    config.displayRussian = value == "ru";
    if (value != "ru" && value != "en")
        config.hotkeyWarnings.push_back("Display.Language must be en or ru; using en.");
}

bool parseConfig(Config& config, bool& malformed, std::string& error) {
    malformed = false;
    error.clear();
    if (!createDefaultIniIfMissing()) {
        malformed = true; error = "could not create or locate RedWolfRadio.ini"; return false;
    }

    std::string value;
    if (!readIniValue("RedWolfRadio", "SchemaVersion", "1", value) ||
        trimAscii(value) != std::to_string(kCatalogSchemaVersion)) {
        malformed = true; error = "RedWolfRadio.SchemaVersion must be 1"; return false;
    }
    if (!readIniValue("Sources", "ExternalEnabled", "true", value) ||
        !parseIniBool(value, config.externalEnabled)) {
        malformed = true; error = "Sources.ExternalEnabled must be true or false"; return false;
    }
    if (!readIniValue("Sources", "ExternalRecursive", "true", value) ||
        !parseIniBool(value, config.externalRecursive)) {
        malformed = true; error = "Sources.ExternalRecursive must be true or false"; return false;
    }
    if (!readIniValue("Sources", "PluginLocalEnabled", "false", value) ||
        !parseIniBool(value, config.pluginLocalEnabled)) {
        malformed = true; error = "Sources.PluginLocalEnabled must be true or false"; return false;
    }
    if (!readIniValue("Sources", "PluginLocalRecursive", "true", value) ||
        !parseIniBool(value, config.pluginLocalRecursive)) {
        malformed = true; error = "Sources.PluginLocalRecursive must be true or false"; return false;
    }
    uint64_t number = 0;
    if (!readIniValue("Cache", "MaxSizeMiB", "4096", value) ||
        !parseIniUnsigned(value, 0, 1048576, number)) {
        malformed = true; error = "Cache.MaxSizeMiB must be an integer from 0 through 1048576"; return false;
    }
    config.cacheMaxSizeMiB = number;
    if (!readIniValue("Cache", "PrefetchTracks", "5", value) ||
        !parseIniUnsigned(value, 1, 256, number)) {
        malformed = true; error = "Cache.PrefetchTracks must be an integer from 1 through 256"; return false;
    }
    config.prefetchTracks = static_cast<uint32_t>(number);
    if (!readIniValue("Cache", "HistoryTracks", "3", value) ||
        !parseIniUnsigned(value, 0, kHistoryLimit, number)) {
        malformed = true; error = "Cache.HistoryTracks must be an integer from 0 through 64"; return false;
    }
    config.historyTracks = static_cast<uint32_t>(number);
    parseHotkeyConfig(config);
    parseDisplayConfig(config);
    if (!readIniValue("Sources", "ExternalPath", "", value)) {
        malformed = true; error = "could not read Sources.ExternalPath"; return false;
    }
    value = trimAscii(value);
    if (value.empty()) {
        config.externalUseDefault = true;
        if (!getKnownMusicFolder(config.externalPath)) {
            malformed = true; error = "Sources.ExternalPath is empty and Windows Music is unavailable"; return false;
        }
    } else {
        config.externalUseDefault = false;
        std::wstring configuredPath;
        if (!toWide(value.c_str(), configuredPath)) {
            malformed = true; error = "Sources.ExternalPath is not valid UTF-8"; return false;
        }
        if (!expandEnvironmentPath(configuredPath, config.externalPath)) {
            malformed = true; error = "Sources.ExternalPath environment expansion failed"; return false;
        }
    }
    if (!readIniValue("Originals", "Mode", "all", value)) {
        malformed = true; error = "could not read Originals.Mode"; return false;
    }
    value = trimAscii(value);
    toLower(value);
    if (value == "all") config.originalsMode = OriginalsMode::All;
    else if (value == "selected") config.originalsMode = OriginalsMode::Selected;
    else if (value == "none") config.originalsMode = OriginalsMode::None;
    else { malformed = true; error = "Originals.Mode must be all, selected, or none"; return false; }

    if (!readIniValue("Originals", "ExcludedIds", "", value)) {
        malformed = true; error = "could not read Originals.ExcludedIds"; return false;
    }
    config.excludedIds.clear();
    size_t start = 0;
    while (start <= value.size()) {
        const size_t comma = value.find(',', start);
        std::string id = trimAscii(value.substr(start, comma == std::string::npos ? std::string::npos : comma - start));
        if (!id.empty()) {
            toLower(id);
            config.excludedIds.push_back(id);
        }
        if (comma == std::string::npos) break;
        start = comma + 1;
    }
    return true;
}

void collectTrackRecursively(const std::wstring& root,
                            const std::wstring& relative,
                            TrackSource source,
                            bool recursive,
                            std::vector<Track>& tracks,
                            std::set<std::string>& usedIds) {
    std::wstring search = root;
    if (!search.empty() && !isSeparator(search.back())) search.push_back(L'\\');
    search.push_back(L'*');
    WIN32_FIND_DATAW data{};
    HANDLE it = FindFirstFileW(search.c_str(), &data);
    if (it == INVALID_HANDLE_VALUE) return;
    do {
        if (wcscmp(data.cFileName, L".") == 0 || wcscmp(data.cFileName, L"..") == 0) continue;
        std::wstring full = root;
        if (!full.empty() && !isSeparator(full.back())) full.push_back(L'\\');
        full += data.cFileName;
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) {
            if (!recursive || (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)) continue;
            std::wstring nextRelative = relative;
            if (!nextRelative.empty()) nextRelative.push_back(L'\\');
            nextRelative += data.cFileName;
            collectTrackRecursively(full, nextRelative, source, recursive, tracks, usedIds);
            continue;
        }
        TrackFormat format = detectFormat(full);
        if (format == TrackFormat::Unknown) continue;
        Track track;
        track.format = format;
        track.source = source;
        std::wstring relWide = relative;
        if (!relWide.empty()) relWide.push_back(L'\\');
        relWide += data.cFileName;
        if (!toUtf8(relWide, track.relativeUtf8)) continue;
        track.absoluteWide = full;
        track.displayName = data.cFileName;
        const size_t extension = track.displayName.find_last_of(L'.');
        if (extension != std::wstring::npos) track.displayName.resize(extension);
        if (source == TrackSource::Original) {
            std::string game = "music/";
            game += normalizeIdText(track.relativeUtf8);
            track.relativeUtf8 = game;
        }
        track.nativePlayable = source == TrackSource::Original &&
            (format == TrackFormat::Wav || format == TrackFormat::Xwma);
        if (track.nativePlayable) track.preparation = PreparationState::Ready;
        else if (source != TrackSource::Original)
            track.preparation = PreparationState::Pending;
        else track.preparation = PreparationState::Rejected;
        track.id = makeTrackId(source, track.relativeUtf8);
        if (usedIds.insert(track.id).second) tracks.push_back(std::move(track));
    } while (FindNextFileW(it, &data));
    FindClose(it);
}

void rebuildPlayBag();

bool trackEligibleLocked(const Track& track) {
    if (track.preparation == PreparationState::Rejected || manager.bannedTrackIds.count(track.id)) return false;
    return track.source != TrackSource::Original ||
        (manager.config.originalsMode != OriginalsMode::None &&
         (manager.config.originalsMode != OriginalsMode::Selected ||
          !manager.excludedOriginalIds.count(track.id)));
}

const Track* historyTrackLocked(size_t cursor) {
    if (cursor >= manager.history.size() || manager.history[cursor] >= manager.catalog.size()) return nullptr;
    const Track& track = manager.catalog[manager.history[cursor]];
    return trackEligibleLocked(track) ? &track : nullptr;
}

size_t nextHistoryCursorLocked() {
    if ((manager.navigationPending || manager.awaitingNavigationLoad) &&
        manager.navigationHistoryCursor != kNoHistory) return manager.navigationHistoryCursor;
    if (manager.historyCursor != kNoHistory && manager.historyCursor + 1 < manager.history.size())
        return manager.historyCursor + 1;
    return kNoHistory;
}

std::vector<size_t> protectedHistoryCursorsLocked() {
    std::vector<size_t> positions;
    const size_t retained = static_cast<size_t>(manager.config.historyTracks) + 1;
    const size_t start = manager.history.size() > retained ? manager.history.size() - retained : 0;
    for (size_t i = start; i < manager.history.size(); ++i) positions.push_back(i);
    positions.push_back(manager.historyCursor);
    positions.push_back(nextHistoryCursorLocked());
    return positions;
}

std::wstring cachePathKey(std::wstring path) {
    trimPathTrail(path);
    toLower(path);
    return path;
}

struct CacheEntry {
    std::wstring path;
    std::wstring key;
    uint64_t bytes = 0;
    uint64_t stamp = 0;
};

uint64_t configuredCacheLimitBytes() {
    if (!manager.config.cacheMaxSizeMiB) return 0;
    return manager.config.cacheMaxSizeMiB * 1024ull * 1024ull;
}

std::vector<CacheEntry> enumerateCacheFiles(const std::wstring& directory) {
    std::vector<CacheEntry> files;
    if (directory.empty()) return files;
    std::wstring search = directory;
    if (!isSeparator(search.back())) search.push_back(L'\\');
    search += L"*";
    WIN32_FIND_DATAW data{};
    HANDLE found = FindFirstFileW(search.c_str(), &data);
    if (found == INVALID_HANDLE_VALUE) return files;
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
            _wcsicmp(data.cFileName, L"red_wolf_radio_silent.wav") == 0) continue;
        const TrackFormat format = detectFormat(data.cFileName);
        if (format != TrackFormat::Wav && format != TrackFormat::Xwma) continue;
        CacheEntry entry;
        entry.path = directory;
        if (!isSeparator(entry.path.back())) entry.path.push_back(L'\\');
        entry.path += data.cFileName;
        entry.key = cachePathKey(entry.path);
        entry.bytes = (static_cast<uint64_t>(data.nFileSizeHigh) << 32) | data.nFileSizeLow;
        entry.stamp = (static_cast<uint64_t>(data.ftLastWriteTime.dwHighDateTime) << 32) |
            data.ftLastWriteTime.dwLowDateTime;
        files.push_back(std::move(entry));
    } while (FindNextFileW(found, &data));
    FindClose(found);
    return files;
}

bool cachePathProtectedLocked(const std::wstring& key) {
    if (!manager.activeTrackId.empty()) {
        for (const Track& track : manager.catalog) {
            if (track.id == manager.activeTrackId &&
                !track.preparedWide.empty() && cachePathKey(track.preparedWide) == key) return true;
        }
    }
    // Retain recent navigation targets, not the entire playback history.
    for (size_t cursor : protectedHistoryCursorsLocked()) {
        const Track* track = historyTrackLocked(cursor);
        if (track && !track->preparedWide.empty() && cachePathKey(track->preparedWide) == key) return true;
    }
    size_t considered = 0;
    for (size_t position = manager.bagCursor;
         position < manager.playBag.size() && considered < manager.config.prefetchTracks;
         ++position, ++considered) {
        const Track& track = manager.catalog[manager.playBag[position]];
        if (!track.preparedWide.empty() && cachePathKey(track.preparedWide) == key) return true;
    }
    return false;
}

std::set<std::wstring> protectedCachePathsSnapshot() {
    std::set<std::wstring> protectedPaths;
    AcquireSRWLockExclusive(&stateLock);
    if (manager.trackSelectionDirty) rebuildPlayBag();
    for (size_t cursor : protectedHistoryCursorsLocked()) {
        const Track* track = historyTrackLocked(cursor);
        if (track && !track->preparedWide.empty()) protectedPaths.insert(cachePathKey(track->preparedWide));
    }
    if (!manager.activeTrackId.empty()) {
        for (const Track& track : manager.catalog) {
            if (track.id == manager.activeTrackId && !track.preparedWide.empty()) {
                protectedPaths.insert(cachePathKey(track.preparedWide));
                break;
            }
        }
    }
    size_t considered = 0;
    for (size_t position = manager.bagCursor;
         position < manager.playBag.size() && considered < manager.config.prefetchTracks;
         ++position, ++considered) {
        const Track& track = manager.catalog[manager.playBag[position]];
        if (!track.preparedWide.empty()) protectedPaths.insert(cachePathKey(track.preparedWide));
    }
    ReleaseSRWLockExclusive(&stateLock);
    return protectedPaths;
}

uint64_t pruneCacheToConfiguredLimit() {
    const uint64_t limit = configuredCacheLimitBytes();
    if (!limit || manager.cacheDirectory.empty()) return 0;
    const std::set<std::wstring> protectedPaths = protectedCachePathsSnapshot();
    std::vector<CacheEntry> files = enumerateCacheFiles(manager.cacheDirectory);
    uint64_t total = 0;
    for (const CacheEntry& file : files) total += file.bytes;
    if (total <= limit) return total;
    std::sort(files.begin(), files.end(), [](const CacheEntry& a, const CacheEntry& b) {
        if (a.stamp != b.stamp) return a.stamp < b.stamp;
        return a.key < b.key;
    });
    size_t removed = 0;
    for (const CacheEntry& file : files) {
        if (total <= limit) break;
        if (protectedPaths.count(file.key)) continue;
        AcquireSRWLockExclusive(&stateLock);
        if (manager.trackSelectionDirty) rebuildPlayBag();
        const bool protectedNow = cachePathProtectedLocked(file.key);
        const bool deleted = !protectedNow && DeleteFileW(file.path.c_str()) != FALSE;
        if (deleted) {
            for (Track& track : manager.catalog) {
                if (track.source == TrackSource::Original || track.preparedWide.empty()) continue;
                if (cachePathKey(track.preparedWide) == file.key) {
                    track.preparedWide.clear();
                    track.preparedUtf8.clear();
                    track.nativePlayable = false;
                    track.preparation = PreparationState::Pending;
                }
            }
            total -= file.bytes;
            ++removed;
        }
        ReleaseSRWLockExclusive(&stateLock);
    }
    if (removed && host && host->log) host->log("[RedWolfRadio] Cache eviction removed %zu derived file(s).", removed);
    if (total > limit && host && host->log) {
        host->log("[RedWolfRadio] Cache remains above MaxSizeMiB because active/prefetched tracks are protected.");
    }
    return total;
}

void touchCacheFile(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), FILE_WRITE_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
        FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return;
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    SetFileTime(file, nullptr, nullptr, &now);
    CloseHandle(file);
}

bool prepareTrackAtIndex(size_t index) {
    std::wstring source;
    std::string id;
    TrackFormat format = TrackFormat::Unknown;
    AcquireSRWLockExclusive(&stateLock);
    if (index >= manager.catalog.size() ||
        manager.catalog[index].preparation != PreparationState::Pending) {
        ReleaseSRWLockExclusive(&stateLock);
        return false;
    }
    Track& pending = manager.catalog[index];
    pending.preparation = PreparationState::Preparing;
    source = pending.absoluteWide;
    id = pending.id;
    format = pending.format;
    ReleaseSRWLockExclusive(&stateLock);

    pruneCacheToConfiguredLimit();
    CanonicalAudioResult result;
    NativeAudioResult nativeResult;
    std::string error;
    const bool xwma = format == TrackFormat::Xwma;
    bool owned = true;
    bool ok = xwma ? prepareNativeXwma(source, manager.cacheDirectory, nativeResult, error) :
        (!manager.cacheDirectory.empty() && convertToCanonicalCache(source, manager.cacheDirectory, result, error));
    if (xwma) {
        result.path = nativeResult.path; result.cacheHit = nativeResult.cacheHit;
        owned = nativeResult.copied;
    }
    std::string routed;
    std::wstring enginePath;
    if (ok && (!resolveEnginePath(result.path, enginePath) || !toUtf8(enginePath, routed))) {
        ok = false;
        error = "prepared path has no safe narrow route within the verified engine limit";
    }

    AcquireSRWLockExclusive(&stateLock);
    if (index < manager.catalog.size() && manager.catalog[index].id == id) {
        Track& track = manager.catalog[index];
        if (ok) {
            // Only plugin-owned files participate in eviction/timestamp touches.
            track.preparedWide = owned ? result.path : L"";
            track.preparedUtf8 = routed;
            track.nativePlayable = true;
            track.preparation = PreparationState::Ready;
            if (result.cacheHit) ++manager.cacheHits;
            if (xwma) ++manager.nativeXwma;
            else if (!result.cacheHit) ++manager.converted;
        } else {
            track.nativePlayable = false;
            track.preparation = PreparationState::Rejected;
            ++manager.rejected;
            manager.trackSelectionDirty = true;
        }
    }
    ReleaseSRWLockExclusive(&stateLock);
    if (ok) {
        if (host && host->log) host->log("[RedWolfRadio] Prepared %s (%s).",
            id.c_str(), xwma ? (owned ? "unchanged XWMA cache copy" : "direct native XWMA") :
                (result.cacheHit ? "cache hit" : "converted"));
        if (owned) touchCacheFile(result.path);
        pruneCacheToConfiguredLimit();
    } else if (host && host->log) {
        host->log("[RedWolfRadio] Preparation failed; omitted %s: %s", id.c_str(), error.c_str());
    }
    return ok;
}

void rebuildPlayBag() {
    manager.playBag.clear();
    for (size_t i = 0; i < manager.catalog.size(); ++i) {
        const Track& track = manager.catalog[i];
        if (!trackEligibleLocked(track)) continue;
        manager.playBag.push_back(i);
    }
    if (!manager.playBag.empty()) {
        std::random_device rd;
        std::mt19937 rng(rd());
        std::shuffle(manager.playBag.begin(), manager.playBag.end(), rng);
        if (manager.playBag.size() > 1 &&
            manager.catalog[manager.playBag.front()].id == manager.lastPlayedId) {
            std::uniform_int_distribution<size_t> choice(1, manager.playBag.size() - 1);
            std::swap(manager.playBag.front(), manager.playBag[choice(rng)]);
        }
    }
    manager.bagCursor = 0;
    manager.trackSelectionDirty = false;
}

void ensurePlayBagLocked() {
    if (manager.trackSelectionDirty) rebuildPlayBag();
    if (!manager.playBag.empty() && manager.bagCursor >= manager.playBag.size()) {
        std::random_device rd;
        std::mt19937 rng(rd());
        std::shuffle(manager.playBag.begin(), manager.playBag.end(), rng);
        if (manager.playBag.size() > 1 &&
            manager.catalog[manager.playBag.front()].id == manager.lastPlayedId) {
            std::uniform_int_distribution<size_t> choice(1, manager.playBag.size() - 1);
            std::swap(manager.playBag.front(), manager.playBag[choice(rng)]);
        }
        manager.bagCursor = 0;
    }
}

const Track* activateTrackLocked(size_t index, size_t historyCursor = kNoHistory) {
    const Track& track = manager.catalog[index];
    manager.activeTrackId = track.id;
    manager.activeTrackSource = track.source;
    manager.activeTrackPending = true;
    manager.activeTrackPlaying = false;
    manager.activeLoadStart = 0;
    manager.notificationText.clear(); manager.notificationUntil = 0;
    manager.selectedHistoryCursor = historyCursor;
    manager.historyRecorded = false;
    manager.voicePaused = false;
    manager.pausedVoice = nullptr;
    manager.deferredPlay = false;
    manager.navigationPending = manager.awaitingNavigationLoad = false;
    manager.navigationHistoryCursor = kNoHistory;
    return &track;
}

void recordPlayingHistoryLocked() {
    if (manager.historyRecorded || manager.activeTrackId.empty()) return;
    if (manager.selectedHistoryCursor < manager.history.size() &&
        historyTrackLocked(manager.selectedHistoryCursor) &&
        historyTrackLocked(manager.selectedHistoryCursor)->id == manager.activeTrackId) {
        manager.historyCursor = manager.selectedHistoryCursor;
        manager.historyRecorded = true;
        return;
    }
    for (size_t index = 0; index < manager.catalog.size(); ++index) {
        if (manager.catalog[index].id != manager.activeTrackId) continue;
        manager.history.push_back(index);
        if (manager.history.size() > kHistoryLimit) {
            manager.history.erase(manager.history.begin());
            // An explicit button target chosen while a new song was loading
            // must follow its entry when the bounded history drops the oldest.
            if ((manager.pendingNavigationAction == rwr::MusicAction::Restart ||
                manager.pendingNavigationAction == rwr::MusicAction::PreviousTrack) &&
                manager.navigationHistoryCursor != kNoHistory && manager.navigationHistoryCursor)
                --manager.navigationHistoryCursor;
        }
        manager.historyCursor = manager.history.size() - 1;
        manager.historyRecorded = true;
        return;
    }
}

const Track* pickTrackLocked() {
    if (manager.catalog.empty()) return nullptr;
    const size_t replay = nextHistoryCursorLocked();
    if (replay != kNoHistory) {
        const Track* historical = historyTrackLocked(replay);
        if (historical) {
            if (!historical->nativePlayable) return nullptr;
            return activateTrackLocked(manager.history[replay], replay);
        }
        // An unavailable/failed history entry must not trap the scheduler.
        if (replay < manager.history.size()) manager.historyCursor = replay;
        manager.navigationHistoryCursor = kNoHistory;
    }
    ensurePlayBagLocked();
    if (manager.playBag.empty()) return nullptr;
    size_t readyPosition = manager.bagCursor;
    while (readyPosition < manager.playBag.size() &&
           !manager.catalog[manager.playBag[readyPosition]].nativePlayable) ++readyPosition;
    if (readyPosition >= manager.playBag.size()) return nullptr;
    if (readyPosition != manager.bagCursor)
        std::swap(manager.playBag[readyPosition], manager.playBag[manager.bagCursor]);
    return activateTrackLocked(manager.playBag[manager.bagCursor++]);
}

size_t nextPrefetchCandidateLocked() {
    const size_t replay = nextHistoryCursorLocked();
    const Track* historical = historyTrackLocked(replay);
    if (historical && historical->preparation == PreparationState::Pending) return manager.history[replay];
    ensurePlayBagLocked();
    size_t considered = 0;
    for (size_t position = manager.bagCursor;
         position < manager.playBag.size() && considered < manager.config.prefetchTracks;
         ++position, ++considered) {
        const size_t index = manager.playBag[position];
        if (manager.catalog[index].preparation == PreparationState::Pending) return index;
    }
    return static_cast<size_t>(-1);
}

size_t nextPrefetchCandidate() {
    AcquireSRWLockExclusive(&stateLock);
    const size_t index = nextPrefetchCandidateLocked();
    ReleaseSRWLockExclusive(&stateLock);
    return index;
}

void prepareInitialPrefetchWindow() {
    for (;;) {
        const size_t index = nextPrefetchCandidate();
        if (index == static_cast<size_t>(-1)) break;
        prepareTrackAtIndex(index);
    }
    pruneCacheToConfiguredLimit();
}

DWORD WINAPI prefetchMain(void*) {
    for (;;) {
        if (WaitForSingleObject(prefetchEvent, INFINITE) != WAIT_OBJECT_0) return 0;
        std::wstring activePath;
        AcquireSRWLockShared(&stateLock);
        if (!manager.activeTrackId.empty()) {
            for (const Track& track : manager.catalog) {
                if (track.id == manager.activeTrackId) { activePath = track.preparedWide; break; }
            }
        }
        ReleaseSRWLockShared(&stateLock);
        if (!activePath.empty()) touchCacheFile(activePath);
        for (;;) {
            const size_t index = nextPrefetchCandidate();
            if (index == static_cast<size_t>(-1)) break;
            prepareTrackAtIndex(index);
        }
    }
}

bool startPrefetchWorker() {
    if (prefetchWorker) return true;
    prefetchEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    if (!prefetchEvent) return false;
    prefetchWorker = CreateThread(nullptr, 0, prefetchMain, nullptr, 0, nullptr);
    if (!prefetchWorker) {
        CloseHandle(prefetchEvent);
        prefetchEvent = nullptr;
        return false;
    }
    return true;
}

void requestPrefetch() {
    if (prefetchEvent) SetEvent(prefetchEvent);
}

void rebuildCatalog() {
    std::vector<std::pair<TrackSource, std::wstring>> roots;
    std::set<std::string> usedIds;

    if (manager.config.externalEnabled) {
        std::wstring root;
        if (manager.config.externalUseDefault) {
            if (getKnownMusicFolder(root)) roots.emplace_back(TrackSource::External, root);
        } else if (!manager.config.externalPath.empty()) {
            if (normalizeFullPath(manager.config.externalPath, root)) roots.emplace_back(TrackSource::External, root);
        }
    }
    std::wstring pluginLocal;
    if (manager.config.pluginLocalEnabled) {
        if (getModuleRelativePath(selfModule, L"Music", pluginLocal))
            roots.emplace_back(TrackSource::PluginLocal, pluginLocal);
        else if (host && host->log)
            host->log("[RedWolfRadio] Could not resolve plugin-local Music from the loaded DLL; source omitted.");
    }
    // RML's API4 compatibility host points baseDir/pluginDir at the loader's
    // shared folders, not the game executable or this plugin's installation.
    std::wstring originalRoot;
    if (host && getModuleRelativePath(static_cast<HMODULE>(host->exeModule),
            L"media_soviet\\Music", originalRoot))
        roots.emplace_back(TrackSource::Original, originalRoot);
    else if (host && host->log)
        host->log("[RedWolfRadio] Could not resolve original Music from the game executable; source omitted.");

    manager.catalog.clear();
    for (auto& entry : roots) {
        if (entry.second.empty()) continue;
        if (host && host->log) {
            std::string path;
            if (toUtf8(entry.second, path)) {
                const char* source = entry.first == TrackSource::Original ? "original" :
                    (entry.first == TrackSource::External ? "external" : "plugin-local");
                const DWORD attributes = GetFileAttributesW(entry.second.c_str());
                host->log("[RedWolfRadio] Source %s: %s (%s).", source, path.c_str(),
                    attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY) ?
                    "folder found" : "folder unavailable");
            }
        }
        collectTrackRecursively(entry.second, L"", entry.first,
            entry.first == TrackSource::Original ? true : (entry.first == TrackSource::External ? manager.config.externalRecursive : manager.config.pluginLocalRecursive),
            manager.catalog, usedIds);
    }
    manager.excludedOriginalIds.clear();
    for (const std::string& id : manager.config.excludedIds) manager.excludedOriginalIds.insert(id);
    manager.converted = 0;
    manager.cacheHits = 0;
    manager.rejected = 0;
    manager.nativeXwma = 0;
    manager.notificationText.clear(); manager.notificationUntil = 0;
    manager.trackSelectionDirty = true;
    manager.bagCursor = 0;
    manager.activeTrackId.clear();
    manager.lastPlayedId.clear();
    manager.history.clear();
    manager.historyCursor = manager.selectedHistoryCursor = kNoHistory;
    manager.historyRecorded = false;
    manager.navigationPending = manager.awaitingNavigationLoad = false;
    manager.navigationHistoryCursor = kNoHistory;
    manager.userPaused = manager.voicePaused = false;
    manager.deferredPlay = false;
    manager.pausedVoice = nullptr;
    manager.pendingNavigationAction = rwr::MusicAction::None;
    manager.previousPressAt = 0;
    manager.previousSequenceCursor = kNoHistory;
    manager.previousBurstDepth = 0;
    rebuildPlayBag();
    prepareInitialPrefetchWindow();
}

void logCatalogStats() {
    if (!host || !host->log) return;
    unsigned total = static_cast<unsigned>(manager.catalog.size());
    unsigned native = 0;
    for (const Track& track : manager.catalog) {
        if (track.nativePlayable) ++native;
    }
    const char* mode = "all";
    if (manager.config.originalsMode == OriginalsMode::Selected) mode = "selected";
    else if (manager.config.originalsMode == OriginalsMode::None) mode = "none";
    host->log("[RedWolfRadio] Catalog prepared: %u tracks total, %u native-capable. Originals mode: %s. Config malformed: %s",
        total, native, mode, manager.configMalformed ? "yes" : "no");
    host->log("[RedWolfRadio] Excluded originals: %zu", manager.excludedOriginalIds.size());
    for (const Track& track : manager.catalog) {
        if (track.source != TrackSource::Original) continue;
        const bool listed = manager.excludedOriginalIds.count(track.id) != 0;
        const char* reason = track.preparation == PreparationState::Rejected ? "unsupported original format" :
            (manager.config.originalsMode == OriginalsMode::None ? "originals disabled" :
             (manager.config.originalsMode == OriginalsMode::Selected && listed ? "selected exclusion" : "included"));
        host->log("[RedWolfRadio] Original ID: %s; eligible=%s; listedInExcludedIds=%s; %s.",
            track.id.c_str(), trackEligibleLocked(track) ? "yes" : "no", listed ? "yes" : "no", reason);
    }
    host->log("[RedWolfRadio] Cache: MaxSizeMiB=%llu, PrefetchTracks=%u, HistoryTracks=%u; startup prepared %u converted, %u cache hits, %u rejected.",
        static_cast<unsigned long long>(manager.config.cacheMaxSizeMiB), manager.config.prefetchTracks, manager.config.historyTracks,
        manager.converted, manager.cacheHits, manager.rejected);
    for (const std::string& warning : manager.config.hotkeyWarnings)
        host->log("[RedWolfRadio] %s", warning.c_str());
    host->log("[RedWolfRadio] Native custom XWMA prepared: %u (no WAV conversion).", manager.nativeXwma);
    host->log("[RedWolfRadio] Gameplay hotkeys: %s; bindings Next=%u/%u, Previous=%u/%u, PlayPause=%u/%u (key/modifiers).",
        manager.config.hotkeysEnabled ? "enabled" : "disabled",
        manager.config.hotkeys[0].key, manager.config.hotkeys[0].modifiers,
        manager.config.hotkeys[1].key, manager.config.hotkeys[1].modifiers,
        manager.config.hotkeys[2].key, manager.config.hotkeys[2].modifiers);
}

bool initializeCatalogFromConfig() {
    Config cfg;
    bool malformed = false;
    std::string error;
    if (!parseConfig(cfg, malformed, error)) {
        manager.configMalformed = malformed;
        if (host && host->log) {
            if (malformed) host->log("[RedWolfRadio] Music config malformed: %s", error.c_str());
            else host->log("[RedWolfRadio] Could not read music config; using defaults.");
        }
        if (malformed) return false;
    } else {
        manager.configMalformed = false;
    }
    manager.config = cfg;
    manager.configMalformed = malformed;
    manager.bannedTrackIds.clear();
    manager.runtimeEnabled = false;
    manager.silenceUntil = 0;
    manager.silenceTrackReady = false;
    manager.silenceTrackPath.clear();
    manager.cacheDirectory.clear();
    std::wstring storageRoot;
    if (getStorageRoot(storageRoot)) {
        storageRoot.push_back(L'\\');
        for (const char* c = kSilenceCacheFolder; *c; ++c) storageRoot.push_back(static_cast<wchar_t>(*c));
        if (ensureDirectory(storageRoot)) {
            manager.cacheDirectory = storageRoot;
            if (!prepareSilenceTrack(storageRoot)) {
                if (host && host->log) host->log("[RedWolfRadio] Failed to prepare built-in 1s silence cache file.");
            }
        } else if (host && host->log) {
            host->log("[RedWolfRadio] Failed to prepare silence cache folder.");
        }
    }
    rebuildCatalog();
    logCatalogStats();
    return !manager.configMalformed;
}

bool selectTrackForReplacement(const Track*& trackOut) {
    if (!manager.runtimeEnabled || manager.catalog.empty()) return false;
    const Track* selected = pickTrackLocked();
    if (!selected || !selected->nativePlayable) return false;
    const std::string& routed = selected->source == TrackSource::Original ? selected->relativeUtf8 : selected->preparedUtf8;
    if (routed.empty() || routed.size() >= 256 || routed.size() + 1 >= std::size(currentRedirectPath)) return false;
    if (selected->source != TrackSource::Original) memcpy(currentRedirectPath, routed.data(), routed.size() + 1);
    trackOut = selected;
    return true;
}

bool shouldEnforceNoOriginalsMode() {
    return manager.config.originalsMode == OriginalsMode::None;
}

void setRuntimeTrackFailureLocked(const Track& track) {
    if (manager.selectedHistoryCursor < manager.history.size()) {
        // A failed replay (including an original XWMA) must not remain the next
        // historical entry forever. It was attempted even though never played.
        manager.historyCursor = manager.selectedHistoryCursor;
        manager.selectedHistoryCursor = kNoHistory;
        manager.navigationPending = manager.awaitingNavigationLoad = false;
        manager.navigationHistoryCursor = kNoHistory;
    }
    if (track.source != TrackSource::Original) {
        manager.bannedTrackIds.insert(track.id);
        manager.trackSelectionDirty = true;
    }
}

struct Event {
    unsigned long long call;
    long long ticks;
    DWORD thread;
    uintptr_t caller;
    Kind kind;
    char phase;
    int arg0, arg1;
    uint32_t bits;
    char path[512];
};
constexpr unsigned kCapacity = 1024;
Event queue[kCapacity];
unsigned head = 0, tail = 0;

// Producers never wait for the writer and never write files or format strings.
// ReadProcessMemory safely bounds diagnostic path reads, including bad pointers.
void enqueue(Event& e, const char* path = nullptr) {
    const DWORD saved = GetLastError();
    if (recording.load(std::memory_order_relaxed)) {
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        e.ticks = now.QuadPart - epoch.QuadPart;
        e.thread = GetCurrentThreadId();
        if (path) {
            size_t i = 0;
            for (; i + 1 < sizeof(e.path); ++i) {
                SIZE_T read = 0;
                if (!ReadProcessMemory(GetCurrentProcess(), path + i, e.path + i, 1, &read) || read != 1) {
                    e.path[i] = 0; e.bits |= 1; break;
                }
                if (!e.path[i]) break;
            }
            if (i + 1 == sizeof(e.path)) { e.path[i] = 0; e.bits |= 2; }
        } else if (e.kind == Load && e.phase == 'B') { e.bits |= 4; }
        if (TryAcquireSRWLockExclusive(&queueLock)) {
            if (head - tail < kCapacity) queue[head++ % kCapacity] = e;
            else dropped.fetch_add(1, std::memory_order_relaxed);
            ReleaseSRWLockExclusive(&queueLock);
        } else dropped.fetch_add(1, std::memory_order_relaxed);
    }
    SetLastError(saved);
}

Event begin(Kind k, void* caller) {
    Event e{}; e.kind = k; e.phase = 'B'; e.caller = reinterpret_cast<uintptr_t>(caller);
    e.call = nextCall.fetch_add(1, std::memory_order_relaxed) + 1;
    counts[k].fetch_add(1, std::memory_order_relaxed);
    return e;
}

#define VOID_WRAPPER(fn, kind) \
void fn() { \
    Event e = begin(kind, __builtin_return_address(0)); enqueue(e); \
    reinterpret_cast<VoidFn>(originals[kind])(); \
    e.phase = 'E'; enqueue(e); \
}
VOID_WRAPPER(onInitialize, Init)
VOID_WRAPPER(onPause, Pause)
VOID_WRAPPER(onResume, Resume)
#undef VOID_WRAPPER

void onStop() {
    pendingTrackRecovery.store(0, std::memory_order_relaxed);
    activeTrackRecovery.store(0, std::memory_order_relaxed);
    recoveryDeadlineTicks.store(0, std::memory_order_relaxed);
    manager.silenceUntil = 0;
    AcquireSRWLockExclusive(&stateLock);
    manager.activeTrackPending = false;
    manager.activeTrackPlaying = false;
    manager.activeTrackId.clear();
    manager.notificationText.clear(); manager.notificationUntil = 0;
    manager.voicePaused = false;
    manager.pausedVoice = nullptr;
    manager.deferredPlay = false;
    if (!manager.awaitingNavigationLoad) {
        manager.userPaused = false;
        manager.navigationPending = false;
        manager.navigationHistoryCursor = kNoHistory;
        manager.previousPressAt = 0;
        manager.previousSequenceCursor = kNoHistory;
        manager.previousBurstDepth = 0;
    }
    ReleaseSRWLockExclusive(&stateLock);
    Event e = begin(Stop, __builtin_return_address(0)); enqueue(e);
    reinterpret_cast<VoidFn>(originals[Stop])();
    e.phase = 'E'; enqueue(e);
}
void onResolvePath(char* destination, const char* source) {
    const DWORD saved = GetLastError();
    if (destination && source) {
        if (strcmp(source, kSentinelPath) == 0) {
            snprintf(destination, 256, "%s", currentRedirectPath);
            SetLastError(saved);
            return;
        }
    }
    originalPathResolver(destination, source);
}
void onLoad(char* path) {
    void* caller = __builtin_return_address(0);
    Event e = begin(Load, caller); enqueue(e, path);
    char* forwarded = path;
    const Track* selected = nullptr;
    if (manager.runtimeEnabled && redirectReady.load(std::memory_order_relaxed)) {
        musicThreadId.store(GetCurrentThreadId(), std::memory_order_relaxed);
        AcquireSRWLockExclusive(&stateLock);
        if (selectTrackForReplacement(selected)) {
            manager.activeTrackSource = selected->source;
            manager.activeTrackPending = true;
            manager.activeTrackPlaying = false;
            manager.activeLoadStart = GetTickCount64();
            manager.lastPlayedId = selected->id;
            Event substitution = e; substitution.phase = 'S'; substitution.arg0 = 1;
            if (selected->source == TrackSource::Original) {
                enqueue(substitution, selected->relativeUtf8.c_str());
                forwarded = const_cast<char*>(selected->relativeUtf8.c_str());
                pendingTrackRecovery.store(0, std::memory_order_relaxed);
            } else {
                // Log the stable catalog ID rather than the internal resolver
                // sentinel so a playback report identifies the exact source.
                enqueue(substitution, selected->id.c_str());
                forwarded = const_cast<char*>(kSentinelPath);
                pendingTrackRecovery.store(1, std::memory_order_relaxed);
            }
            manager.silenceUntil = 0;
        } else if (shouldEnforceNoOriginalsMode() &&
            manager.silenceTrackReady && !manager.silenceTrackPath.empty()) {
            const size_t silenceLength = manager.silenceTrackPath.size() + 1;
            if (silenceLength < 256 && silenceLength < std::size(currentRedirectPath)) {
                memcpy(currentRedirectPath, manager.silenceTrackPath.data(), silenceLength);
                Event substitution = e; substitution.phase = 'S'; substitution.arg0 = 2;
                enqueue(substitution, kSentinelPath);
                forwarded = const_cast<char*>(kSentinelPath);
                manager.silenceUntil = GetTickCount64() + 1500;
                pendingTrackRecovery.store(0, std::memory_order_relaxed);
            }
        }
        ReleaseSRWLockExclusive(&stateLock);
        requestPrefetch();
    }
    reinterpret_cast<LoadFn>(originals[Load])(forwarded);
    e.phase = 'E'; enqueue(e);
}
void onPlay(bool arg0, int arg1) {
    const DWORD saved = GetLastError();
    Event e = begin(Play, __builtin_return_address(0)); e.arg0 = arg0; e.arg1 = arg1; enqueue(e);
    if (manager.runtimeEnabled) {
        AcquireSRWLockExclusive(&stateLock);
        if (manager.userPaused && !manager.activeTrackId.empty()) {
            manager.deferredPlay = true;
            manager.deferredPlayLoop = arg0;
            manager.deferredPlayDelay = arg1;
            // A selected song is part of navigation history even when the user
            // has elected to keep it paused at the beginning.
            recordPlayingHistoryLocked();
            ReleaseSRWLockExclusive(&stateLock);
            e.phase = 'D'; enqueue(e);
            SetLastError(saved);
            return;
        }
        ReleaseSRWLockExclusive(&stateLock);
    }
    SetLastError(saved);
    reinterpret_cast<PlayFn>(originals[Play])(arg0, arg1);
    const unsigned recovery = pendingTrackRecovery.exchange(0, std::memory_order_relaxed);
    if (recovery) {
        LARGE_INTEGER now; QueryPerformanceCounter(&now);
        activeTrackRecovery.store(recovery, std::memory_order_relaxed);
        recoveryDeadlineTicks.store(now.QuadPart + (frequency.QuadPart * kCatalogRecoveryMilliseconds) / 1000, std::memory_order_relaxed);
    }
    e.phase = 'E'; enqueue(e);
}
void onVolume(float value) {
    Event e = begin(Volume, __builtin_return_address(0));
    memcpy(&e.bits, &value, sizeof(value));
    const bool changed = lastVolume.exchange(e.bits, std::memory_order_relaxed) != e.bits;
    if (changed) enqueue(e);
    reinterpret_cast<VolumeFn>(originals[Volume])(value);
    if (changed) { e.phase = 'E'; enqueue(e); }
}

rwr::MusicAction pollMusicHotkey() {
    // Only the thread that loads music may operate on the engine voice. No
    // process-wide hotkey registration or engine calls from worker threads.
    if (!manager.runtimeEnabled || !manager.config.hotkeysEnabled ||
        musicThreadId.load(std::memory_order_relaxed) != GetCurrentThreadId()) return rwr::MusicAction::None;
    static ULONGLONG lastPoll = 0;
    const ULONGLONG now = GetTickCount64();
    if (lastPoll && now - lastPoll < 15) return rwr::MusicAction::None;
    if (!lastPoll || now - lastPoll > 500) hotkeyTracker = {};
    lastPoll = now;
    const HWND foreground = GetForegroundWindow();
    DWORD foregroundPid = 0;
    if (foreground) GetWindowThreadProcessId(foreground, &foregroundPid);
    const bool focused = foreground && foregroundPid == GetCurrentProcessId();
    std::array<bool, 256> down{};
    if (focused) {
        for (unsigned key : {VK_CONTROL, VK_SHIFT, VK_MENU, VK_LWIN, VK_RWIN})
            down[key] = (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
        for (const rwr::Hotkey& binding : manager.config.hotkeys)
            if (binding.key && binding.key < down.size())
                down[binding.key] = (GetAsyncKeyState(static_cast<int>(binding.key)) & 0x8000) != 0;
    }
    return hotkeyTracker.sample(focused, down, manager.config.hotkeys);
}

void logMusicControl(rwr::MusicAction action, int outcome, const char* detail = nullptr) {
    Event event = begin(action == rwr::MusicAction::PlayPause ? Pause : Stop, nullptr);
    event.phase = 'H';
    event.arg0 = static_cast<int>(action);
    event.arg1 = outcome;
    enqueue(event, detail);
}

int processMusicControls(rwr::MusicAction action, int status, bool loading, ULONGLONG now = GetTickCount64()) {
    bool stop = false;
    bool changePause = false;
    bool desiredPause = false;
    void* expectedVoice = nullptr;
    bool prefetch = false;
    bool startDeferred = false;
    bool deferredLoop = false;
    int deferredDelay = 0;
    rwr::MusicAction navigation = rwr::MusicAction::None;
    AcquireSRWLockExclusive(&stateLock);
    if (action == rwr::MusicAction::PlayPause) {
        if (!manager.activeTrackId.empty() || manager.awaitingNavigationLoad) {
            manager.userPaused = !manager.userPaused;
            logMusicControl(action, 2, manager.userPaused ? "pause requested" : "resume requested");
        } else logMusicControl(action, 0, "no active track");
    } else if (action != rwr::MusicAction::None) {
        const size_t previousTarget = (manager.navigationPending || manager.awaitingNavigationLoad) ?
            manager.navigationHistoryCursor : kNoHistory;
        const bool unrecordedSelection = manager.activeTrackPending && !manager.historyRecorded;
        const bool newSelectionPending = previousTarget == kNoHistory &&
            ((unrecordedSelection && manager.selectedHistoryCursor == kNoHistory) || manager.awaitingNavigationLoad);
        manager.navigationPending = !manager.awaitingNavigationLoad;
        manager.pendingNavigationAction = action;
        manager.navigationHistoryCursor = kNoHistory;
        if (action == rwr::MusicAction::Previous) {
            if (manager.previousPressAt && now - manager.previousPressAt <= kPreviousRepeatMilliseconds) {
                manager.previousBurstDepth = std::min(manager.previousBurstDepth + 1, kHistoryLimit);
            } else {
                manager.previousSequenceCursor = manager.activeTrackPending && !manager.historyRecorded ?
                    kNoHistory : manager.historyCursor;
                manager.previousBurstDepth = 0;
            }
            manager.previousPressAt = now;
            if (manager.previousSequenceCursor != kNoHistory)
                manager.navigationHistoryCursor = manager.previousSequenceCursor > manager.previousBurstDepth ?
                    manager.previousSequenceCursor - manager.previousBurstDepth : 0;
        } else {
            manager.previousPressAt = 0;
            manager.previousSequenceCursor = kNoHistory;
            manager.previousBurstDepth = 0;
            if (action == rwr::MusicAction::Restart || action == rwr::MusicAction::PreviousTrack) {
                const size_t selected = previousTarget != kNoHistory ? previousTarget :
                    (unrecordedSelection && manager.selectedHistoryCursor != kNoHistory ?
                        manager.selectedHistoryCursor : manager.historyCursor);
                if (action == rwr::MusicAction::Restart) {
                    // A new, unrecorded load has no history slot yet. Resolve
                    // after the loader finishes rather than restarting the old song.
                    if (!newSelectionPending) manager.navigationHistoryCursor = selected;
                } else if (selected != kNoHistory) {
                    // During a new load, the history cursor already denotes
                    // the preceding song. During replay it denotes that selection.
                    manager.navigationHistoryCursor = newSelectionPending ? selected :
                        (selected ? selected - 1 : 0);
                }
            } else if (manager.historyCursor != kNoHistory && manager.historyCursor + 1 < manager.history.size())
                manager.navigationHistoryCursor = manager.historyCursor + 1;
        }
        prefetch = true;
    }
    if (manager.navigationPending && !loading) {
        navigation = manager.pendingNavigationAction;
        size_t replay = kNoHistory;
        if (navigation == rwr::MusicAction::Previous) {
            if (manager.previousSequenceCursor == kNoHistory) manager.previousSequenceCursor = manager.historyCursor;
            if (manager.previousSequenceCursor != kNoHistory) {
                replay = manager.previousSequenceCursor > manager.previousBurstDepth ?
                    manager.previousSequenceCursor - manager.previousBurstDepth : 0;
            } else {
                manager.navigationPending = false;
                logMusicControl(navigation, 0, "no playback history");
            }
        } else if (navigation == rwr::MusicAction::Restart || navigation == rwr::MusicAction::PreviousTrack) {
            replay = manager.navigationHistoryCursor != kNoHistory ? manager.navigationHistoryCursor : manager.historyCursor;
            if (replay == kNoHistory) {
                manager.navigationPending = false;
                logMusicControl(navigation, 0, "no playback history");
            }
        } else if (manager.historyCursor != kNoHistory && manager.historyCursor + 1 < manager.history.size()) {
            replay = manager.historyCursor + 1;
        }
        manager.navigationHistoryCursor = replay;
        if (manager.navigationPending) {
            if (replay != kNoHistory) {
                const Track* target = historyTrackLocked(replay);
                if (!target) {
                    manager.navigationPending = false;
                    logMusicControl(navigation, 0, "history track unavailable");
                } else stop = target->nativePlayable;
            } else {
                ensurePlayBagLocked();
                for (size_t i = manager.bagCursor; i < manager.playBag.size(); ++i)
                    if (manager.catalog[manager.playBag[i]].nativePlayable) { stop = true; break; }
                if (manager.playBag.empty()) {
                    manager.navigationPending = false;
                    logMusicControl(navigation, 0, "no eligible tracks");
                }
            }
            prefetch = true;
        }
        if (stop) {
            manager.navigationPending = false;
            manager.awaitingNavigationLoad = true;
            manager.activeTrackId.clear();
            manager.activeTrackPending = manager.activeTrackPlaying = false;
            manager.voicePaused = false;
            manager.pausedVoice = nullptr;
            manager.deferredPlay = false;
            manager.silenceUntil = 0;
        }
    }
    if (!stop && !loading && manager.deferredPlay && !manager.userPaused) {
        startDeferred = true;
        deferredLoop = manager.deferredPlayLoop;
        deferredDelay = manager.deferredPlayDelay;
        manager.deferredPlay = false;
    }
    if (!stop && !startDeferred && !manager.deferredPlay && !loading && status && !manager.activeTrackId.empty() &&
        manager.userPaused != manager.voicePaused) {
        changePause = true;
        desiredPause = manager.userPaused;
        expectedVoice = manager.pausedVoice;
    }
    ReleaseSRWLockExclusive(&stateLock);

    if (stop) {
        pendingTrackRecovery.store(0, std::memory_order_relaxed);
        activeTrackRecovery.store(0, std::memory_order_relaxed);
        recoveryDeadlineTicks.store(0, std::memory_order_relaxed);
        reinterpret_cast<VoidFn>(originals[Stop])();
        logMusicControl(navigation, 1, "advance requested from game scheduler");
        status = 0;
    } else if (startDeferred) {
        onPlay(deferredLoop, deferredDelay);
        logMusicControl(rwr::MusicAction::PlayPause, 1, "started selected paused track");
        status = 1;
    } else if (changePause) {
        const auto result = rwr::setMusicVoicePaused(static_cast<HMODULE>(host->engineModule),
            engineLoadingFlag, desiredPause, expectedVoice);
        AcquireSRWLockExclusive(&stateLock);
        if (result.status == rwr::VoiceControlStatus::Applied) {
            manager.voicePaused = desiredPause;
            manager.pausedVoice = desiredPause ? result.voice : nullptr;
            logMusicControl(rwr::MusicAction::PlayPause, 1, desiredPause ? "paused at current position" : "resumed");
        } else if (result.status != rwr::VoiceControlStatus::Loading) {
            manager.userPaused = manager.voicePaused;
            logMusicControl(rwr::MusicAction::PlayPause, -1, "voice control unavailable");
        }
        ReleaseSRWLockExclusive(&stateLock);
    }
    if (prefetch) requestPrefetch();
    return status;
}

void notifyTrackStartedLocked(ULONGLONG now) {
    manager.notificationText.clear(); manager.notificationUntil = 0;
    if (!manager.config.showTrackName) return;
    for (const Track& track : manager.catalog) {
        if (track.id == manager.activeTrackId) {
            manager.notificationText = track.displayName;
            manager.notificationUntil = now + 5000;
            return;
        }
    }
}

// Private native dialog ABI verified against the hash-guarded game build.
// See evidence/menu-hook-research/native-dialog-route.md for callers and copies.
using NativeDialogFn = void* (*)(void*, int, void*, int, int, int, int, const wchar_t*, void*);
std::atomic<NativeDialogFn> previousNativeDialog{nullptr};
std::atomic<bool> nativePlayerReady{false};
constexpr size_t kNativeDialogRva = 0x6ee2a0;
constexpr size_t kScriptDialogReturnRva = 0x5a0454;
constexpr unsigned char kNativeDialogPrefix[14] = {
    0x48,0x8b,0xc4,0x44,0x89,0x48,0x20,0x89,0x50,0x10,0x48,0x89,0x48,0x08};
constexpr unsigned char kScriptDialogCall[5] = {0xe8,0x4c,0xde,0x14,0x00};

void currentNativePlayerSnapshot(rwr::NativePlayerSnapshot& result) {
    result = {};
    result.state = rwr::SongPopupState::Unavailable;
    // This optional UI must never wait on the music thread or allocate under its lock.
    if (!TryAcquireSRWLockShared(&stateLock)) {
        result.state = rwr::SongPopupState::Busy;
        return;
    }
    result.russian = manager.config.displayRussian;
    if (manager.runtimeEnabled) {
        result.state = rwr::SongPopupState::Stopped;
        for (const Track& track : manager.catalog) {
            if (manager.activeTrackId.empty() || track.id != manager.activeTrackId) continue;
            const size_t count = std::min(track.displayName.size(), rwr::kSongPopupLimit);
            std::copy_n(track.displayName.data(), count, result.name.data());
            result.state = manager.userPaused || manager.voicePaused ? rwr::SongPopupState::Paused :
                manager.activeTrackPending || !manager.activeTrackPlaying ? rwr::SongPopupState::Loading :
                rwr::SongPopupState::Playing;
            break;
        }
    }
    ReleaseSRWLockShared(&stateLock);
}
rwr::SongPopupText currentSongPopupText() {
    rwr::NativePlayerSnapshot snapshot{};
    currentNativePlayerSnapshot(snapshot);
    return rwr::songPopupText(snapshot.name.data(), snapshot.state, snapshot.russian);
}

void* dispatchNativeSongDialog(const void* caller, void* game, int arg2, void* arg3,
    int arg4, int arg5, int dialogType, int titleId, const wchar_t* text, void* arg9) {
    const DWORD incomingError = GetLastError();
    const auto continuation = previousNativeDialog.load(std::memory_order_acquire);
    if (!continuation) return nullptr;
    // Exact caller + tuple avoid examining text in other native dialog variants.
    const bool ownCall = host && host->exeModule && host->readablePtr && game &&
        caller == reinterpret_cast<unsigned char*>(host->exeModule) + kScriptDialogReturnRva &&
        arg2 == 1 && !arg3 && arg4 == 4 && arg5 == 0 && dialogType == 1 && titleId == 0x983e && !arg9;
    if (ownCall && text && host->readablePtr(text, sizeof(rwr::kSongPopupMarker)) &&
        !memcmp(text, rwr::kSongPopupMarker, sizeof(rwr::kSongPopupMarker))) {
        const auto snapshot = currentSongPopupText();
        const bool live = nativePlayerReady.load(std::memory_order_acquire);
        if (host->log) host->log("[RedWolfRadio] Native song window requested; live player=%d.", live ? 1 : 0);
        SetLastError(incomingError);
        // The native routine copies text before returning. Never retain its object pointer.
        void* window = continuation(game, arg2, arg3, arg4, live ? 0x525752 : arg5,
            dialogType, titleId, snapshot.data(), arg9);
        const DWORD outgoingError = GetLastError();
        if (live && window) rwr::markNativePlayerWindow(window);
        SetLastError(outgoingError);
        return window;
    }
    SetLastError(incomingError);
    return continuation(game, arg2, arg3, arg4, arg5, dialogType, titleId, text, arg9);
}

__attribute__((noinline)) void* onNativeSongDialog(void* game, int arg2, void* arg3,
    int arg4, int arg5, int dialogType, int titleId, const wchar_t* text, void* arg9) {
    return dispatchNativeSongDialog(__builtin_return_address(0), game, arg2, arg3,
        arg4, arg5, dialogType, titleId, text, arg9);
}

bool installNativeSongPopup() {
    if (!manager.config.nativeSongPopup || !host || !host->exeModule || !host->readablePtr ||
        !host->installInlineHook || host->exeSize < kNativeDialogRva + sizeof(kNativeDialogPrefix)) return false;
    auto* base = reinterpret_cast<unsigned char*>(host->exeModule);
    auto* target = base + kNativeDialogRva;
    auto* call = base + kScriptDialogReturnRva - sizeof(kScriptDialogCall);
    // Refuse unknown ownership/changed code before a host hook request, which can abort launch.
    if (!host->readablePtr(target, sizeof(kNativeDialogPrefix)) ||
        memcmp(target, kNativeDialogPrefix, sizeof(kNativeDialogPrefix)) ||
        !host->readablePtr(call, sizeof(kScriptDialogCall)) ||
        memcmp(call, kScriptDialogCall, sizeof(kScriptDialogCall))) return false;
    // RML leaves the game thread running while installing hooks. Publish a
    // complete private continuation before the host can expose this detour.
    // All four stolen instructions are relocation-free. FF25 preserves RAX.
    constexpr size_t continuationSize = sizeof(kNativeDialogPrefix) + 14;
    auto* trampoline = static_cast<unsigned char*>(VirtualAlloc(nullptr, continuationSize,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!trampoline) return false;
    memcpy(trampoline, kNativeDialogPrefix, sizeof(kNativeDialogPrefix));
    auto* jump = trampoline + sizeof(kNativeDialogPrefix);
    jump[0] = 0xff; jump[1] = 0x25; memset(jump + 2, 0, 4);
    const auto destination = reinterpret_cast<uintptr_t>(target + sizeof(kNativeDialogPrefix));
    memcpy(jump + 6, &destination, sizeof(destination));
    DWORD oldProtection = 0;
    if (!VirtualProtect(trampoline, continuationSize, PAGE_EXECUTE_READ, &oldProtection) ||
        !FlushInstructionCache(GetCurrentProcess(), trampoline, continuationSize)) {
        VirtualFree(trampoline, 0, MEM_RELEASE); return false;
    }
    previousNativeDialog.store(reinterpret_cast<NativeDialogFn>(trampoline), std::memory_order_release);
    // Keep the private continuation for process lifetime, even on host failure:
    // a published callback must never observe freed forwarding code.
    void* hostContinuation = nullptr;
    if (!host->installInlineHook(target, reinterpret_cast<void*>(onNativeSongDialog), &hostContinuation,
        kNativeDialogPrefix, sizeof(kNativeDialogPrefix), "Red Wolf Radio optional native song popup")) return false;
    if (host->log) host->log("[RedWolfRadio] Native song dialog adapter installed.");
    return true;
}

using NativePlayerFrameFn = void (*)(void*, void*, int);
using NativePlayerBodyFn = void (*)(void*, void*);
using NativePlayerHitFn = bool (*)(void*, void*);
std::atomic<NativePlayerFrameFn> previousNativePlayerFrame{nullptr};
std::atomic<NativePlayerBodyFn> previousNativePlayerBody{nullptr};
std::atomic<NativePlayerHitFn> previousNativePlayerHit{nullptr};
constexpr size_t kNativePlayerFrameRva = 0x6f0070;
constexpr size_t kNativePlayerBodyRva = 0x7f2660;
constexpr size_t kNativePlayerHitRva = 0x6ef830;
constexpr unsigned char kNativePlayerFramePrefix[] = {
    0x48,0x8b,0xc4,0x55,0x53,0x41,0x55,0x41,0x56,0x41,0x57,0x48,0x8d,0xa8,0x28,0xfb,0xff,0xff};
constexpr unsigned char kNativePlayerBodyPrefix[] = {
    0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x6c,0x24,0x18,0x48,0x89,0x74,0x24,0x20};
constexpr unsigned char kNativePlayerHitPrefix[] = {
    0x48,0x8b,0xc4,0x55,0x53,0x56,0x48,0x8d,0x68,0xa1,0x48,0x81,0xec,0xd0,0x00,0x00,0x00};

void onNativePlayerFrame(void* game, void* dialog, int index) {
    const DWORD incoming = GetLastError();
    if (nativePlayerReady.load(std::memory_order_acquire) && rwr::isNativePlayerWindow(dialog)) {
        rwr::drawNativePlayerFrame(game, dialog, index);
        SetLastError(incoming);
        return;
    }
    const auto next = previousNativePlayerFrame.load(std::memory_order_acquire);
    SetLastError(incoming);
    if (next) next(game, dialog, index);
}
void onNativePlayerBody(void* game, void* dialog) {
    const DWORD incoming = GetLastError();
    const bool owned = nativePlayerReady.load(std::memory_order_acquire) && rwr::isNativePlayerWindow(dialog);
    SetLastError(incoming);
    if (owned) return; // The compact frame draws its own body and controls.
    const auto next = previousNativePlayerBody.load(std::memory_order_acquire);
    if (next) next(game, dialog);
}
bool onNativePlayerHit(void* game, void* dialog) {
    const DWORD incoming = GetLastError();
    if (nativePlayerReady.load(std::memory_order_acquire) && rwr::isNativePlayerWindow(dialog)) {
        const bool hit = rwr::nativePlayerHit(game, dialog);
        SetLastError(incoming);
        return hit;
    }
    const auto next = previousNativePlayerHit.load(std::memory_order_acquire);
    SetLastError(incoming);
    return next ? next(game, dialog) : false;
}

template<size_t N> bool nativePlayerPrefixMatches(size_t rva, const unsigned char (&prefix)[N]) {
    return host && host->exeModule && host->readablePtr && rva <= host->exeSize &&
        N <= host->exeSize - rva && host->readablePtr(static_cast<unsigned char*>(host->exeModule) + rva, N) &&
        !memcmp(static_cast<unsigned char*>(host->exeModule) + rva, prefix, N);
}
template<class Fn, size_t N> bool prepareNativePlayerContinuation(size_t rva,
    const unsigned char (&prefix)[N], std::atomic<Fn>& destination) {
    // These verified prefixes contain complete relocation-free instructions.
    auto* code = static_cast<unsigned char*>(VirtualAlloc(nullptr, N + 14,
        MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!code) return false;
    memcpy(code, prefix, N);
    code[N] = 0xff; code[N + 1] = 0x25;
    const auto resume = reinterpret_cast<uintptr_t>(host->exeModule) + rva + N;
    memcpy(code + N + 6, &resume, sizeof(resume));
    DWORD old = 0;
    if (!VirtualProtect(code, N + 14, PAGE_EXECUTE_READ, &old) ||
        !FlushInstructionCache(GetCurrentProcess(), code, N + 14)) {
        VirtualFree(code, 0, MEM_RELEASE); return false;
    }
    // Kept for process lifetime: callbacks may already be published by RML.
    destination.store(reinterpret_cast<Fn>(code), std::memory_order_release);
    return true;
}
template<class Fn, size_t N> bool installNativePlayerHook(size_t rva,
    const unsigned char (&prefix)[N], Fn detour, const char* label) {
    void* ignored = nullptr;
    return host->installInlineHook(static_cast<unsigned char*>(host->exeModule) + rva,
        reinterpret_cast<void*>(detour), &ignored, prefix, N, label) != 0;
}
bool installNativePlayer() {
    if (!manager.config.nativeSongPopup || !host || !host->installInlineHook ||
        !nativePlayerPrefixMatches(kNativeDialogRva, kNativeDialogPrefix) ||
        !nativePlayerPrefixMatches(kScriptDialogReturnRva - sizeof(kScriptDialogCall), kScriptDialogCall) ||
        !nativePlayerPrefixMatches(kNativePlayerFrameRva, kNativePlayerFramePrefix) ||
        !nativePlayerPrefixMatches(kNativePlayerBodyRva, kNativePlayerBodyPrefix) ||
        !nativePlayerPrefixMatches(kNativePlayerHitRva, kNativePlayerHitPrefix) ||
        !rwr::initializeNativePlayerUi(host, currentNativePlayerSnapshot, rwr::queueUiMusicAction)) return false;
    // Prepublish every forwarding path before the first hook request. A partially
    // installed set only forwards foreign windows; creation/ownership is last.
    if (!prepareNativePlayerContinuation(kNativePlayerFrameRva, kNativePlayerFramePrefix, previousNativePlayerFrame) ||
        !prepareNativePlayerContinuation(kNativePlayerBodyRva, kNativePlayerBodyPrefix, previousNativePlayerBody) ||
        !prepareNativePlayerContinuation(kNativePlayerHitRva, kNativePlayerHitPrefix, previousNativePlayerHit)) return false;
    if (!installNativePlayerHook(kNativePlayerFrameRva, kNativePlayerFramePrefix, onNativePlayerFrame, "Red Wolf Radio native player frame") ||
        !installNativePlayerHook(kNativePlayerBodyRva, kNativePlayerBodyPrefix, onNativePlayerBody, "Red Wolf Radio native player body") ||
        !installNativePlayerHook(kNativePlayerHitRva, kNativePlayerHitPrefix, onNativePlayerHit, "Red Wolf Radio native player hit region")) return false;
    nativePlayerReady.store(true, std::memory_order_release);
    if (!installNativeSongPopup()) { nativePlayerReady.store(false, std::memory_order_release); return false; }
    if (host->log) host->log("[RedWolfRadio] Live compact native player installed; automatic refresh and four playback controls.");
    return true;
}

using HudPrintFn = void (*)(void*, void*, float, float, unsigned long, const wchar_t*);
using FrameFn = void (*)(void*);
HudPrintFn hudPrint = nullptr;
FrameFn previousFrame = nullptr;
thread_local bool insideHudFrame = false;

void renderTrackNotification() {
    std::wstring text;
    bool russian = false;
    AcquireSRWLockShared(&stateLock);
    if (manager.runtimeEnabled && manager.config.showTrackName &&
        !manager.activeTrackId.empty() && GetTickCount64() < manager.notificationUntil) {
        text = manager.notificationText; russian = manager.config.displayRussian;
    }
    ReleaseSRWLockShared(&stateLock);
    if (text.empty() || !host || !host->readablePtr || !hudPrint) return;
    auto* base = reinterpret_cast<unsigned char*>(host->exeModule);
    auto* fontManager = base + 0x996fb0;
    auto** fontSlot = reinterpret_cast<void**>(base + 0x994200);
    auto* width = reinterpret_cast<int*>(base + 0x99528c);
    auto* height = reinterpret_cast<int*>(base + 0x995274);
    auto* scale = reinterpret_cast<float*>(base + 0x992088);
    if (!host->readablePtr(fontManager, 8) || !host->readablePtr(fontSlot, sizeof(void*)) ||
        !host->readablePtr(width, 4) || !host->readablePtr(height, 4) || !host->readablePtr(scale, 4) ||
        !*fontSlot || !host->readablePtr(*fontSlot, 8) || *width < 320 || *width > 20000 ||
        *height < 200 || *height > 20000 || !(*scale >= .25f && *scale <= 8.f)) return;
    const float x = 20.f * *scale;
    const float y = static_cast<float>(*height) * .78f;
    const size_t limit = static_cast<size_t>(std::max(2.f, std::min(80.f, (*width - x * 2) / (12.f * *scale))));
    text = rwr::notificationName(std::move(text), limit);
    auto line = [&](float lineY, const wchar_t* value) {
        hudPrint(fontManager, *fontSlot, x + 1, lineY + 1, 0xe0000000u, value);
        hudPrint(fontManager, *fontSlot, x, lineY, 0xffffffffu, value);
    };
    line(y, russian ? L"\u0421\u0435\u0439\u0447\u0430\u0441 \u0438\u0433\u0440\u0430\u0435\u0442" : L"Now playing");
    line(y + 20.f * *scale, text.c_str());
}

void onHudFrame(void* controller) {
    if (!previousFrame) return;
    if (insideHudFrame) { previousFrame(controller); return; }
    insideHudFrame = true;
    previousFrame(controller);
    const DWORD after = GetLastError();
    renderTrackNotification();
    insideHudFrame = false;
    // Drawing must not change the native frame's observable last error.
    SetLastError(after);
}

// ABI prefixes from ForceNodes' published v1 service. Only the common header
// and registerFrameClient member are accessed; no implementation is copied.
struct FrameContextPrefix { unsigned structSize, phase; };
using FrameClientFn = void (*)(const FrameContextPrefix*, void*);
struct FrameServicePrefix {
    unsigned structSize, apiVersion;
    int (*registerFrameClient)(FrameClientFn, void*);
};
void notificationFrameClient(const FrameContextPrefix* frame, void*) {
    if (frame && frame->structSize >= sizeof(FrameContextPrefix) && frame->phase == 1) {
        const DWORD saved = GetLastError(); renderTrackNotification(); SetLastError(saved);
    }
}

bool installNotificationRenderer() {
    if (!manager.config.showTrackName) return false;
    if (!host || !host->readablePtr) return false;
    const char* name = "?PrintLeftUnicodeNoArg@C3D_FONTMANAGER@@QEAAXPEAVC3D_FONT@@MMKPEB_W@Z";
    void** slot = host->findIatSlot(host->exeModule, "C3DDLL64.dll", name);
    hudPrint = slot && host->readablePtr(slot, sizeof(void*)) ? reinterpret_cast<HudPrintFn>(*slot) : nullptr;
    if (!hudPrint) return false;
    if (host->consume) {
        auto* service = static_cast<const FrameServicePrefix*>(host->consume("ForceNodes", 1));
        if (service && host->readablePtr(service, sizeof(FrameServicePrefix)) &&
            service->apiVersion == 1 && service->structSize >= sizeof(FrameServicePrefix) &&
            service->registerFrameClient && service->registerFrameClient(notificationFrameClient, nullptr)) {
            host->log("[RedWolfRadio] Track HUD registered with ForceNodes frame service."); return true;
        }
    }
    constexpr unsigned char prefix[14] = {0x48,0x8b,0xc4,0x48,0x89,0x58,0x20,0x55,0x56,0x57,0x41,0x55,0x41,0x56};
    auto* target = reinterpret_cast<unsigned char*>(host->exeModule) + 0x2f0f10;
    // Leave an unknown owner alone. Refusing before the host hook request also
    // avoids turning an optional display conflict into an RML launch failure.
    if (!host->readablePtr(target, sizeof(prefix)) || memcmp(target, prefix, sizeof(prefix))) return false;
    void* continuation = nullptr;
    if (!host->installInlineHook(target, reinterpret_cast<void*>(onHudFrame), &continuation,
        prefix, sizeof(prefix), "Red Wolf Radio optional track HUD")) return false;
    previousFrame = reinterpret_cast<FrameFn>(continuation);
    host->log("[RedWolfRadio] Track HUD installed at the verified gameplay frame boundary.");
    return previousFrame != nullptr;
}

int onPlaying() {
    Event e = begin(Playing, __builtin_return_address(0));
    int result = reinterpret_cast<PlayingFn>(originals[Playing])();
    const DWORD statusError = GetLastError();
    const bool controlThread = manager.runtimeEnabled &&
        musicThreadId.load(std::memory_order_relaxed) == GetCurrentThreadId();
    rwr::MusicAction action = controlThread ? pollMusicHotkey() : rwr::MusicAction::None;
    // A sampled key edge takes this poll; queued UI commands remain FIFO for
    // subsequent polls. UI controls also work with keyboard shortcuts disabled.
    if (controlThread && action == rwr::MusicAction::None) action = rwr::popUiMusicAction();
    const ULONGLONG nowMs = GetTickCount64();
    bool holdNoOriginalsHold = false;
    if (manager.runtimeEnabled && shouldEnforceNoOriginalsMode() && nowMs < manager.silenceUntil) {
        AcquireSRWLockExclusive(&stateLock);
        if (manager.trackSelectionDirty) rebuildPlayBag();
        if (manager.playBag.empty()) holdNoOriginalsHold = true;
        ReleaseSRWLockExclusive(&stateLock);
    }
    if (holdNoOriginalsHold) result = 1;
    unsigned recovery = activeTrackRecovery.load(std::memory_order_relaxed);
    if (recovery) {
        const ULONGLONG deadline = recoveryDeadlineTicks.load(std::memory_order_relaxed);
        const bool shouldTimeout = result && ( [&] {
            LARGE_INTEGER now; QueryPerformanceCounter(&now);
            return now.QuadPart >= static_cast<long long>(deadline);
        }());
        if (shouldTimeout) {
            const char loading = engineLoadingFlag ? __atomic_load_n(engineLoadingFlag, __ATOMIC_SEQ_CST) : 0;
            if (!loading) {
                if (activeTrackRecovery.compare_exchange_strong(recovery, 0, std::memory_order_relaxed)) {
                    recoveryDeadlineTicks.store(0, std::memory_order_relaxed);
                }
            } else if (activeTrackRecovery.compare_exchange_strong(recovery, 0, std::memory_order_relaxed)) {
                recoveryDeadlineTicks.store(0, std::memory_order_relaxed);
                const char priorLoading = __atomic_exchange_n(engineLoadingFlag, 0, __ATOMIC_SEQ_CST);
                reinterpret_cast<VoidFn>(originals[Stop])();
                Event timeout = begin(Stop, __builtin_return_address(0));
                timeout.phase = 'T'; timeout.arg0 = static_cast<int>(recovery); timeout.arg1 = priorLoading; enqueue(timeout);
                result = 0;
                if (manager.runtimeEnabled) {
                    AcquireSRWLockExclusive(&stateLock);
                    const Track* active = nullptr;
                    for (const Track& track : manager.catalog) {
                        if (track.id == manager.activeTrackId) { active = &track; break; }
                    }
                    if (active) setRuntimeTrackFailureLocked(*active);
                    manager.activeTrackPending = false;
                    manager.activeTrackPlaying = false;
                    manager.activeTrackId.clear();
                    ReleaseSRWLockExclusive(&stateLock);
                }
            }
        } else if (!result) {
            activeTrackRecovery.store(0, std::memory_order_relaxed);
            if (!shouldTimeout) recoveryDeadlineTicks.store(0, std::memory_order_relaxed);
            if (manager.runtimeEnabled) {
                AcquireSRWLockExclusive(&stateLock);
                if (manager.activeTrackPending) {
                    const Track* active = nullptr;
                    for (const Track& track : manager.catalog) {
                        if (track.id == manager.activeTrackId) { active = &track; break; }
                    }
                    if (active) setRuntimeTrackFailureLocked(*active);
                }
                manager.activeTrackPending = false;
                manager.activeTrackPlaying = false;
                manager.activeTrackId.clear();
                ReleaseSRWLockExclusive(&stateLock);
            }
        }
    }
    const bool loading = engineLoadingFlag && __atomic_load_n(engineLoadingFlag, __ATOMIC_SEQ_CST) != 0;
    if (manager.runtimeEnabled) {
        AcquireSRWLockExclusive(&stateLock);
        if (manager.deferredPlay) result = 1;
        if (manager.voicePaused && !manager.activeTrackId.empty()) result = 1;
        if (!manager.deferredPlay && (manager.activeTrackPending || manager.activeTrackPlaying) && (result != 0) && !loading) {
            if (manager.activeTrackPending) notifyTrackStartedLocked(GetTickCount64());
            manager.activeTrackPending = false;
            manager.activeTrackPlaying = true;
            recordPlayingHistoryLocked();
        } else if ((manager.activeTrackPending || manager.activeTrackPlaying) && result == 0) {
            if (manager.activeTrackPending) {
                const Track* active = nullptr;
                for (const Track& track : manager.catalog) {
                    if (track.id == manager.activeTrackId) { active = &track; break; }
                }
                if (active) setRuntimeTrackFailureLocked(*active);
            }
            manager.activeTrackPending = false;
            manager.activeTrackPlaying = false;
            manager.activeTrackId.clear();
            manager.notificationText.clear(); manager.notificationUntil = 0;
        }
        ReleaseSRWLockExclusive(&stateLock);
    }
    if (controlThread) result = processMusicControls(action, result, loading);
    if (lastPlaying.exchange(result, std::memory_order_relaxed) != result) {
        e.phase = 'E'; e.arg0 = result; enqueue(e);
    }
    SetLastError(statusError);
    return result;
}

void* detours[] = {reinterpret_cast<void*>(onInitialize), reinterpret_cast<void*>(onLoad),
    reinterpret_cast<void*>(onPlay), reinterpret_cast<void*>(onPause), reinterpret_cast<void*>(onResume),
    reinterpret_cast<void*>(onStop), reinterpret_cast<void*>(onVolume), reinterpret_cast<void*>(onPlaying)};

unsigned long long written = 0;
bool writeLine(const char* line) {
    const auto size = static_cast<DWORD>(strlen(line));
    if (written + size > 32ull * 1024 * 1024) { recording.store(false); return false; }
    DWORD n = 0;
    if (!WriteFile(output, line, size, &n, nullptr) || n != size) { recording.store(false); return false; }
    written += n; return true;
}
void escapePath(const char* src, char* dst, size_t cap) {
    size_t n = 0;
    for (size_t i = 0; src[i] && n + 4 < cap; ++i) {
        unsigned char c = static_cast<unsigned char>(src[i]);
        if (c < 32 || c >= 127 || c == '\\') {
            const char* hex = "0123456789abcdef";
            dst[n++] = '\\'; dst[n++] = 'x'; dst[n++] = hex[c >> 4]; dst[n++] = hex[c & 15];
        } else dst[n++] = static_cast<char>(c);
    }
    dst[n] = 0;
}
void drain() {
    for (;;) {
        Event e;
        AcquireSRWLockExclusive(&queueLock);
        bool present = tail != head;
        if (present) e = queue[tail++ % kCapacity];
        ReleaseSRWLockExclusive(&queueLock);
        if (!present) break;
        char path[2100], line[2600]; escapePath(e.path, path, sizeof(path));
        float volume; memcpy(&volume, &e.bits, sizeof(volume));
        snprintf(line, sizeof(line), "%llu\t%.3f\t%lu\t%s\t%c\t0x%llx\t%d\t%d\t0x%08x\t%.9g\t%s\n",
            e.call, static_cast<double>(e.ticks) * 1000 / frequency.QuadPart,
            static_cast<unsigned long>(e.thread), names[e.kind], e.phase,
            static_cast<unsigned long long>(e.caller), e.arg0, e.arg1, e.bits,
            e.kind == Volume ? static_cast<double>(volume) : 0.0, path);
        writeLine(line);
    }
}
void summary() {
    char line[700];
    snprintf(line, sizeof(line), "# counts Initialize=%llu LoadMusic=%llu Play=%llu Pause=%llu Resume=%llu Stop=%llu SetVolume=%llu IsPlayingMusic=%llu dropped=%llu\n",
        counts[0].load(), counts[1].load(), counts[2].load(), counts[3].load(), counts[4].load(),
        counts[5].load(), counts[6].load(), counts[7].load(), dropped.load());
    writeLine(line);
}
DWORD WINAPI writerMain(void*) {
    ULONGLONG last = GetTickCount64();
    while (WaitForSingleObject(stopEvent, 50) == WAIT_TIMEOUT) {
        drain();
        if (GetTickCount64() - last >= 10000) { summary(); last = GetTickCount64(); }
    }
    drain(); summary(); return 0;
}

bool checkOpenFileHash(HANDLE file, const char* expected) {
    if (file == INVALID_HANDLE_VALUE || !expected) return false;
    BCRYPT_ALG_HANDLE alg = nullptr; BCRYPT_HASH_HANDLE hash = nullptr;
    bool ok = BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) >= 0;
    if (ok) ok = BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0) >= 0;
    unsigned char buffer[32768], digest[32]; DWORD read = 0;
    while (ok) {
        if (!ReadFile(file, buffer, sizeof(buffer), &read, nullptr)) { ok = false; break; }
        if (!read) break;
        ok = BCryptHashData(hash, buffer, read, 0) >= 0;
    }
    if (ok) ok = BCryptFinishHash(hash, digest, sizeof(digest), 0) >= 0;
    char actual[65]{};
    if (ok) for (unsigned i = 0; i < 32; ++i) snprintf(actual + i * 2, 3, "%02x", digest[i]);
    if (hash) BCryptDestroyHash(hash);
    if (alg) BCryptCloseAlgorithmProvider(alg, 0);
    return ok && strcmp(actual, expected) == 0;
}
bool checkModule(HMODULE module, const char* expected) {
    wchar_t path[32768];
    DWORD n = module ? GetModuleFileNameW(module, path, 32768) : 0;
    if (!n || n >= 32768) return false;
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_DELETE,
        nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    const bool ok = checkOpenFileHash(file, expected);
    if (file != INVALID_HANDLE_VALUE) CloseHandle(file);
    return ok;
}
bool startLog() {
    wchar_t path[32768]; DWORD n = GetModuleFileNameW(selfModule, path, 32768);
    if (!n || n >= 32000) return false;
    wchar_t* slash = wcsrchr(path, L'\\'); if (!slash) return false;
    *slash = 0; wcscat_s(path, L"\\RedWolfRadio-logs");
    if (!CreateDirectoryW(path, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
    SYSTEMTIME utc; GetSystemTime(&utc);
    wchar_t suffix[100];
    swprintf_s(suffix, L"\\trace-%04u%02u%02u-%02u%02u%02u-%lu.tsv", utc.wYear, utc.wMonth,
        utc.wDay, utc.wHour, utc.wMinute, utc.wSecond, GetCurrentProcessId());
    wcscat_s(path, suffix);
    output = CreateFileW(path, GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (output == INVALID_HANDLE_VALUE) return false;
    QueryPerformanceCounter(&epoch); QueryPerformanceFrequency(&frequency);
    char header[1000];
    snprintf(header, sizeof(header), "# Red Wolf Radio %s; UTC=%04u-%02u-%02uT%02u:%02u:%02uZ; exeBase=0x%llx; engineBase=0x%llx\n# Runtime music routing log; Stop/T=failed-load timeout. H=music command (arg0 Next=1 Previous=2 PlayPause=3 Restart=4 PreviousTrack=5; arg1 applied=1 pending=2 ignored=0 failed=-1). Play/D=deferred while user-paused.\n# Volume/status repeats counted, only changes logged; 10-second counts; 32MiB cap; process-exit tail may be lost.\n# exeSHA256=%s\n# engineSHA256=%s\ncall\tms\tthread\tfunction\tphase\tcaller\targ0\targ1\tbits_or_path_flags\tvolume\tpath_bytes\n",
        kVersion, utc.wYear, utc.wMonth, utc.wDay, utc.wHour, utc.wMinute, utc.wSecond,
        reinterpret_cast<unsigned long long>(host->exeModule), reinterpret_cast<unsigned long long>(host->engineModule), kExeHash, kEngineHash);
    if (!writeLine(header)) return false;
    stopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (!stopEvent) return false;
    writer = CreateThread(nullptr, 0, writerMain, nullptr, 0, nullptr);
    if (!writer) return false;
    return true;
}
void closeLogBeforeHooks() {
    if (writer) { SetEvent(stopEvent); WaitForSingleObject(writer, INFINITE); CloseHandle(writer); writer = nullptr; }
    if (stopEvent) { CloseHandle(stopEvent); stopEvent = nullptr; }
    if (output != INVALID_HANDLE_VALUE) { CloseHandle(output); output = INVALID_HANDLE_VALUE; }
}
} // namespace

extern "C" __declspec(dllexport) unsigned TsmPluginApiVersion() { return 4; }
extern "C" __declspec(dllexport) int TsmPluginInit(const TsmHost* api, TsmPluginInfo* info) {
    if (!api || !info || api->apiVersion != 4 || api->structSize < sizeof(TsmHost) ||
        !api->log || !api->findIatSlot || !api->patchIat || !api->installInlineHook || !api->allocNear) return 1;
    host = api; info->name = "Red Wolf Radio"; info->version = kVersion;
    if (!checkModule(static_cast<HMODULE>(api->exeModule), kExeHash) ||
        !checkModule(static_cast<HMODULE>(api->engineModule), kEngineHash) ||
        !checkModule(GetModuleHandleW(L"rml_runtime.dll"), kRuntimeHash)) {
        api->log("[RedWolfRadio] Declined: game, engine, or Republic runtime differs from verified build. No hooks installed.");
        return 1;
    }
    engineLoadingFlag = reinterpret_cast<volatile char*>(
        reinterpret_cast<unsigned char*>(api->engineModule) + 0x1dc6e1);
    // Never bypass an existing import owner.
    for (unsigned i = 0; i < 8; ++i) {
        void** slot = api->findIatSlot(api->exeModule, "C3DDLL64.dll", imports[i]);
        void* target = reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(api->engineModule), imports[i]));
        if (!slot || !target || *slot != target) {
            api->log("[RedWolfRadio] Declined: missing/changed import %s. No hooks installed.", names[i]); return 1;
        }
    }
    api->log("[RedWolfRadio] Build hashes and eight imports validated; waiting for Start.");
    return 0;
}
extern "C" __declspec(dllexport) int TsmPluginStart() {
    if (!host) return 1;
    if (!startLog()) { closeLogBeforeHooks(); host->log("[RedWolfRadio] Cannot create runtime log/worker; no hooks installed."); return 1; }
    const bool catalogOk = initializeCatalogFromConfig();
    size_t nativeTrackCount = 0;
    for (const Track& track : manager.catalog) if (track.nativePlayable) ++nativeTrackCount;
    if (!catalogOk) {
        host->log("[RedWolfRadio] Catalog/config parse failed; music replacement remains disabled.");
    }
    // Pin before publishing callbacks; API has no unload lifecycle. Workers and
    // installed detours must remain valid even if a later hook request fails.
    HMODULE pinned = nullptr;
    if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_PIN,
        reinterpret_cast<LPCWSTR>(&TsmPluginStart), &pinned)) {
        closeLogBeforeHooks(); return 1;
    }
    unsigned installed = 0;
    auto* pathTarget = reinterpret_cast<unsigned char*>(host->engineModule) + 0x776d0;
    unsigned char* relocatedPathContinuation = nullptr;
    if (memcmp(pathTarget, expectedPathPrologue, kPathStolenBytes) == 0) {
        relocatedPathContinuation = host->allocNear(pathTarget, 64);
    }
    bool privateContinuationReady = relocatedPathContinuation &&
        buildRelocatedPathContinuation(relocatedPathContinuation, 64, expectedPathPrologue, pathTarget) &&
        FlushInstructionCache(GetCurrentProcess(), relocatedPathContinuation, 64);
    void* ignoredUnsafeHostContinuation = nullptr;
    originalPathResolver = privateContinuationReady ? reinterpret_cast<PathFn>(relocatedPathContinuation) : nullptr;
    if (privateContinuationReady && host->installInlineHook(pathTarget, reinterpret_cast<void*>(onResolvePath),
        &ignoredUnsafeHostContinuation, expectedPathPrologue, kPathStolenBytes,
        "Red Wolf Radio scoped cached-audio path resolver with relocated continuation")) {
        ++installed;
    } else {
        originalPathResolver = nullptr;
        host->log("[RedWolfRadio] Safe relocated path continuation or resolver hook failed; music replacement remains disabled.");
    }
    for (unsigned i = 0; i < 8; ++i) {
        if (host->patchIat(host->exeModule, "C3DDLL64.dll", imports[i], detours[i], &originals[i], names[i])) ++installed;
        else { host->log("[RedWolfRadio] Hook failed: %s; installed wrappers remain pass-through.", names[i]); break; }
    }
    if (installed == 9) {
        redirectReady.store(true, std::memory_order_relaxed);
        manager.runtimeEnabled = catalogOk && !manager.catalog.empty() &&
            (nativeTrackCount > 0 || manager.config.originalsMode == OriginalsMode::None);
        if (manager.config.nativeSongPopup && !installNativePlayer())
            host->log("[RedWolfRadio] Optional native song popup unavailable; Script entry retains its fallback message.");
        if (manager.runtimeEnabled) {
            if (manager.config.showTrackName && !installNotificationRenderer())
                host->log("[RedWolfRadio] Optional track HUD unavailable; audio remains active.");
            if (!startPrefetchWorker()) {
                host->log("[RedWolfRadio] Background prefetch worker unavailable; startup-prepared tracks remain usable.");
            } else {
                requestPrefetch();
            }
            recording.store(true);
            host->log("[RedWolfRadio] ACTIVE: 8 music IAT hooks plus scoped path resolver; bounded-cache playback enabled.");
        } else {
            recording.store(true);
            host->log("[RedWolfRadio] ACTIVE: 8 music IAT hooks plus scoped path resolver; playback disabled (no viable tracks).");
        }
    } else {
        recording.store(true);
        host->log("[RedWolfRadio] INCOMPLETE: %u/9 hooks; music replacement remains disabled.", installed);
    }
    // Once a hook is published, stay loaded even when observation is disabled.
    return 0;
}
BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) { selfModule = instance; DisableThreadLibraryCalls(instance); }
    return TRUE;
}







