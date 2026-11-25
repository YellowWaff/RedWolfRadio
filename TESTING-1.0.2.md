# Red Wolf Radio 1.0.2 acceptance record

Prepared November 25, 2026 for WRSR **1.2.0.9**, Windows x64 and the original Republic Mod Loader **1.0.1**. **The full build, automated suites, requested manual playback/control checks, normal shutdown and relaunch verification pass. Publication and downloaded-file verification are recorded separately in [VALIDATION.md](VALIDATION.md).**

The Space Program DLC is installed on the test machine. Radio does not require that DLC. These checks cover Radio's music, player and lifecycle with that installation; they do not establish a completed Space Program campaign.

## Completed build and startup checks

- [x] Full build and automated suites for all three compatibility profiles: 1.2.0.9, 1.1.1.13 and 1.1.1.9.
- [x] Tested DLL SHA-256: `a35636a944693f5cb30cef8fc1e93f15139ad7c0c25a4cb95610b318f05b0adb`.
- [x] Tested build installed at 11:22:44 local time with the personal INI preserved byte-for-byte. Only one Radio copy is enabled; the subscribed companion Script action is available.
- [x] Actual game version 1.2.0.9 and its exact executable/engine pair selected. The earlier 1.2.0.8 download is a separate build and is not admitted by the new profile.
- [x] Startup: bootstrap `0x0`, thirteen hooks, zero hook failures, and all thirteen chain-integrity checks passed.

## Completed playback and player checks

- [x] The user confirmed normal music playback and working game music-volume adjustment.
- [x] The user confirmed a natural track transition. Live title changes were observed during GUI navigation as well.
- [x] The subscribed action under **Settings → Other → Scripts (0101) → Red Wolf Radio → green checkmark** opened the native player.
- [x] GUI Next, Pause, Previous while paused, Restart while paused and Resume produced the expected title/state changes and applied-command trace records. This covers all four player buttons.
- [x] The user confirmed dragging the red title bar works.
- [x] The user confirmed **Ctrl+Shift+Right**, **Ctrl+Shift+Left** and **Ctrl+Shift+Down** work while the game has focus.

The Previous button goes back immediately. Keyboard Previous retains its restart-first, quick-second-press behavior. The checked entries distinguish GUI/trace observations from the user's listening and physical-input confirmation.

## Completed lifecycle checks

- [x] Initial user session PID `50572`: 11:23:06–12:00:55 America/Chicago, normal exit `0x0`, roughly 37m50s.
- [x] Relaunch session PID `70772`: 12:01:17–12:11:34 America/Chicago, duration 10m17s, normal exit `0x0` through Quit to Desktop. The compact player and source title were active in a loaded republic.
- [x] Final relaunch report: bootstrap `0x0`, Radio 1.0.2 active, zero audit issues and no runtime ERROR entries. The trace flushed through a final Stop event with return `0`; it recorded five Load and five Play events, zero failed commands/load timeouts, and zero dropped events.
- [x] Final checks confirm the tested DLL is unchanged, personal INI and RML configuration are preserved, and both the game and RML are closed. Detailed receipts are recorded in [VALIDATION.md](VALIDATION.md).

## Coverage notes and optional follow-up

Music formats, controls, INI keys/defaults and cache behavior are unchanged. WAV, MP3 and FLAC retain the cached-WAV route. Supported native XWMA remains RIFF WMAudio2, mono/stereo, 16-bit decoded samples, 8–48 kHz. Record the formats actually exercised in the retained traces; this round's general listening confirmation does not establish separate fresh audible acceptance of every encoding or every Workshop XWMA profile.

Optional extended checks include a native XWMA track when available, explicit pause-position listening, Next while paused, single-window reopening, centered reopening, input overlap with other native/DLC windows, and readability at 1440×1080. Existing automated coverage and historical gameplay records remain separate from fresh live results for 1.2.0.9.

Russian player messages can be checked with `[Display] Language=ru`; the optional five-second notification can be checked with `ShowTrackName=true`. Restart after INI changes and restore the desired settings afterward. This acceptance record does not claim those optional language/notification or resolution checks were repeated for 1.0.2 unless further evidence is recorded in [VALIDATION.md](VALIDATION.md).

The older 1.1.1.13 and 1.1.1.9 profiles passed automated coverage for this release and have not been rerun live. Keep personal paths, music, logs and cache files outside public archives. Back up the customized INI and plugin-local music outside Steam/game folders before every update; restore settings with the game closed if Steam replaces them.

The accepted DLL is ready for packaging. Actual Workshop/GitHub upload and download verification is recorded separately; successful local tests alone do not establish publication.
