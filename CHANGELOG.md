# Changelog

## 1.0.0 - 2026-10-13

- Play supported external RIFF XWMA/WMAudio2 through the game's native loader without WAV conversion; preserve source files and use compressed copies only for Unicode/long paths.
- Add a 360x128 movable player from Scripts (0101), with live source filenames and Previous, Restart, Play/Pause and Next icons. Hover descriptions appear in the title bar.
- Keep keyboard Previous's restart-first behavior; the player has separate direct Previous and Restart actions. Navigation while paused stays paused.
- Add English/Russian player descriptions, bilingual INI help, Russian documentation/listing text, and English/Russian graphic guides.
- Enable the Script-menu player by default. Keep the optional five-second HUD notification off by default and preserve explicitly configured INI settings.
- Bundle the companion script, menu icon and registration instructions in both release archives.
- Validate XWMA, source labels, smaller-player layout, all controls, input overlap, 1440x1080 readability and normal shutdown/relaunch through automated tests, user acceptance and log review.

## 1.0.0-player2 - 2026-10-13 (local presentation prototype)

- Shrink the movable player to 360x128 UI units, about half its previous area, retaining two source-filename lines.
- Replace button captions with drawn transport icons; keep English/Russian action descriptions on hover in the title bar.
- Retain the existing four music actions, guarded input, dragging and automatic title refresh.

## 1.0.0-rc2 - 2026-10-12 (local release candidate)

- Add a compact player opened from the Script menu: one live song window with automatic refresh, dragging, and Previous, Restart, Play/Pause and Next buttons.
- Queue button commands for the music thread; preserve the existing configurable keyboard behavior and paused navigation.
- Draw arbitrary source names with the native non-formatting Unicode API, including literal percent signs.
- Enable the Script-menu player by default; keep the optional five-second HUD notification off by default. Explicit existing INI settings remain in effect.
- Include the companion Script-menu action, icon and registration instructions in both archives.
- Record successful local gameplay acceptance of full XWMA tracks, live updates, all controls, window overlap, 1440x1080 layout and normal shutdown/relaunch. The packaged rc2 build still needs a final installation smoke check before publication.

## 1.0.0-rc1 - 2026-10-10 (local candidate, pending gameplay acceptance)

- Prepare custom RIFF XWMA/WMAudio2 files for the game's existing music loader without WAV conversion; validate packet metadata first.
- Route short ASCII XWMA paths directly; copy compressed bytes unchanged to the bounded cache for Unicode/long source paths. Source bytes and timestamps remain untouched.
- Show the source filename without extension for five seconds after confirmed playback starts. Add Display.ShowTrackName (true by default) and Display.Language (en/ru).
- Use ForceNodes' optional frame service when available; otherwise install the verified gameplay frame hook only while its original prefix is intact.
- Add bilingual INI help, Russian documentation and Workshop copy, and localized infographic assets.
- Preserve existing INI settings; missing new keys use defaults. External XWMA and HUD still require in-game acceptance before a final 1.0 release.

## 0.9.0 - 2026-09-20

- Change the default Play/Pause shortcut to Ctrl+Shift+Down to avoid also pausing the game with Space. Existing INI bindings remain configurable.
- Verify that an ordinary Steam Workshop incremental update retains a customized RedWolfRadio.ini while replacing the plugin DLL.
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
