// SPDX-License-Identifier: GPL-3.0-only
#include "../src/music_voice_control.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
void require(bool value, const char* description) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", description); std::exit(1); }
}
struct FakeVoice { void** vtable; unsigned starts = 0, stops = 0; bool paused = false; };
HRESULT WINAPI start(void* pointer, UINT32 flags, UINT32 operation) {
    require(flags == 0 && operation == 0, "Start uses immediate operation and no flags");
    auto& voice = *static_cast<FakeVoice*>(pointer);
    ++voice.starts; voice.paused = false;
    SetLastError(5678);
    return S_OK;
}
HRESULT WINAPI stop(void* pointer, UINT32 flags, UINT32 operation) {
    require(flags == 0 && operation == 0, "Stop uses immediate operation and no tails");
    auto& voice = *static_cast<FakeVoice*>(pointer);
    ++voice.stops; voice.paused = true;
    SetLastError(5678);
    return S_OK;
}
HRESULT WINAPI failed(void*, UINT32, UINT32) { return E_FAIL; }
} // namespace

int main() {
    std::vector<unsigned char> engine(rwr::kMusicSourceVoiceRva + sizeof(void*));
    const HMODULE module = reinterpret_cast<HMODULE>(engine.data());
    volatile char loading = 0;
    void* vtable[26]{};
    vtable[rwr::kMusicVoiceStartSlot] = reinterpret_cast<void*>(start);
    vtable[rwr::kMusicVoiceStopSlot] = reinterpret_cast<void*>(stop);
    FakeVoice voice{vtable};
    void* pointer = &voice;
    const auto setPointer = [&](void* value) {
        std::memcpy(engine.data() + rwr::kMusicSourceVoiceRva, &value, sizeof(value));
    };
    setPointer(pointer);
    SetLastError(1234);
    auto result = rwr::setMusicVoicePaused(module, &loading, true);
    require(result.status == rwr::VoiceControlStatus::Applied && result.voice == pointer &&
        voice.paused && voice.stops == 1 && voice.starts == 0, "pause uses only source Stop");
    require(GetLastError() == 1234, "pause preserves thread last error");
    result = rwr::setMusicVoicePaused(module, &loading, false, pointer);
    require(result.status == rwr::VoiceControlStatus::Applied && !voice.paused &&
        voice.starts == 1 && voice.stops == 1, "resume uses only source Start");
    require(GetLastError() == 1234, "resume preserves thread last error");
    loading = 1;
    result = rwr::setMusicVoicePaused(module, &loading, true);
    require(result.status == rwr::VoiceControlStatus::Loading && voice.stops == 1,
        "loading never touches voice");
    loading = 0;
    setPointer(nullptr);
    require(rwr::setMusicVoicePaused(module, &loading, true).status == rwr::VoiceControlStatus::NoVoice,
        "missing voice is benign");
    setPointer(pointer);
    require(rwr::setMusicVoicePaused(module, &loading, false, &vtable).status ==
        rwr::VoiceControlStatus::VoiceChanged && voice.starts == 1, "do not resume replacement voice");
    setPointer(reinterpret_cast<void*>(1));
    require(rwr::setMusicVoicePaused(module, &loading, true).status == rwr::VoiceControlStatus::InvalidVoice,
        "unreadable voice is rejected without dereference");
    setPointer(pointer);
    vtable[rwr::kMusicVoiceStopSlot] = &engine;
    require(rwr::setMusicVoicePaused(module, &loading, true).status == rwr::VoiceControlStatus::InvalidVoice,
        "nonexecutable method is rejected");
    vtable[rwr::kMusicVoiceStopSlot] = reinterpret_cast<void*>(failed);
    result = rwr::setMusicVoicePaused(module, &loading, true);
    require(result.status == rwr::VoiceControlStatus::Failed && result.error == E_FAIL,
        "voice HRESULT propagated");
    require(rwr::setMusicVoicePaused(nullptr, &loading, true).status == rwr::VoiceControlStatus::InvalidVoice,
        "missing engine rejected");
    require(rwr::setMusicVoicePaused(module, nullptr, true).status == rwr::VoiceControlStatus::InvalidVoice,
        "missing loading guard rejected");
    std::puts("Music source-voice control tests passed.");
}
