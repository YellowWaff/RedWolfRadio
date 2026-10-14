// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include <array>
#include <string_view>

namespace rwr {
inline constexpr wchar_t kSongPopupMarker[] =
    L"Red Wolf Radio: song information unavailable. Enable the radio plugin in Republic Mod Loader. [RWR_NOW_PLAYING_V1]";
inline constexpr size_t kSongPopupLimit = 250;
enum class SongPopupState { Playing, Paused, Loading, Stopped, Unavailable, Busy };
using SongPopupText = std::array<wchar_t, kSongPopupLimit + 1>;

// The native dialog wraps text and then uses each line as a printf format.
// Never admit ASCII percent, even doubled: wrapping could split the pair.
// This diagnostic representation does not change the source file or catalog.
inline SongPopupText songPopupText(std::wstring_view name, SongPopupState state, bool russian) {
    SongPopupText result{};
    size_t used = 0;
    bool truncated = false;
    auto append = [&](std::wstring_view value) {
        for (size_t i = 0; i < value.size() && !truncated; ++i) {
            wchar_t c = value[i];
            if (c >= 0xd800 && c <= 0xdbff && i + 1 < value.size() &&
                value[i + 1] >= 0xdc00 && value[i + 1] <= 0xdfff) {
                if (used + 2 > kSongPopupLimit) { truncated = true; break; }
                result[used++] = c; result[used++] = value[++i];
            } else {
                if (used == kSongPopupLimit) { truncated = true; break; }
                if (c >= 0xd800 && c <= 0xdfff) c = L'\ufffd';
                result[used++] = c;
            }
        }
    };
    append(L"Red Wolf Radio\n");
    const wchar_t* label = L"";
    switch (state) {
    case SongPopupState::Playing: label = russian ? L"Сейчас играет:" : L"Now playing:"; break;
    case SongPopupState::Paused: label = russian ? L"Пауза:" : L"Paused:"; break;
    case SongPopupState::Loading: label = russian ? L"Загрузка:" : L"Loading:"; break;
    case SongPopupState::Stopped: label = russian ? L"Сейчас ничего не играет." : L"No track is playing."; break;
    case SongPopupState::Unavailable: label = russian ? L"Информация о музыке недоступна." : L"Song information unavailable."; break;
    case SongPopupState::Busy: label = russian ? L"Обновление." : L"Updating."; break;
    }
    append(label);
    if (!name.empty()) {
        append(L"\n");
        for (size_t i = 0; i < name.size() && !truncated; ++i) {
            const wchar_t c = name[i];
            if (c == L'%') append(L"[percent]");
            else if (c < 32 || (c >= 127 && c <= 159)) append(L" ");
            else if (c >= 0xd800 && c <= 0xdbff && i + 1 < name.size() &&
                name[i + 1] >= 0xdc00 && name[i + 1] <= 0xdfff) {
                append(name.substr(i, 2)); ++i;
            } else append(name.substr(i, 1));
        }
    }
    if (truncated) {
        if (used == kSongPopupLimit) {
            --used;
            if (used && result[used] >= 0xdc00 && result[used] <= 0xdfff &&
                result[used - 1] >= 0xd800 && result[used - 1] <= 0xdbff) --used;
        }
        result[used++] = L'\u2026';
    }
    result[used] = 0;
    return result;
}
} // namespace rwr
