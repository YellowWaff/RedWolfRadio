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
