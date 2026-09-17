# Red Wolf Radio

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

[Originals]
; all, selected, or none
Mode=all
; Used by selected mode. IDs are comma-separated and appear in the plugin log.
ExcludedIds=
```

Environment variables such as `%USERPROFILE%\Music` are expanded when the plugin loads the configuration. Leaving the value blank uses the Windows Music known folder, which follows folder redirection to locations such as OneDrive.

## Data and diagnostics

Derived audio and logs are written under `%LOCALAPPDATA%\RedWolfRadio`. Original music and user source files are read-only inputs. The cache can be deleted while the game is closed; Red Wolf Radio recreates required files on the next launch.

`MaxSizeMiB=0` disables cache eviction. With a finite limit, Red Wolf Radio removes the least-recently-used derived files first. The active track and the upcoming `PrefetchTracks` window are never evicted, so the cache can temporarily remain above the configured limit when those protected tracks alone are larger than the limit.

## Build and test

The Windows build uses an LLVM MinGW x86-64 toolchain and PowerShell:

```powershell
.\build.ps1 -Toolchain C:\path\to\llvm-mingw
```

The build runs converter fixtures, forwarding and scheduler tests, checks DLL exports/imports, and prints SHA-256 hashes. Miniaudio 0.11.25 is vendored for offline decoding and conversion only; see [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md).

## License

Red Wolf Radio's source is licensed under GPL-3.0-only. Vendored dependencies retain their own licenses.
