// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <stdint.h>
#include <string>

struct CanonicalAudioResult {
    std::wstring path;
    uint64_t frames = 0;
    bool cacheHit = false;
};

bool validateCanonicalWav(const std::wstring& path, uint64_t* frames = nullptr);
bool convertToCanonicalCache(const std::wstring& source,
                             const std::wstring& cacheDirectory,
                             CanonicalAudioResult& result,
                             std::string& error);

struct XwmaInfo {
    uint16_t channels = 0;
    uint32_t sampleRate = 0;
    uint32_t packetCount = 0;
    uint32_t decodedBytes = 0;
};
struct NativeAudioResult {
    std::wstring path;
    bool copied = false;
    bool cacheHit = false;
};
bool validateXwma(const std::wstring& path, XwmaInfo* info = nullptr);
bool enginePathSafe(const std::wstring& path);
bool resolveEnginePath(const std::wstring& path, std::wstring& routed);
bool prepareNativeXwma(const std::wstring& source, const std::wstring& cacheDirectory,
                       NativeAudioResult& result, std::string& error);
