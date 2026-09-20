// SPDX-License-Identifier: GPL-3.0-only
#pragma once

#include <windows.h>
#include <cstdint>

namespace rwr {

static_assert(sizeof(void*) == 8, "Verified music voice control requires x64");

// Only use after the existing engine hash check, and on the game music thread.
// The verified C3DDLL64.dll imports XAudio2_8.dll. Its source voice lives at
// RVA 0x1dc7f0; the XAudio2 2.8 x64 Start/Stop slots are 19/20. The game's
// Pause/Resume exports merely change volume and do not preserve song position.
// IXAudio2SourceVoice::Stop(0, 0) preserves buffers and the playback cursor:
// https://learn.microsoft.com/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2sourcevoice-stop
constexpr uintptr_t kMusicSourceVoiceRva = 0x1dc7f0;
constexpr unsigned kMusicVoiceStartSlot = 19;
constexpr unsigned kMusicVoiceStopSlot = 20;

enum class VoiceControlStatus { Applied, Loading, NoVoice, InvalidVoice, VoiceChanged, Failed };

struct VoiceControlResult {
    VoiceControlStatus status = VoiceControlStatus::InvalidVoice;
    HRESULT error = E_POINTER;
    void* voice = nullptr;
};

namespace detail {

inline bool readVoiceMemory(const void* source, void* destination, SIZE_T bytes) {
    SIZE_T copied = 0;
    return source && ReadProcessMemory(GetCurrentProcess(), source, destination, bytes, &copied) &&
        copied == bytes;
}

inline bool executableVoiceMethod(const void* method) {
    MEMORY_BASIC_INFORMATION memory{};
    if (!method || !VirtualQuery(method, &memory, sizeof(memory)) || memory.State != MEM_COMMIT ||
        (memory.Protect & (PAGE_GUARD | PAGE_NOACCESS))) return false;
    const DWORD protection = memory.Protect & 0xff;
    return protection == PAGE_EXECUTE || protection == PAGE_EXECUTE_READ ||
        protection == PAGE_EXECUTE_READWRITE || protection == PAGE_EXECUTE_WRITECOPY;
}

} // namespace detail

// Checks memory before invoking a captured source-voice method; it never writes
// engine globals, flushes buffers, destroys a voice, or calls a music export.
// A memory probe does not synchronize the engine: the caller must execute on
// the same thread as music status/load/stop, never the hotkey polling worker.
// While loading, defer the command until a later music status poll.
// expectedVoice optionally prevents a resume from applying to a replaced voice.
inline VoiceControlResult setMusicVoicePaused(HMODULE engine, const volatile char* loadingFlag,
    bool paused, void* expectedVoice = nullptr) {
    const DWORD saved = GetLastError();
    VoiceControlResult result{};
    const auto finish = [&]() { SetLastError(saved); return result; };
    char loading = 0;
    if (!engine || !loadingFlag || !detail::readVoiceMemory(
        const_cast<const char*>(loadingFlag), &loading, sizeof(loading))) return finish();
    if (loading) {
        result.status = VoiceControlStatus::Loading;
        result.error = S_FALSE;
        return finish();
    }
    const auto* slot = reinterpret_cast<const unsigned char*>(engine) + kMusicSourceVoiceRva;
    if (!detail::readVoiceMemory(slot, &result.voice, sizeof(result.voice))) return finish();
    if (!result.voice) {
        result.status = VoiceControlStatus::NoVoice;
        result.error = S_FALSE;
        return finish();
    }
    if (expectedVoice && result.voice != expectedVoice) {
        result.status = VoiceControlStatus::VoiceChanged;
        result.error = S_FALSE;
        return finish();
    }
    const void* vtable = nullptr;
    const void* method = nullptr;
    const unsigned methodSlot = paused ? kMusicVoiceStopSlot : kMusicVoiceStartSlot;
    if (!detail::readVoiceMemory(result.voice, &vtable, sizeof(vtable)) || !vtable ||
        !detail::readVoiceMemory(static_cast<const unsigned char*>(vtable) + methodSlot * sizeof(void*),
            &method, sizeof(method)) || !detail::executableVoiceMethod(method)) return finish();

    // Recheck after the guarded pointer reads. With game-thread execution, a
    // loader cannot begin between this check and the call; only a prior loader
    // finishing can change its flag. Other ownership models need another seam.
    if (!detail::readVoiceMemory(const_cast<const char*>(loadingFlag), &loading, sizeof(loading)))
        return finish();
    if (loading) {
        result.status = VoiceControlStatus::Loading;
        result.error = S_FALSE;
        return finish();
    }
    using Method = HRESULT (WINAPI*)(void*, UINT32, UINT32);
    result.error = reinterpret_cast<Method>(const_cast<void*>(method))(result.voice, 0, 0);
    result.status = SUCCEEDED(result.error) ? VoiceControlStatus::Applied : VoiceControlStatus::Failed;
    return finish();
}

} // namespace rwr
