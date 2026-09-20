# Changelog

## Unreleased

- Resolve original music beside the game executable and plugin-local music beside the loaded Red Wolf Radio DLL, including separate Development and Workshop installations.
- Log resolved source folders and stable original-track IDs with their inclusion/exclusion status.
- Verify the local Development package's artwork, independent DLL/INI location, local WAV playback, and original-track modes in the game; preserve the tested loose DLL and existing user settings during migration.
- Add configurable gameplay shortcuts for Next, Previous, and Play/Pause in the INI.
- Make Previous restart the current track; repeated presses within one second step backward through playback history.
- Retain the last three previous tracks by default with `Cache.HistoryTracks`, sharing the bounded cache.
- Pause and resume the XAudio2 source voice at its actual playback position; retain the game's music-volume behavior.
- Disable invalid or duplicate hotkey bindings with a diagnostic while preserving music settings.
- Prepare historical replay targets in the background and keep navigation on the game music thread.
- Verify the bounded-cache build with a 65-minute gameplay run and a 10-minute restart, including 12 completed WAV tracks and cache reuse.
- Verify Next, restart-first Previous, true pause/resume, and silent navigation while paused in the game. Rapid backward navigation reaches the start of the session history and then restarts that earliest entry.

- Expand Windows environment variables in `Sources.ExternalPath`.
- Write `%USERPROFILE%\Music` into newly created INI files without embedding a username.
- Keep a blank `ExternalPath` as the redirected Windows Music known-folder fallback.
- Add a configurable bounded derived-audio cache, defaulting to 4096 MiB.
- Prepare a configurable upcoming playlist window, defaulting to five tracks, instead of converting the complete library at startup.
- Evict least-recently-used cache files while protecting the active and upcoming tracks.
- Add the approved Red Wolf Radio logo and launcher artwork.
- Add reproducible standalone and Workshop package generation, including the RML-discoverable `previewimage.png`.

## 0.8.1 - 2026-09-16

- Add a fresh launch-time catalog for arbitrary WAV, FLAC, MP3, and XWMA filenames.
- Convert WAV, FLAC, and MP3 inputs to game-compatible cached WAV files.
- Preserve original game files and native original XWMA playback.
- Add `all`, `selected`, and `none` original-track modes.
- Keep game music volume, pause, resume, stop, and track transitions working.
- Reshuffle after every complete playlist cycle without repeating the boundary track.
- Log the selected stable track ID and source path for diagnostics.
