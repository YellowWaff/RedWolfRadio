// SPDX-License-Identifier: GPL-3.0-only
#include "../src/game_build.h"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

static void require(bool ok, const char* why) {
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}
int main() {
    const auto& oldBuild = rwr::kVerifiedGameBuilds[0];
    const auto& newBuild = rwr::kVerifiedGameBuilds[1];
    for (const auto& build : rwr::kVerifiedGameBuilds) {
        require(rwr::findVerifiedGameBuild(build.exeHash, build.engineHash) == &build,
            "verified pair selects its own address profile");
        int32_t displacement = 0;
        std::memcpy(&displacement, build.scriptDialogCall + 1, sizeof(displacement));
        require(build.scriptDialogCall[0] == 0xe8 &&
            static_cast<int64_t>(build.scriptDialogReturnRva) + displacement ==
                static_cast<int64_t>(build.nativeDialogRva),
            "guarded Script call resolves to the selected native dialog");
    }
    require(!rwr::findVerifiedGameBuild(oldBuild.exeHash, newBuild.engineHash) &&
        !rwr::findVerifiedGameBuild(newBuild.exeHash, oldBuild.engineHash),
        "mixed-version EXE/engine pairs remain unsupported");
    require(!rwr::findVerifiedGameBuild(nullptr, newBuild.engineHash) &&
        !rwr::findVerifiedGameBuild(newBuild.exeHash, nullptr) &&
        !rwr::findVerifiedGameBuild("", "") &&
        !rwr::findVerifiedGameBuild("unknown", newBuild.engineHash),
        "missing and unknown binaries cannot select a hook profile");
    puts("PASS: verified game pairs, mixed/unknown build rejection and exact Script call destinations.");
}
