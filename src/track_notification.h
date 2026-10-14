// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <string>
#include <stdint.h>
namespace rwr {
inline std::wstring notificationName(std::wstring name, size_t limit) {
    for (wchar_t& c : name) if (c < 32 || c == 127) c = L' ';
    if (name.size() > limit && limit > 1) {
        size_t end = limit - 1;
        if (end && name[end - 1] >= 0xd800 && name[end - 1] <= 0xdbff) --end;
        name.resize(end); name += L'\u2026';
    }
    return name;
}
}
