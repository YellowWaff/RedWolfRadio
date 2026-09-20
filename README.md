# Red Wolf Radio

<p align="center">
  <img src="assets/red-wolf-radio-primary-logo-transparent.png" alt="Red Wolf Radio logo" width="520">
</p>

Red Wolf Radio adds user music to **Workers & Resources: Soviet Republic** through Republic Mod Loader while preserving the game's files and music controls.

Version 0.8.1 is the first tested baseline. It is pinned to Workers & Resources 1.1.1.9 and Republic Mod Loader 1.0.1. Compatibility with other game or loader builds is not assumed because the plugin verifies the executable and engine binaries before installing its hooks.

## Current behavior

- Scans enabled music folders on every game launch; filenames and track counts are unrestricted.
- Reads an external folder and an optional `Music` folder beside `RedWolfRadio.dll`.
- Plays WAV, FLAC, and MP3 files through validated, game-compatible cached WAV files.
- Preserves the game's original XWMA playback and never edits original game files.
- Supports all original tracks, selected exclusions, or custom music only.
- Uses the game's music volume, pause, resume, stop, and next-track behavior.
- Shuffles the complete eligible catalog and starts a new shuffled cycle after every track has played.
- Provides configurable Next, Previous, and true Play/Pause shortcuts during gameplay.

External XWMA discovery exists but remains preliminary until a broader external-file test matrix is complete. The current development build bounds derived audio and prepares only the upcoming playlist window; the tested `v0.8.1` tag still prepares the complete custom catalog during startup.

## Install

Copy these files into Republic Mod Loader's plugin folder for the game:

```text
RedWolfRadio.dll
RedWolfRadio.ini
```

Keep the INI filename identical to the DLL stem so Republic Mod Loader can expose it through **Open settings**. The plugin creates the file on first launch if it is absent. Changes take effect on the next game launch.

## Configure

```ini
[RedWolfRadio]
SchemaVersion=1

[Sources]
ExternalEnabled=true
; Environment variables such as %USERPROFILE% are expanded.
; Blank uses the current Windows account's configured Music known folder.
ExternalPath=%USERPROFILE%\Music
ExternalRecursive=true
; The optional local source is Music beside RedWolfRadio.dll.
PluginLocalEnabled=false
PluginLocalRecursive=true

[Cache]
; Maximum derived-audio cache size in MiB. Zero means unlimited.
MaxSizeMiB=4096
; Number of upcoming playlist entries kept ready.
PrefetchTracks=5
; Retain this many previous tracks in the same cache.
HistoryTracks=3

[Hotkeys]
Enabled=true
Next=Ctrl+Shift+Right
Previous=Ctrl+Shift+Left
PlayPause=Ctrl+Shift+Space

[Originals]
; all, selected, or none
Mode=all
; Used by selected mode. IDs are comma-separated and appear in the plugin log.
ExcludedIds=
```

Environment variables such as `%USERPROFILE%\Music` are expanded when the plugin loads the configuration. Leaving the value blank uses the Windows Music known folder, which follows folder redirection to locations such as OneDrive.

## Data and diagnostics

Derived audio and logs are written under `%LOCALAPPDATA%\RedWolfRadio`. Original music and user source files are read-only inputs. The cache can be deleted while the game is closed; Red Wolf Radio recreates required files on the next launch.

`MaxSizeMiB=0` disables cache eviction. With a finite limit, Red Wolf Radio removes the least-recently-used derived files first. The active track, the upcoming `PrefetchTracks` window, and the last `HistoryTracks` previous tracks are protected. A pending navigation target is also protected. They share one cache without duplicate audio files. The cache can temporarily remain above the configured limit when protected tracks alone are larger than the limit.

## Keyboard controls

The shortcuts work while a republic is loaded and the game is the foreground application. They are not active on the main menu. Changes to the INI take effect after restarting the game.

| Default shortcut | Action |
|---|---|
| Ctrl+Shift+Right | Next track; after going backward, move forward through played history first. |
| Ctrl+Shift+Left | Restart the current track. Press again within one second to go back one track; further quick presses go farther back. |
| Ctrl+Shift+Space | Pause at the current audio position, or resume from it. |

Next and Previous preserve a user pause: a newly selected song waits silently at its beginning until resumed. One keypress triggers one action; holding the keys does not repeat. History holds up to 64 playback entries per launch, including tracks selected while paused; the most recent three previous tracks stay ready by default. Set `HistoryTracks` from 0 to 64 to change retention. Older unprotected tracks are prepared again if needed, while the current song continues. At the beginning of history, Previous restarts the oldest available track.

Bindings accept letters, digits, F1-F24, arrows, Space, PageUp/PageDown, Home, End, Insert, Delete, Enter, Escape, Tab, Backspace, and MediaNext/MediaPrevious/MediaPlayPause. Combine a key with Ctrl, Shift, Alt, or Win using `+`, such as `Alt+N`. Names are case-insensitive. Use `None` to disable an action, or `Enabled=false` to disable all shortcuts. Invalid or duplicate bindings are disabled and explained in the startup log; music and original-track settings remain active. Shortcuts do not consume game keyboard input, so choose combinations that do not conflict with your game controls.

In diagnostic TSVs, phase `H` records shortcut requests/results: `arg0` identifies Next=1, Previous=2, PlayPause=3; `arg1` is applied=1, pending=2, ignored=0, or failed=-1. The detail field describes the action. `Play/D` means a selected track is waiting for the user to resume.

## Build and test

The Windows build uses an LLVM MinGW x86-64 toolchain and PowerShell:

```powershell
.\build.ps1 -Toolchain C:\path\to\llvm-mingw
.\package-release.ps1
```

The build runs converter fixtures, forwarding and scheduler tests, checks DLL exports/imports, and prints SHA-256 hashes. The packaging script creates both a conventional release archive and a Workshop-ready archive containing the approved preview artwork. Miniaudio 0.11.25 is vendored for offline decoding and conversion only; see [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

## Launcher and Workshop artwork

Republic Mod Loader 1.0.1 gets a plugin's detail image from its parent Steam Workshop item. It does not define a per-plugin image field. A standalone Red Wolf Radio Workshop package therefore places [`previewimage.png`](previewimage.png) at the item root and the plugin files in a `plugin` directory. RML discovers that layout and displays the Workshop preview for the plugin.

A local DLL copied directly into RML's own plugin directory has no separate Workshop identity, so RML cannot display Red Wolf Radio's image there. Do not replace Republic Mod Loader's own preview image. See [DISTRIBUTION.md](DISTRIBUTION.md) for the verified package layout and source references.

## License

Red Wolf Radio's source is licensed under GPL-3.0-only. Vendored dependencies retain their own licenses. The logo and launcher artwork have separate terms documented in [assets/README.md](assets/README.md).
