// SPDX-License-Identifier: GPL-3.0-only
// Run the real catalog code with the executable, loader, and plugin DLL in
// separate folders. No game binaries are loaded and no hooks are installed.
#include "../src/music_trace.cpp"
#include <filesystem>
#include <stdarg.h>

static std::vector<std::string> capturedLogs;
static void require(bool condition, const char* message) {
    if (!condition) { fprintf(stderr, "FAIL: %s (Windows error %lu)\n", message, GetLastError()); exit(1); }
}
static void captureLog(const char* format, ...) {
    char buffer[4096]{};
    va_list args; va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    capturedLogs.emplace_back(buffer);
}
static bool logged(const std::string& text) {
    for (const auto& line : capturedLogs) if (line.find(text) != std::string::npos) return true;
    return false;
}
static const Track& track(const char* id) {
    for (const auto& item : manager.catalog) if (item.id == id) return item;
    fprintf(stderr, "Missing catalog ID: %s\n", id);
    exit(1);
}
static bool contains(const char* id) {
    for (const auto& item : manager.catalog) if (item.id == id) return true;
    return false;
}
static void writeFixture(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_NEW, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE, "create discovery-only fixture");
    const char bytes[] = "unchanged original / ignored non-audio discovery fixture";
    DWORD written = 0;
    require(WriteFile(file, bytes, sizeof(bytes), &written, nullptr) && written == sizeof(bytes), "write discovery fixture");
    CloseHandle(file);
}
struct Snapshot { std::vector<unsigned char> bytes; FILETIME modified; };
static Snapshot snapshot(const std::wstring& path) {
    HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE, "open fixture snapshot");
    Snapshot result{};
    require(GetFileTime(file, nullptr, nullptr, &result.modified), "read original timestamp");
    const DWORD size = GetFileSize(file, nullptr);
    require(size != INVALID_FILE_SIZE, "size original snapshot");
    result.bytes.resize(size);
    DWORD read = 0;
    require(ReadFile(file, result.bytes.data(), size, &read, nullptr) && read == size, "snapshot original bytes");
    CloseHandle(file);
    return result;
}
static int isolatedTest(const std::wstring& root) {
    const std::wstring dllPath = root + L"\\Package\\plugin\\RedWolfRadio.dll";
    selfModule = LoadLibraryW(dllPath.c_str());
    require(selfModule != nullptr, "load actual plugin DLL from separate package directory");
    TsmHost testHost{};
    testHost.exeModule = GetModuleHandleW(nullptr);
    std::string loader, shared;
    require(toUtf8(root + L"\\Loader\\rml", loader) && toUtf8(root + L"\\Loader\\rml\\plugins", shared), "encode misleading loader paths");
    testHost.baseDir = loader.c_str(); testHost.pluginDir = shared.c_str(); testHost.log = captureLog;
    host = &testHost;
    std::wstring resolved;
    require(getModuleRelativePath(static_cast<HMODULE>(testHost.exeModule), L"media_soviet\\Music", resolved) &&
        resolved == root + L"\\Game\\media_soviet\\Music", "real executable handle determines original root");
    require(getPluginIniPath(resolved) && resolved == root + L"\\Package\\plugin\\RedWolfRadio.ini", "INI shares actual DLL directory");
    require(!getModuleRelativePath(nullptr, L"Music", resolved) && resolved.empty(), "missing handle cannot fall back to current executable");
    manager = ManagerState{};
    manager.config.externalUseDefault = false;
    manager.config.externalPath = root + L"\\Library";
    manager.config.pluginLocalEnabled = true;
    manager.config.prefetchTracks = 256;
    manager.cacheDirectory = root + L"\\Cache";
    require(ensureDirectory(manager.cacheDirectory), "create isolated cache");
    manager.config.excludedIds = {"original:music/future pack/unlisted theme.xwma"};
    rebuildCatalog();
    logCatalogStats();
    require(manager.catalog.size() == 7, "discover complete WAV/FLAC/MP3/XWMA catalog and ignore artwork");
    require(track("original:music/arbitrary opening.wav").absoluteWide == root + L"\\Game\\media_soviet\\Music\\Arbitrary Opening.WAV", "original absolute path remains game-relative");
    const Track& nested = track("original:music/future pack/unlisted theme.xwma");
    require(nested.relativeUtf8 == "music/future pack/unlisted theme.xwma" && nested.nativePlayable, "native original XWMA retains engine-relative routing");
    require(trackEligibleLocked(nested), "all mode includes an ID even when it is listed in exclusions");
    require(track("plugin-local:own song.wav").nativePlayable && track("plugin-local:album/another song.flac").nativePlayable &&
        track("external:unspecified name.mp3").nativePlayable && track("external:more/nested.wav").nativePlayable, "separate plugin-local and external formats prepare successfully");
    require(!trackEligibleLocked(track("original:music/unsupported.flac")), "unsupported original format remains cataloged but ineligible");
    require(!contains("original:music/loader decoy.xwma") && !contains("plugin-local:shared decoy.wav"), "shared loader directories cannot contaminate catalog");
    require(logged("Source original:") && logged("Source plugin-local:") && logged("Source external:"), "log every resolved enabled source");
    require(logged("Original ID: original:music/arbitrary opening.wav;") &&
        logged("Original ID: original:music/future pack/unlisted theme.xwma;") &&
        logged("Original ID: original:music/unsupported.flac; eligible=no;"), "log complete stable original ID inventory including rejected entries");

    capturedLogs.clear();
    manager.config.originalsMode = OriginalsMode::Selected;
    rebuildCatalog(); logCatalogStats();
    require(!trackEligibleLocked(track("original:music/future pack/unlisted theme.xwma")) &&
        trackEligibleLocked(track("original:music/arbitrary opening.wav")), "selected mode excludes exactly the configured discovered original");
    require(logged("eligible=no; listedInExcludedIds=yes; selected exclusion"), "log selected exclusion eligibility");
    manager.config.originalsMode = OriginalsMode::None;
    rebuildCatalog();
    for (const auto& item : manager.catalog) if (item.source == TrackSource::Original)
        require(!trackEligibleLocked(item), "none mode excludes every discovered original");
    require(trackEligibleLocked(track("plugin-local:own song.wav")), "disabling originals leaves custom music eligible");

    manager.config.externalRecursive = false;
    manager.config.pluginLocalRecursive = false;
    rebuildCatalog();
    require(manager.catalog.size() == 5 && !contains("plugin-local:album/another song.flac") &&
        !contains("external:more/nested.wav") && contains("original:music/future pack/unlisted theme.xwma"), "source-specific recursion settings preserve recursive original discovery");
    manager.config.pluginLocalEnabled = false;
    manager.config.externalEnabled = false;
    rebuildCatalog();
    require(manager.catalog.size() == 3, "disabled custom sources are not scanned");
    writeFixture(root + L"\\Game\\media_soviet\\Music\\Added After Scan.xwma");
    rebuildCatalog();
    require(contains("original:music/added after scan.xwma") && manager.catalog.size() == 4, "new file is discovered on the next catalog build without fixed filenames or count");
    require(DeleteFileW((root + L"\\Game\\media_soviet\\Music\\Added After Scan.xwma").c_str()), "remove known test-created extra original");
    FreeLibrary(selfModule); selfModule = nullptr;
    puts("PASS: real executable/DLL directory discovery, separate loader decoys, recursive arbitrary filenames, ignored artwork, all/selected/none original modes, stable ID logging, and launch rescan.");
    return 0;
}

int wmain(int argc, wchar_t** argv) {
    if (argc == 2) return isolatedTest(argv[1]);
    require(argc == 1, "expected no test arguments");
    wchar_t temp[32768]{};
    const DWORD length = GetTempPathW(static_cast<DWORD>(std::size(temp)), temp);
    require(length && length < std::size(temp), "resolve fixture temp root");
    const std::wstring root = std::wstring(temp) + L"RedWolfRadio-source-paths-" + std::to_wstring(GetCurrentProcessId()) + L"-" + std::to_wstring(GetTickCount64());
    require(CreateDirectoryW(root.c_str(), nullptr), "create unique isolated fixture root");
    for (const wchar_t* dir : {L"Game\\media_soviet\\Music\\Future Pack", L"Package\\plugin\\Music\\Album", L"Loader\\rml\\media_soviet\\Music", L"Loader\\rml\\plugins\\Music", L"Library\\More"})
        require(ensureDirectory(root + L"\\" + dir), "create isolated fixture directory");
    std::wstring source;
    auto copy = [&](const wchar_t* name, const wchar_t* destination) {
        require(getModuleRelativePath(GetModuleHandleW(nullptr), name, source) && CopyFileW(source.c_str(), (root + L"\\" + destination).c_str(), TRUE), "copy test binary or audio fixture");
    };
    copy(L"source_paths-tests.exe", L"Game\\catalog-test.exe");
    copy(L"RedWolfRadio.dll", L"Package\\plugin\\RedWolfRadio.dll");
    copy(L"MusicTrace-probe.wav", L"Game\\media_soviet\\Music\\Arbitrary Opening.WAV");
    copy(L"MusicTrace-probe.wav", L"Package\\plugin\\Music\\Own Song.wav");
    copy(L"MusicTrace-descending.flac", L"Package\\plugin\\Music\\Album\\Another Song.flac");
    copy(L"MusicTrace-alternating.mp3", L"Library\\Unspecified Name.mp3");
    copy(L"MusicTrace-probe.wav", L"Library\\More\\Nested.wav");
    for (const wchar_t* file : {L"Game\\media_soviet\\Music\\Future Pack\\Unlisted Theme.xwma", L"Game\\media_soviet\\Music\\Unsupported.flac", L"Game\\media_soviet\\Music\\cover.png", L"Loader\\rml\\media_soviet\\Music\\Loader Decoy.xwma", L"Loader\\rml\\plugins\\Music\\Shared Decoy.wav", L"Package\\plugin\\Music\\cover.jpg", L"Library\\album.winmd"})
        writeFixture(root + L"\\" + file);
    std::vector<std::pair<std::wstring, Snapshot>> originalsBefore;
    for (const auto& item : std::filesystem::recursive_directory_iterator(root + L"\\Game\\media_soviet\\Music")) {
        if (!item.is_regular_file()) continue;
        originalsBefore.emplace_back(item.path().wstring(), snapshot(item.path().wstring()));
        require(SetFileAttributesW(item.path().c_str(), FILE_ATTRIBUTE_READONLY), "protect original fixture against writes");
    }
    const std::wstring application = root + L"\\Game\\catalog-test.exe";
    std::wstring command = L"\"" + application + L"\" \"" + root + L"\"";
    STARTUPINFOW startup{}; startup.cb = sizeof(startup);
    PROCESS_INFORMATION process{};
    require(CreateProcessW(application.c_str(), command.data(), nullptr, nullptr, FALSE, 0, nullptr,
        (root + L"\\Loader\\rml").c_str(), &startup, &process), "start isolated test from misleading loader working directory");
    require(WaitForSingleObject(process.hProcess, 60000) == WAIT_OBJECT_0, "isolated catalog test completes");
    DWORD status = 1;
    require(GetExitCodeProcess(process.hProcess, &status), "read isolated test exit status");
    CloseHandle(process.hThread); CloseHandle(process.hProcess);
    require(status == 0, "isolated catalog test passed");
    for (const auto& entry : originalsBefore) {
        const Snapshot after = snapshot(entry.first);
        require(after.bytes == entry.second.bytes && CompareFileTime(&after.modified, &entry.second.modified) == 0, "original file content and modification time preserved");
        require(SetFileAttributesW(entry.first.c_str(), FILE_ATTRIBUTE_NORMAL), "restore fixture attributes for cleanup");
    }
    // Delete only this freshly created task-specific directory after validating
    // its resolved absolute location beneath the designated temporary root.
    const auto resolved = std::filesystem::weakly_canonical(root);
    const auto tempResolved = std::filesystem::weakly_canonical(temp);
    require(resolved.parent_path() == tempResolved && resolved.filename().wstring().find(L"RedWolfRadio-source-paths-") == 0,
        "cleanup stays within the unique task fixture directory");
    std::filesystem::remove_all(resolved);
    puts("PASS: all original fixture bytes and timestamps unchanged; isolated files cleaned up.");
    return 0;
}
