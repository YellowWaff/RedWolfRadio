// SPDX-License-Identifier: GPL-3.0-only
#include "../src/game_build.h"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void require(bool ok, const char* why) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}
int main() {
    for (const auto& build : rwr::kVerifiedGameBuilds) {
        require(std::strlen(build.exeHash) == 64 && std::strlen(build.engineHash) == 64,
            "each verified binary uses its full SHA-256");
        require(rwr::findVerifiedGameBuild(build.exeHash, build.engineHash) == &build,
            "verified pair selects its own address profile");
        int32_t displacement = 0;
        std::memcpy(&displacement, build.scriptDialogCall + 1, sizeof(displacement));
        require(build.scriptDialogCall[0] == 0xe8 &&
            static_cast<int64_t>(build.scriptDialogReturnRva) + displacement ==
                static_cast<int64_t>(build.nativeDialogRva),
            "guarded Script call resolves to the selected native dialog");
        // A near match must not bypass the exact-binary guard. Mutating the
        // final digit also catches accidental truncated/prefix comparisons.
        std::array<char, 65> changedExe{}, changedEngine{};
        std::memcpy(changedExe.data(), build.exeHash, changedExe.size());
        std::memcpy(changedEngine.data(), build.engineHash, changedEngine.size());
        changedExe[63] = changedExe[63] == '0' ? '1' : '0';
        changedEngine[63] = changedEngine[63] == '0' ? '1' : '0';
        require(!rwr::findVerifiedGameBuild(changedExe.data(), build.engineHash) &&
            !rwr::findVerifiedGameBuild(build.exeHash, changedEngine.data()),
            "a one-digit EXE or engine hash mismatch remains unsupported");
        require(!rwr::findVerifiedGameBuild(nullptr, build.engineHash) &&
            !rwr::findVerifiedGameBuild(build.exeHash, nullptr) &&
            !rwr::findVerifiedGameBuild("unknown", build.engineHash) &&
            !rwr::findVerifiedGameBuild(build.exeHash, "unknown"),
            "missing and unknown binaries cannot select any hook profile");
    }
    for (const auto& executable : rwr::kVerifiedGameBuilds)
        for (const auto& engine : rwr::kVerifiedGameBuilds) {
            const auto* selected = rwr::findVerifiedGameBuild(executable.exeHash, engine.engineHash);
            // Different versions can legitimately share an identical engine.
            // Any accepted combination must identify that exact pair; a mixed
            // pair must never inherit either version's unrelated address set.
            require(!selected || (!std::strcmp(selected->exeHash, executable.exeHash) &&
                !std::strcmp(selected->engineHash, engine.engineHash)),
                "all cross-version combinations retain exact-pair identity");
        }
    require(!rwr::findVerifiedGameBuild("", "") && !rwr::findVerifiedGameBuild(nullptr, nullptr),
        "empty binary identities cannot select a hook profile");
    puts("PASS: every verified pair, all cross-version combinations, exact-hash rejection and Script call destinations.");
}
