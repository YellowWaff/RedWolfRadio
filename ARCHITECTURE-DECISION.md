# Red Wolf Radio architecture decision

Decision date: 2026-09-13. Scope: first supported release for the exact WRSR 1.1.1.9 executable/engine hashes and Republic Mod Loader 1.0.1 baseline recorded in `FEASIBILITY-PLAN.md`.

## Decision

Use Republic Mod Loader's built-in Tesmio API 4 compatibility host for the hook-dependent implementation. Keep the game's `C3DMusicXWMA` engine as the only playback backend. Embed miniaudio **0.11.25**, commit `9634bedb5b5a2ca38c1ee7108a9358a4e233f14d`, for offline decoding and conversion only; compile it with device I/O and the high-level engine disabled. FLAC and MP3 inputs are converted to PCM16 stereo 48 kHz WAV files in a plugin-owned cache before any music hook is enabled. Never pass FLAC or MP3 paths to the game engine.

Use the verified `LoadMusic`, `Play`, `Stop`, `Pause`, `Resume`, `SetVolume`, and `IsPlayingMusic` route. Preserve original XWMA playback through that route. Resolve absolute custom/cache paths through the verified scoped `C3DPath_GetFullPath` hook and relocated continuation. Do not replace game DLLs or edit original music files.

This choice follows the runtime evidence:

- Native RML API 1 has no hook service; RML's built-in Tesmio API 4 host admitted and ran the tracer with TesmioLoader absent.
- The game engine played the canonical WAV probe, reported its end correctly, applied the game's music volume, and resumed an original XWMA.
- Valid FLAC and MP3 probes both produced silence and left the engine's loading flag set. The MP3 diagnostic recovered after six seconds and the scheduler immediately resumed XWMA. Unsupported containers must therefore be rejected or converted before they reach the engine.
- Miniaudio supplies independent WAV, FLAC, and MP3 decoding plus sample-format, channel, and sample-rate conversion. Its decoder can request the target PCM shape directly. Device I/O can be compiled out, so this design does not open another audio device.

Miniaudio is selected for the decoder/converter spike and becomes the shipped decoder after its pinned revision passes the fixture, malformed-input, Unicode-I/O, and license-notice checks below. If that spike fails, replace only the conversion component; the catalog, cache, hooks, and configuration contracts remain unchanged.

## Playback support contract

| Input | Release path | Status |
|---|---|---|
| `.wav` | Validate; play directly only when it is canonical PCM16 stereo 48 kHz and its routed path fits the engine buffer. Otherwise convert/copy to the short cache path. | Canonical probe verified; variant matrix pending. |
| `.flac` | Decode and convert to canonical cached WAV, then give that WAV to the game engine. | Native path rejected; converter acceptance pending. |
| `.mp3` | Decode and convert to canonical cached WAV, then give that WAV to the game engine. | Native path rejected; converter acceptance pending. |
| `.xwma` | Keep originals native. For external/plugin-local files, validate RIFF/XWMA and copy unchanged to a short cache path when required. | Original XWMA verified; arbitrary external XWMA acceptance pending. Miniaudio is not used for XWMA. |

An unrecognized, unreadable, malformed, or failed input is omitted from the eligible catalog with a diagnostic. One bad file cannot hold the scheduler in the loading state. No runtime fallback may re-enable an original track that configuration excluded.

## Storage and startup

Use `RedWolfRadio.ini` beside `RedWolfRadio.dll` for user-editable settings. Republic Mod Loader recognizes the exact DLL-stem filename and exposes it through **Open settings**. Use `%LOCALAPPDATA%\RedWolfRadio` as the writable data root:

- `cache\v1`: derived canonical WAVs and short-path copies of otherwise valid direct-play inputs.
- `logs`: startup inventory, conversion, hook activation, and playback diagnostics.

The external source defaults to `SHGetKnownFolderPath(FOLDERID_Music)` on each launch unless the user sets an explicit path. The optional plugin-local source is `Music` beside the plugin DLL and is disabled by default. Both may be enabled together.

Every launch performs a fresh directory scan. Cache metadata may avoid repeated conversion, but it never substitutes for discovery. Each catalog entry has a source-qualified stable ID formed from the source kind and normalized relative path. Enumeration indices, current file counts, and numbered game filenames are never identities or bounds.

The cache key is a digest of source content plus conversion profile and converter version. Writes go to a temporary file and are atomically renamed only after the WAV header, decoded frame count, and expected output format validate. The final cache filename is a short digest, keeping the path under the engine's verified 255-byte input limit. Removed sources stop appearing on the next launch; unreachable cache files may be pruned later without affecting discovery.

The development build uses a bounded cache and background preparation. Fresh discovery still scans the complete configured library, but conversion admits only the upcoming playback window. `[Cache] MaxSizeMiB=4096` and `PrefetchTracks=5` are user-configurable, and `MaxSizeMiB=0` explicitly opts into an unlimited cache. The active track and prepared window are protected. Least-recently-used derived files outside that set are evicted, and all filesystem and decoding work stays outside the game audio callbacks. If the protected set alone exceeds the configured limit, playback safety takes precedence and the condition is logged.

Scanning and the initial prefetch window complete before hook activation. Later preparation runs on a background worker. If initialization or compatibility validation fails, hooks remain pass-through and vanilla behavior continues. After activation, configuration is authoritative: `originals.mode = "none"` must not fall back to an original on custom-file failure.

## Configuration contract

The first implementation uses one UTF-8 INI file. If it is absent, the plugin creates it beside the DLL with a portable `%USERPROFILE%\Music` path. Environment variables are expanded when the configuration loads. A blank value resolves the Windows Music known folder and follows Windows folder redirection. Invalid settings cause a clearly logged pass-through startup rather than silently changing the user's exclusions.

```ini
[RedWolfRadio]
SchemaVersion=1

[Sources]
ExternalEnabled=true
ExternalPath=%USERPROFILE%\Music
ExternalRecursive=true
PluginLocalEnabled=false
PluginLocalRecursive=true

[Cache]
MaxSizeMiB=4096
PrefetchTracks=5

[Originals]
Mode=all
ExcludedIds=
```

`ExternalPath` may contain Windows environment variables and may be edited to another folder. A blank value uses the current Windows Music known folder. `Mode` is `all`, `selected`, or `none`; comma-separated `ExcludedIds` applies when mode is `selected`. Unknown exclusion IDs are retained and reported so a temporarily missing original remains excluded if it returns. The player uses a launch-seeded shuffle bag over eligible stable IDs and avoids an immediate repeat when at least two entries exist.

## Runtime state model

Publish an immutable catalog only after scanning, validation, and required conversion finish. The `LoadMusic` wrapper treats the game's requested numbered path as a scheduling trigger and chooses from the catalog; it does not use that number as a plugin track index. It forwards the selected original path or routes a selected custom/cache path through the scoped resolver hook.

Playback wrappers do no filesystem or conversion work. They forward game controls to the native engine and track only bounded state needed for selection. A failed custom load is quarantined for the remainder of the process and selection advances with a bounded number of attempts.

When the eligible catalog is empty because originals are disabled, enter an explicit silent state. The next spike must prove a low-overhead status policy that prevents the game's approximately 90 Hz scheduler from repeatedly calling `LoadMusic`, while allowing Stop, menu/republic transitions, and later state changes to work. This behavior is a release gate; it is not inferred from the tracer.

## Implementation sequence and release gates

1. **Decoder utility — implemented.** Miniaudio 0.11.25 is vendored at the pinned commit with its MIT-0 license. The standalone-tested converter uses wide Windows paths and converts WAV, FLAC, and MP3 to content/profile-keyed PCM16 stereo 48 kHz WAV cache files. Controlled fixtures and the current eight-file user matrix pass canonical-header, nonzero-frame, deterministic cache-hit, Unicode-path, resampling, and damaged-input rejection checks. Broader tag, CBR/VBR, and channel-layout fixtures remain acceptance work.
2. **Catalog/config library.** Implement known-folder resolution, plugin-DLL-relative source resolution, fresh recursive scanning, junction-loop avoidance, overlapping-root deduplication, stable IDs, INI validation, original discovery, modes/exclusions, cache keys, and atomic writes. Test with arbitrary names and counts, Unicode and paths longer than 255 bytes.
3. **Scheduler/filter spike.** Replace the one-shot probe with a small dynamic catalog. Demonstrate selected-original exclusion, all-original suppression, custom-only playback, one track, zero tracks, arbitrary custom counts, full track endings, and no high-frequency empty-catalog loop.
4. **External XWMA and WAV matrix.** Test plugin-local and Windows-Music paths, direct and short-cache copies, external XWMA, PCM16/float WAV, mono/stereo, 44.1/48 kHz, and malformed inputs. Publish support only for passing cases; convert every other decodable WAV variant.
5. **Lifecycle hardening.** Validate exact module hashes and every hook destination before activation. Keep wrappers safe in pass-through state when any prerequisite fails. Re-run focus loss, pause/resume, menu/republic, load/save, device-change, mute/volume, shutdown, and conflicting-hook tests.
6. **Packaging acceptance.** Install only plugin-owned DLL/data, verify original file hashes before and after, verify additions/removals on the next launch, then disable/remove the plugin and confirm vanilla music returns.

The first release remains pinned to the verified loader/game/engine combination. Supporting another build requires rerunning the hook, resolver-relocation, scheduler, and format acceptance checks.

Miniaudio capability sources: [project repository](https://github.com/mackron/miniaudio), [decoder and conversion documentation](https://miniaud.io/docs/manual/index.html), and [pinned 0.11.25 source](https://github.com/mackron/miniaudio/tree/9634bedb5b5a2ca38c1ee7108a9358a4e233f14d).
