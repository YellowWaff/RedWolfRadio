// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <cstddef>
#include <cstring>

namespace rwr {

// Select code, engine state and native UI layout only for an inspected pair.
// Engine exports are resolved by name; internal globals and detour prefixes
// remain tied to the exact executable/engine hashes below.
struct EngineLayout {
    size_t pathResolverRva;
    unsigned char pathResolverPrefix[18];
    size_t loadingFlagRva;
    size_t sourceVoiceRva;
};

inline constexpr EngineLayout kLegacyEngineLayout = {
    0x776d0,
    {0x40,0x55,0x41,0x56,0x48,0x81,0xec,0x18,0x02,0x00,0x00,0x48,0x8b,0x05,0xb6,0x64,0x15,0x00},
    0x1dc6e1, 0x1dc7f0
};

inline constexpr EngineLayout kSpaceProgramEngineLayout = {
    0x777b0,
    {0x40,0x55,0x41,0x56,0x48,0x81,0xec,0x18,0x02,0x00,0x00,0x48,0x8b,0x05,0xd6,0x73,0x15,0x00},
    0x1dd6e1, 0x1dd7f0
};

struct NativeUiLayout {
    size_t scaleRva, fontRva, fontManagerRva, widthRva, heightRva;
    size_t panelRva, inputRva, shaderRva;
    size_t windowSize, windowTypeOffset, windowStyleOffset;
    size_t windowWidthOffset, windowHeightOffset, windowBottomOffset, windowContentBottomOffset;
    size_t gameWindowsBeginOffset, gameWindowsEndOffset, gameHoveredWindowOffset;
    size_t gameGuidedOverlayOffset, gameBlockInputOffset, gameSelectionOffset;
    size_t gameHoverFlagOffset, gameCloseDelayOffset, gameWhiteTextureOffset;
};

inline constexpr NativeUiLayout kLegacyNativeUiLayout = {
    0x992088, 0x994200, 0x996fb0, 0x99528c, 0x995274,
    0x9be060, 0xa54b90, 0x9ead08,
    0x5d8, 0x248, 0x24c, 0x520, 0x51c, 0x250, 0x254,
    0xcac0, 0xcac8, 0xcb80, 0x13bf0, 0xfc1, 0xad74, 0xfc3, 0xfc8, 0xb048
};

inline constexpr NativeUiLayout kSpaceProgramNativeUiLayout = {
    0x9dc08c, 0x9df328, 0x9e0fc0, 0x9e0374, 0x9e0370,
    0xa08070, 0xa9ec50, 0xa34dc8,
    0x6d8, 0x248, 0x24c, 0x620, 0x61c, 0x250, 0x254,
    0xcb60, 0xcb68, 0xcc20, 0x13ca8, 0xff1, 0xada4, 0xff3, 0xff8, 0xb078
};

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
    const EngineLayout* engine = &kLegacyEngineLayout;
    const NativeUiLayout* ui = &kLegacyNativeUiLayout;
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
     0x6f0210, 0x7f2800, 0x6ef9d0, 0x2f0e90},
    {"1.2.0.9",
     "3eb4649c01c6a4e01f86ace3bd8135d6359ec3774cf93e17ab69c0f1b65e6d28",
     "3cda80ab81ba3f30acfb972c3c71caf460fdc618548f9f87e88d0aab2b96fa26",
     0x7242f0, 0x5c6a63, {0xe8, 0x8d, 0xd8, 0x15, 0x00},
     0x7260c0, 0x832510, 0x725880, 0x30dd10,
     &kSpaceProgramEngineLayout, &kSpaceProgramNativeUiLayout}
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
