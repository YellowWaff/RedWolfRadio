// Generates a deterministic 12-second PCM16 stereo WAV for the native-engine probe.
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static void u16(FILE* f, uint16_t v) { fputc(v & 255, f); fputc(v >> 8, f); }
static void u32(FILE* f, uint32_t v) { u16(f, v & 0xffff); u16(f, v >> 16); }

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    constexpr uint32_t rate = 48000, channels = 2, seconds = 12;
    constexpr uint32_t frames = rate * seconds, dataBytes = frames * channels * 2;
    FILE* f = fopen(argv[1], "wb");
    if (!f) return 3;
    fwrite("RIFF", 1, 4, f); u32(f, 36 + dataBytes); fwrite("WAVEfmt ", 1, 8, f);
    u32(f, 16); u16(f, 1); u16(f, channels); u32(f, rate); u32(f, rate * channels * 2);
    u16(f, channels * 2); u16(f, 16); fwrite("data", 1, 4, f); u32(f, dataBytes);
    const uint32_t pitches[6] = {330, 440, 550, 660, 770, 880};
    for (uint32_t frame = 0; frame < frames; ++frame) {
        const uint32_t segmentFrame = frame % (rate * 2), pitch = pitches[frame / (rate * 2)];
        const uint32_t period = rate / pitch, phase = frame % period;
        int32_t triangle = phase < period / 2 ? static_cast<int32_t>(phase * 4 * 5000 / period - 5000)
                                             : static_cast<int32_t>(15000 - phase * 4 * 5000 / period);
        uint32_t fade = segmentFrame < 1200 ? segmentFrame : (rate * 2 - segmentFrame < 1200 ? rate * 2 - segmentFrame : 1200);
        int16_t sample = static_cast<int16_t>(triangle * static_cast<int32_t>(fade) / 1200);
        u16(f, static_cast<uint16_t>(sample)); u16(f, static_cast<uint16_t>(sample));
    }
    const bool ok = !ferror(f) && fclose(f) == 0;
    return ok ? 0 : 4;
}
