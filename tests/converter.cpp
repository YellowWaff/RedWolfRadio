// SPDX-License-Identifier: GPL-3.0-only
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include "../src/audio_converter.h"

static void require(bool condition, const char* message) {
    if (!condition) { fprintf(stderr, "FAIL: %s\n", message); exit(1); }
}

int wmain(int argc, wchar_t** argv) {
    require(argc == 4, "expected WAV, FLAC, and MP3 fixture paths");
    wchar_t temp[MAX_PATH * 4]{};
    require(GetTempPathW(static_cast<DWORD>(std::size(temp)), temp) > 0, "resolve temp path");
    std::wstring cache = temp;
    cache += L"RedWolfRadio-converter-test-" + std::to_wstring(GetCurrentProcessId());
    require(CreateDirectoryW(cache.c_str(), nullptr) || GetLastError() == ERROR_ALREADY_EXISTS, "create test cache");

    for (int i = 1; i < argc; ++i) {
        CanonicalAudioResult first;
        std::string error;
        require(convertToCanonicalCache(argv[i], cache, first, error), error.c_str());
        require(validateCanonicalWav(first.path, nullptr), "output is canonical PCM16 stereo 48 kHz WAV");
        require(first.frames > 0, "decoded frames are nonzero");
        CanonicalAudioResult second;
        require(convertToCanonicalCache(argv[i], cache, second, error), "repeat conversion succeeds");
        require(second.cacheHit && second.path == first.path && second.frames == first.frames,
            "content/profile key gives a deterministic validated cache hit");
    }

    const std::wstring unicodeSource = cache + L"\\input-\u97f3\u697d.mp3";
    require(CopyFileW(argv[3], unicodeSource.c_str(), FALSE), "create Unicode-path input");
    CanonicalAudioResult unicodeResult;
    std::string error;
    require(convertToCanonicalCache(unicodeSource, cache, unicodeResult, error), "decode Unicode-path MP3");
    require(validateCanonicalWav(unicodeResult.path, nullptr), "Unicode input output validates");

    const std::wstring damaged = cache + L"\\damaged.mp3";
    HANDLE file = CreateFileW(damaged.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    require(file != INVALID_HANDLE_VALUE, "create damaged input");
    const char junk[] = "not audio";
    DWORD written = 0;
    require(WriteFile(file, junk, sizeof(junk), &written, nullptr) && written == sizeof(junk), "write damaged input");
    CloseHandle(file);
    CanonicalAudioResult damagedResult;
    require(!convertToCanonicalCache(damaged, cache, damagedResult, error), "damaged input is rejected");

    WIN32_FIND_DATAW data{};
    const std::wstring search = cache + L"\\*";
    HANDLE find = FindFirstFileW(search.c_str(), &data);
    if (find != INVALID_HANDLE_VALUE) {
        do {
            if (!(data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) DeleteFileW((cache + L"\\" + data.cFileName).c_str());
        } while (FindNextFileW(find, &data));
        FindClose(find);
    }
    RemoveDirectoryW(cache.c_str());
    puts("PASS: WAV, FLAC, MP3, Unicode-path decoding, canonical output, deterministic cache hits, and damaged-input rejection.");
    return 0;
}
