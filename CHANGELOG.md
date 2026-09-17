# Changelog

## 0.8.1 - 2026-09-16

- Add a fresh launch-time catalog for arbitrary WAV, FLAC, MP3, and XWMA filenames.
- Convert WAV, FLAC, and MP3 inputs to game-compatible cached WAV files.
- Preserve original game files and native original XWMA playback.
- Add `all`, `selected`, and `none` original-track modes.
- Keep game music volume, pause, resume, stop, and track transitions working.
- Reshuffle after every complete playlist cycle without repeating the boundary track.
- Log the selected stable track ID and source path for diagnostics.
