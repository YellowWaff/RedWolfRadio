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

namespace {
constexpr char kVersion[] = "0.9.0-dev-bounded-cache";
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
    OriginalsMode originalsMode = OriginalsMode::All;
    std::vector<std::string> excludedIds;
};
struct Track {
    std::string id;
    TrackSource source = TrackSource::External;
    std::string relativeUtf8;
    std::wstring absoluteWide;
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
    bool hooksInstalled = false;
    ULONGLONG silenceUntil = 0;
    bool silenceTrackReady = false;
    std::string silenceTrackPath;
    std::wstring cacheDirectory;
    size_t bagCursor = 0;
    std::string lastPlayedId;
    unsigned converted = 0;
    unsigned cacheHits = 0;
    unsigned rejected = 0;
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
    WCHAR modulePath[MAX_PATH * 4]{};
    const DWORD length = GetModuleFileNameW(selfModule, modulePath, static_cast<DWORD>(std::size(modulePath)));
    if (!length || length >= std::size(modulePath)) return false;
    WCHAR* slash = wcsrchr(modulePath, L'\\');
    if (!slash) return false;
    slash[1] = 0;
    path.assign(modulePath);
    for (const char* c = kIniName; *c; ++c) path.push_back(static_cast<wchar_t>(*c));
    return true;
}

bool createDefaultIniIfMissing() {
    std::wstring iniPath;
    if (!getPluginIniPath(iniPath)) return false;
    HANDLE file = CreateFileW(iniPath.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return GetLastError() == ERROR_FILE_EXISTS;
    std::string text =
        "; Red Wolf Radio settings. Republic Mod Loader opens this file from the plugin page.\r\n"
        "; Changes take effect on the next game launch.\r\n\r\n"
        "[RedWolfRadio]\r\n"
        "SchemaVersion=1\r\n\r\n"
        "[Sources]\r\n"
        "ExternalEnabled=true\r\n"
        "; Environment variables such as %USERPROFILE% are expanded.\r\n"
        "; Leave blank to use this Windows account's configured Music known folder.\r\n"
        "ExternalPath=%USERPROFILE%\\Music\r\n"
        "ExternalRecursive=true\r\n"
        "; The optional local folder is Music beside RedWolfRadio.dll.\r\n"
        "PluginLocalEnabled=false\r\n"
        "PluginLocalRecursive=true\r\n\r\n"
        "[Cache]\r\n"
        "; Maximum derived-audio cache size in MiB. Zero means unlimited.\r\n"
        "MaxSizeMiB=4096\r\n"
        "; Number of upcoming playlist entries kept ready.\r\n"
        "PrefetchTracks=5\r\n\r\n"
        "[Originals]\r\n"
        "; Mode may be all, selected, or none.\r\n"
        "Mode=all\r\n"
        "; With Mode=selected, list stable original: IDs separated by commas.\r\n"
        "ExcludedIds=\r\n";
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
        if (source == TrackSource::Original) {
            std::string game = "music/";
            game += normalizeIdText(track.relativeUtf8);
            track.relativeUtf8 = game;
        }
        track.nativePlayable = source == TrackSource::Original &&
            (format == TrackFormat::Wav || format == TrackFormat::Xwma);
        if (track.nativePlayable) track.preparation = PreparationState::Ready;
        else if (source != TrackSource::Original && format != TrackFormat::Xwma)
            track.preparation = PreparationState::Pending;
        else track.preparation = PreparationState::Rejected;
        track.id = makeTrackId(source, track.relativeUtf8);
        if (usedIds.insert(track.id).second) tracks.push_back(std::move(track));
    } while (FindNextFileW(it, &data));
    FindClose(it);
}

void rebuildPlayBag();

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
    search += L"*.wav";
    WIN32_FIND_DATAW data{};
    HANDLE found = FindFirstFileW(search.c_str(), &data);
    if (found == INVALID_HANDLE_VALUE) return files;
    do {
        if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
            _wcsicmp(data.cFileName, L"red_wolf_radio_silent.wav") == 0) continue;
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
    ReleaseSRWLockExclusive(&stateLock);

    pruneCacheToConfiguredLimit();
    CanonicalAudioResult result;
    std::string error;
    bool ok = !manager.cacheDirectory.empty() &&
        convertToCanonicalCache(source, manager.cacheDirectory, result, error);
    std::string routed;
    if (ok && (!toUtf8(result.path, routed) || routed.empty() || routed.size() >= 256)) {
        ok = false;
        error = "converted cache path exceeds the verified engine limit";
    }

    AcquireSRWLockExclusive(&stateLock);
    if (index < manager.catalog.size() && manager.catalog[index].id == id) {
        Track& track = manager.catalog[index];
        if (ok) {
            track.preparedWide = result.path;
            track.preparedUtf8 = routed;
            track.nativePlayable = true;
            track.preparation = PreparationState::Ready;
            if (result.cacheHit) ++manager.cacheHits; else ++manager.converted;
        } else {
            track.nativePlayable = false;
            track.preparation = PreparationState::Rejected;
            ++manager.rejected;
            manager.trackSelectionDirty = true;
        }
    }
    ReleaseSRWLockExclusive(&stateLock);
    if (ok) {
        touchCacheFile(result.path);
        pruneCacheToConfiguredLimit();
    } else if (host && host->log) {
        host->log("[RedWolfRadio] Conversion failed; omitted %s: %s", id.c_str(), error.c_str());
    }
    return ok;
}

void rebuildPlayBag() {
    manager.playBag.clear();
    for (size_t i = 0; i < manager.catalog.size(); ++i) {
        const Track& track = manager.catalog[i];
        if (track.preparation == PreparationState::Rejected) continue;
        if (manager.bannedTrackIds.count(track.id)) continue;
        if (track.source == TrackSource::Original) {
            if (manager.config.originalsMode == OriginalsMode::None) continue;
            if (manager.config.originalsMode == OriginalsMode::Selected &&
                manager.excludedOriginalIds.find(track.id) != manager.excludedOriginalIds.end()) continue;
        }
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

const Track* pickTrackLocked() {
    if (manager.catalog.empty()) return nullptr;
    if (manager.trackSelectionDirty) rebuildPlayBag();
    if (manager.playBag.empty()) return nullptr;
    if (manager.bagCursor >= manager.playBag.size()) {
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
    size_t readyPosition = manager.bagCursor;
    while (readyPosition < manager.playBag.size() &&
           !manager.catalog[manager.playBag[readyPosition]].nativePlayable) ++readyPosition;
    if (readyPosition >= manager.playBag.size()) return nullptr;
    if (readyPosition != manager.bagCursor)
        std::swap(manager.playBag[readyPosition], manager.playBag[manager.bagCursor]);
    const Track& track = manager.catalog[manager.playBag[manager.bagCursor++]];
    manager.activeTrackId = track.id;
    manager.activeTrackSource = track.source;
    manager.activeTrackPending = true;
    manager.activeTrackPlaying = false;
    manager.activeLoadStart = 0;
    return &track;
}

size_t nextPrefetchCandidateLocked() {
    if (manager.trackSelectionDirty) rebuildPlayBag();
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
    std::wstring baseRoot;

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
        if (host && host->pluginDir && *host->pluginDir && toWide(host->pluginDir, pluginLocal)) {
            if (normalizeFullPath(pluginLocal, pluginLocal)) {
                std::wstring candidate = pluginLocal;
                if (!candidate.empty() && !isSeparator(candidate.back())) candidate.push_back(L'\\');
                candidate += L"Music";
                roots.emplace_back(TrackSource::PluginLocal, candidate);
            }
        }
    }
    if (host && host->baseDir && *host->baseDir && toWide(host->baseDir, baseRoot)) {
        if (normalizeFullPath(baseRoot, baseRoot)) {
            std::wstring candidate = baseRoot;
            if (!candidate.empty() && !isSeparator(candidate.back())) candidate.push_back(L'\\');
            candidate += L"media_soviet\\Music";
            roots.emplace_back(TrackSource::Original, candidate);
        }
    }

    manager.catalog.clear();
    for (auto& entry : roots) {
        if (entry.second.empty()) continue;
        collectTrackRecursively(entry.second, L"", entry.first,
            entry.first == TrackSource::Original ? true : (entry.first == TrackSource::External ? manager.config.externalRecursive : manager.config.pluginLocalRecursive),
            manager.catalog, usedIds);
    }
    manager.excludedOriginalIds.clear();
    for (const std::string& id : manager.config.excludedIds) manager.excludedOriginalIds.insert(id);
    manager.converted = 0;
    manager.cacheHits = 0;
    manager.rejected = 0;
    for (const Track& track : manager.catalog) {
        if (track.source != TrackSource::Original && track.preparation == PreparationState::Rejected) {
            ++manager.rejected;
            if (host && host->log) host->log("[RedWolfRadio] External XWMA remains disabled pending compatibility validation: %s", track.id.c_str());
        }
    }
    manager.trackSelectionDirty = true;
    manager.bagCursor = 0;
    manager.activeTrackId.clear();
    manager.lastPlayedId.clear();
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
    host->log("[RedWolfRadio] Cache: MaxSizeMiB=%llu, PrefetchTracks=%u; startup prepared %u converted, %u cache hits, %u rejected.",
        static_cast<unsigned long long>(manager.config.cacheMaxSizeMiB), manager.config.prefetchTracks,
        manager.converted, manager.cacheHits, manager.rejected);
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
    Event e = begin(Play, __builtin_return_address(0)); e.arg0 = arg0; e.arg1 = arg1; enqueue(e);
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
int onPlaying() {
    Event e = begin(Playing, __builtin_return_address(0));
    int result = reinterpret_cast<PlayingFn>(originals[Playing])();
    const DWORD statusError = GetLastError();
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
    if (manager.runtimeEnabled) {
        AcquireSRWLockExclusive(&stateLock);
        if ((manager.activeTrackPending || manager.activeTrackPlaying) && (result != 0)) {
            manager.activeTrackPending = false;
            manager.activeTrackPlaying = true;
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
        }
        ReleaseSRWLockExclusive(&stateLock);
    }
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
    snprintf(header, sizeof(header), "# Red Wolf Radio %s; UTC=%04u-%02u-%02uT%02u:%02u:%02uZ; exeBase=0x%llx; engineBase=0x%llx\n# Runtime music routing log; Stop/T records the six-second failed-load recovery timeout.\n# Volume/status repeats counted, only changes logged; 10-second counts; 32MiB cap; process-exit tail may be lost.\n# exeSHA256=%s\n# engineSHA256=%s\ncall\tms\tthread\tfunction\tphase\tcaller\targ0\targ1\tbits_or_path_flags\tvolume\tpath_bytes\n",
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
        if (manager.runtimeEnabled) {
            rebuildPlayBag();
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







