// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <cstring>

namespace rwr {

// Select an address set only for a verified EXE/engine pair. Engine music RVAs,
// native UI globals and object layouts are shared by these two inspected builds.
// The relocated EXE functions and Script caller are deliberately build-specific.
struct GameBuild {
    const char* version;
    const char* exeHash;
    const char* engineHash;
    size_t nativeDialogRva;
    size_t scriptDialogReturnRva;
    unsigned char scriptDialogCall[5];
    size_t nativePlayerFrameRva;
    size_t nativePlayerBodyRva;
    size_t nativePlayerHitRva;
    size_t hudFrameRva;
};

inline constexpr GameBuild kVerifiedGameBuilds[] = {
    {"1.1.1.9",
     "296644a9f207d609031fc2ae73fed2dcb34619a1d55a35d1c7b51965ce6841b8",
     "65fcb6845c3cb7c25a22121f25904a855ea88657e31a3b50a15adac26e231516",
     0x6ee2a0, 0x5a0454, {0xe8, 0x4c, 0xde, 0x14, 0x00},
     0x6f0070, 0x7f2660, 0x6ef830, 0x2f0f10},
    {"1.1.1.13",
     "4df5fbecc75300e3b843ae4e6426761c84ae2e989321c011c6f560be79122125",
     "5d8a0540a01170475392b4650f492582fab4ed20eec8755190a053f410247e3a",
     0x6ee440, 0x5a03e4, {0xe8, 0x5c, 0xe0, 0x14, 0x00},
     0x6f0210, 0x7f2800, 0x6ef9d0, 0x2f0e90}
};

inline const GameBuild* findVerifiedGameBuild(const char* exeHash, const char* engineHash) {
    if (!exeHash || !engineHash) return nullptr;
    for (const auto& build : kVerifiedGameBuilds) {
        if (!std::strcmp(exeHash, build.exeHash) && !std::strcmp(engineHash, build.engineHash))
            return &build;
    }
    return nullptr;
}

} // namespace rwr
