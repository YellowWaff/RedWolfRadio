# SPDX-License-Identifier: GPL-3.0-only
"""Generate short, audibly distinct FLAC and MP3 fixtures for native-engine tests."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import soundfile as sf


RATE = 48_000
AMPLITUDE = 0.22
FADE_SAMPLES = 480


def sequence(frequencies: list[float], seconds_per_step: float, silence_seconds: float) -> np.ndarray:
    step_frames = round(RATE * seconds_per_step)
    silence_frames = round(RATE * silence_seconds)
    tone_frames = step_frames - silence_frames
    pieces: list[np.ndarray] = []
    for frequency in frequencies:
        phase = np.arange(tone_frames, dtype=np.float64) * (2.0 * np.pi * frequency / RATE)
        mono = (np.sin(phase) * AMPLITUDE).astype(np.float32)
        fade = np.linspace(0.0, 1.0, FADE_SAMPLES, endpoint=True, dtype=np.float32)
        mono[:FADE_SAMPLES] *= fade
        mono[-FADE_SAMPLES:] *= fade[::-1]
        pieces.append(mono)
        pieces.append(np.zeros(silence_frames, dtype=np.float32))
    mono = np.concatenate(pieces)
    return np.column_stack((mono, mono))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    args.output.mkdir(parents=True, exist_ok=True)

    flac_path = args.output / "MusicTrace-descending.flac"
    mp3_path = args.output / "MusicTrace-alternating.mp3"
    flac = sequence([990, 770, 550, 440, 330], 0.8, 0.05)
    mp3 = sequence([330, 880, 330, 880, 330, 880], 0.75, 0.05)
    sf.write(flac_path, flac, RATE, format="FLAC", subtype="PCM_16")
    sf.write(
        mp3_path,
        mp3,
        RATE,
        format="MP3",
        subtype="MPEG_LAYER_III",
        compression_level=0.5,
        bitrate_mode="VARIABLE",
    )

    for path, expected_frames in ((flac_path, len(flac)), (mp3_path, len(mp3))):
        info = sf.info(path)
        if info.samplerate != RATE or info.channels != 2 or info.frames != expected_frames:
            raise RuntimeError(f"unexpected encoded fixture metadata: {path}: {info}")
        print(f"{path.name}: {info.format}/{info.subtype}, {info.frames / info.samplerate:.3f}s")


if __name__ == "__main__":
    main()
