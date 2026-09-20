// SPDX-License-Identifier: GPL-3.0-only
// Standalone, silent XAudio2 2.8 check. Requires a working default audio device;
// does not open/inject the game or read any of its files.
#include "../src/music_voice_control.h"
#include <xaudio2.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
void require(bool value, const char* description) {
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", description); std::exit(1); }
}
}

int main() {
    require(SUCCEEDED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)), "initialize COM");
    IXAudio2* audio = nullptr;
    require(SUCCEEDED(XAudio2Create(&audio, 0, XAUDIO2_DEFAULT_PROCESSOR)), "create XAudio2 2.8");
    IXAudio2MasteringVoice* master = nullptr;
    require(SUCCEEDED(audio->CreateMasteringVoice(&master)), "create silent-test master voice");
    WAVEFORMATEX format{};
    format.wFormatTag = WAVE_FORMAT_PCM;
    format.nChannels = 2;
    format.nSamplesPerSec = 48000;
    format.wBitsPerSample = 16;
    format.nBlockAlign = 4;
    format.nAvgBytesPerSec = 192000;
    IXAudio2SourceVoice* voice = nullptr;
    require(SUCCEEDED(audio->CreateSourceVoice(&voice, &format)), "create PCM source voice");
    std::vector<short> silence(48000 * 2 * 4, 0);
    XAUDIO2_BUFFER buffer{};
    buffer.Flags = XAUDIO2_END_OF_STREAM;
    buffer.AudioBytes = static_cast<UINT32>(silence.size() * sizeof(short));
    buffer.pAudioData = reinterpret_cast<const BYTE*>(silence.data());
    require(SUCCEEDED(voice->SubmitSourceBuffer(&buffer)), "submit four seconds of silence");
    require(SUCCEEDED(voice->Start()), "start silent sample counter");
    Sleep(150);
    XAUDIO2_VOICE_STATE before{};
    voice->GetState(&before);
    require(before.SamplesPlayed > 0 && before.BuffersQueued == 1, "sample cursor initially advances");

    std::vector<unsigned char> engine(rwr::kMusicSourceVoiceRva + sizeof(void*));
    std::memcpy(engine.data() + rwr::kMusicSourceVoiceRva, &voice, sizeof(voice));
    volatile char loading = 0;
    auto module = reinterpret_cast<HMODULE>(engine.data());
    auto paused = rwr::setMusicVoicePaused(module, &loading, true);
    require(paused.status == rwr::VoiceControlStatus::Applied, "helper stops real source voice");
    Sleep(100); // Stop is asynchronous; allow one audio processing quantum.
    XAUDIO2_VOICE_STATE stoppedFirst{}, stoppedLater{};
    voice->GetState(&stoppedFirst);
    Sleep(150);
    voice->GetState(&stoppedLater);
    require(stoppedLater.SamplesPlayed == stoppedFirst.SamplesPlayed && stoppedLater.BuffersQueued == 1,
        "paused voice retains queued buffer and frozen cursor");
    auto resumed = rwr::setMusicVoicePaused(module, &loading, false, paused.voice);
    require(resumed.status == rwr::VoiceControlStatus::Applied, "helper restarts real source voice");
    Sleep(150);
    XAUDIO2_VOICE_STATE playingAgain{};
    voice->GetState(&playingAgain);
    require(playingAgain.SamplesPlayed > stoppedLater.SamplesPlayed && playingAgain.BuffersQueued == 1,
        "resumed voice continues after frozen cursor");
    std::printf("Silent XAudio2 2.8 test passed: before=%llu, paused=%llu -> %llu, resumed=%llu.\n",
        static_cast<unsigned long long>(before.SamplesPlayed),
        static_cast<unsigned long long>(stoppedFirst.SamplesPlayed),
        static_cast<unsigned long long>(stoppedLater.SamplesPlayed),
        static_cast<unsigned long long>(playingAgain.SamplesPlayed));
    voice->DestroyVoice();
    master->DestroyVoice();
    audio->Release();
    CoUninitialize();
}
