# Red Wolf Radio architecture decision

Decision date: 2026-09-13. Scope: first supported release for the exact WRSR 1.1.1.9 executable/engine hashes and Republic Mod Loader 1.0.1 baseline recorded in `FEASIBILITY-PLAN.md`.

## October 10, 2026: 1.0 candidate additions

The current implementation retains the same hash-gated game audio engine and RML API4 compatibility host. Native RML API1 still does not provide a music or generic HUD notification service; TesmioLoader is neither installed nor required. The scoped resolver supplies validated custom XWMA to the existing LoadMusic/Play path. Miniaudio remains confined to WAV/FLAC/MP3 conversion.

Validate RIFF/XWMA size/chunk bounds, WMAudio2 format, compressed packet count, and nondecreasing cumulative decoded byte counts in `dpds`. Repeated final packet counts are valid: two of the 33 local game tracks contain padding packets with no additional PCM. Use short ASCII absolute source paths directly. Otherwise hash and atomically publish a byte-identical compressed `.xwma` copy to the existing bounded cache; never create a WAV for XWMA. Only plugin-owned prepared paths are touched or evicted. An ASCII DOS short-path alias may be used at the engine boundary; accounting keeps the actual owned path. Without a suitable alias, a non-ASCII/oversized cache route fails with a diagnostic.

The HUD uses the original source basename collected by FindFirstFileW, strips only its final extension, and starts a five-second wall-clock timer on the pending-to-playing transition after loading finishes. Deferred playback while user-paused has no notification until actual start. Draw wide, literal text through the engine's `PrintLeftUnicodeNoArg` API on the gameplay frame; sanitize control characters and shorten long names without splitting a UTF-16 surrogate pair. No cache filenames, audio-worker drawing, or varargs formatting of user filenames.

Reuse ForceNodes' published v1 frame service (`ForceNodes`, version 1, after-main phase) when present. Otherwise request RML's verified 14-byte, position-independent frame prefix at EXE RVA 0x2F0F10 only if still intact; unknown detours disable the optional HUD before a host hook request. Font manager 0x996FB0, font slot 0x994200, UI scale 0x992088 and screen dimensions 0x99528C/0x995274 are restricted to the pinned executable and checked at draw time. Native frame forwarding executes once and preserves its last error. These addresses/API were compared with [ForceNodes 1.8.0 source](https://github.com/Ultimate-Universe/WRSR-ForceNodes) and local binary disassembly; the new display remains unverified in live gameplay.

Microsoft documents [RIFF XWMA and dpds loading](https://learn.microsoft.com/en-us/windows/win32/xaudio2/how-to--load-audio-data-files-in-xaudio2) and the [WMA packet table](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/ns-xaudio2-xaudio2_buffer_wma). Do not equate our native game-loader route with XAudio2 2.8 decoding XWMA: [Microsoft lists XWMA decoding as absent in 2.8](https://learn.microsoft.com/en-us/windows/win32/xaudio2/xaudio2-redistributable), which the engine imports. The working original soundtrack demonstrates the game handles that format itself.

Russian is the first additional language, based on the game's active dedicated [Russian forum](https://steamcommunity.com/app/784150/discussions/2/), not a measured player-language share. INI comments, README, Workshop copy and a secondary JPEG guide are translated. Technical keys remain stable; Display.Language controls the HUD label only. New display settings default safely when absent from an older INI, and the existing file is not overwritten. Ukrainian can be added through the same configuration/documentation path after a translation review.

## Decision

Use Republic Mod Loader's built-in Tesmio API 4 compatibility host for the hook-dependent implementation. Keep the game's `C3DMusicXWMA` engine as the only playback backend. Embed miniaudio **0.11.25**, commit `9634bedb5b5a2ca38c1ee7108a9358a4e233f14d`, for offline decoding and conversion only; compile it with device I/O and the high-level engine disabled. FLAC and MP3 inputs are converted to PCM16 stereo 48 kHz WAV files in a plugin-owned cache before any music hook is enabled. Never pass FLAC or MP3 paths to the game engine.

Use the verified `LoadMusic`, `Play`, `Stop`, `Pause`, `Resume`, `SetVolume`, and `IsPlayingMusic` route. Preserve original XWMA playback through that route. Resolve absolute custom/cache paths through the verified scoped `C3DPath_GetFullPath` hook and relocated continuation. Do not replace game DLLs or edit original music files.

### Gameplay controls (September 2026 development build)

Read foreground key state on the existing game music-status thread and dispatch controls on that same thread. Use only the high bit of `GetAsyncKeyState`, with an edge detector and exact modifiers; reset held-key state after focus loss or a gap in status polling. No global shortcuts, extra game hooks, or audio operations on background threads are introduced. This seam runs in a loaded republic, not the main menu.

Disassembly of the hash-verified engine shows that its exported Pause/Resume functions only change volume. True user pause therefore calls `IXAudio2SourceVoice::Stop(0,0)` without flushing buffers; resume calls `Start(0,0)`. The verified XAudio2 2.8 source voice is stored at engine RVA `0x1dc7f0`, with Start and Stop at x64 vtable slots 19 and 20. Guard the loading flag and pointer reads, and remember voice identity before resuming. An independent silent XAudio2 integration test verified that the sample cursor remains unchanged while stopped and advances after restart. See [Microsoft's Stop contract](https://learn.microsoft.com/en-us/windows/win32/api/xaudio2/nf-xaudio2-ixaudio2sourcevoice-stop).

Next/Previous wait for an eligible prepared target before stopping the current stream and reporting its end to the game scheduler. Historical replay does not consume new shuffled entries. Keep at most 64 playback history entries in memory and retain the last `Cache.HistoryTracks` previous entries (default three) in the existing cache. A song selected while user-paused enters history but its native Play call is deferred until resume, preventing an audible start before the next status poll. The active track, upcoming window, retained history, and requested navigation target are all protected from eviction; no duplicate cache copies are required. Invalid hotkey settings disable only the affected controls, never original-track exclusions.

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

## Current-song menu investigation — October 11, 2026

Following successful 1.0 XWMA/display tests, the user reported that the temporary title is obscured by game menus at 1440×1080. Sangie's preferred first investigation is an on-demand current-song display in an existing game menu/window. Jess's movable player window is the fallback if there is no reasonably small native route. The user explicitly confirmed this order. Neither resolution presets nor a custom movable player has been implemented.

Verified locally:

- The built-in Script menu launches Pyrois scripts registered with `$OBJECT_SCRIPT`. Installed Timemaster item `2914450350` and Calculation Script item `3779822646` use this mechanism. A Workshop item's `WORKSHOP_ITEMTYPE_SCRIPT` classification alone does not register a menu action. The [script author's modding guide](https://github.com/chalwachalwa/Aerial-delivery---pyrois-script/wiki/Modding-guide) describes the same metadata.
- The installed game's `media_soviet/scripts/SOVIETInstructions.txt` exposes debug text windows (`Window_ShowText`, `Window_ShowCharArray`), scenario windows, existing-window inspection and position changes. Its published instructions do not provide a general custom window/button builder, live music-plugin state access, or an arbitrary DLL-call bridge.
- RML's inspected native `RmlHostApi` and API4 compatibility host expose no ready-made in-game settings page, draggable UI service or Script-menu callback registration. Adding a built-in script would require verifying how it communicates with Red Wolf Radio's DLL.
- Installed Republic Notepad item `3816212516` includes an interactive Dear ImGui panel implementation. Its source uses DirectX 11 Present/Resize, window-message and DirectInput hooks; it is evidence for an implementation approach, not a built-in game window API or proof of Red Wolf Radio compatibility. Its current panel explicitly disables movement. A movable radio panel would require its own lifecycle, input-capture and coexistence tests. Do not change loaders or infer a TesmioLoader dependency from that example's README.

### Concrete native boundary found

Static analysis of the verified `SOVIET64.exe` establishes that Pyrois opcode 1012 (`Window_ShowText`) calls the native dialog routine at RVA `0x6ee2a0`, from callsite RVA `0x5a044f`. Opcode 2007 (`Window_ShowCharArray`) reaches the same routine. The native routine takes UTF-16 text as its eighth argument and copies it into dialog-owned storage synchronously. `Window_ShowText` bounds its input to 250 UTF-16 code units. Its 14-byte entry prefix consists of complete instructions with no RIP-relative addressing, compatible in principle with the inspected RML API4 inline-hook mechanism. This is a private game function tied to the verified executable, not a documented RML menu service. Bounded disassembly and derivation are retained under `evidence/menu-hook-research/`.

The preferred proof is one fixed, plugin-owned Script action, **Red Wolf Radio: Now playing**. It calls `Window_ShowText` with a unique exact marker/fallback message. A narrowly scoped hook substitutes the current source filename and playback state only for that marker and the verified script caller/argument shape. All unrelated dialog calls retain every argument and return value. The native game then owns rendering, dismissal and input. No changes to original game files, graphics hooks or custom input capture are required for this proof.

An alternative is regenerating a status script whenever playback changes; the script author's debugging guide describes rerunning edited scripts without restarting. That avoids a native hook but adds unverified script reload/encoding behavior, filesystem writes and stale status when the plugin is absent. Retain it as an alternative rather than treating it as a proven live bridge.

### Smallest implementation and acceptance plan

1. Package a single one-shot script and its `$OBJECT_SCRIPT` registration in the local Development item. Its unmodified message should explain that song information is unavailable when the plugin is disabled, rather than show stale song data. Verify actual local Script-menu discovery before changing release packaging.
2. Add the optional marker-only bridge behind exact module hashes, instruction-prefix and caller checks. Check ownership before asking the host to install a hook: RML can treat rejected hooks as launch errors. Preserve the original continuation and all unrelated calls. Test forwarding and mismatch refusal before live installation.
3. Take a bounded snapshot under the existing state lock, release the lock before entering native UI code, and obtain the name from the active catalog entry. Do not use the five-second notification buffer or cache filename. Distinguish loading, paused, playing and no active track; honor English/Russian labels independently of `ShowTrackName`.
4. Handle the native renderer's format-string behavior before admitting arbitrary filenames. Both dialog types reach the wrapping helper at RVA `0x7aae80`, which passes assembled lines to variadic Unicode print functions. The engine's `FixVaListPercString` helper does not establish safe literal treatment of all format-specifier-shaped names. Do not pass raw `%` from filenames. Merely doubling it is not yet proven safe across wrapping; a diagnostic prototype can visibly substitute the full-width percent glyph while preserving the actual filename/catalog unchanged. Decide and validate the final display policy before release. Keep the complete message within 250 UTF-16 code units, preserve surrogate pairs, and test Cyrillic, percent signs, punctuation and long names.
5. Test open/close/reopen after Next and pause, with `ShowTrackName=false`, at 1440×1080 and the normal resolution. Verify other script dialogs, normal audio, shutdown/relaunch, and the plugin-disabled fallback. The first version is a snapshot refreshed on reopening, not a live player. Actual modal behavior, placement and readability remain gameplay questions.

Proceed to the movable player fallback only if the native snapshot cannot satisfy the simple display requirement without substantial additional hooks. That fallback may show live state, accept dragging and offer Previous/Play/Pause/Next; its actions must be queued for the music-control thread and its input capture must be tested separately. Movement, playback buttons and position persistence are not requirements for the first native-menu proof.

This investigation changed research documentation only. No menu feature is implemented or installed, no new DLL was built, and no live native-dialog acceptance is claimed.

### Prototype implementation — October 12, 2026

The user subsequently authorized implementation and local installation. `1.0.0-popup1` implements the guarded native-dialog snapshot and a self-contained companion Script action. `Display.NativeSongPopup` defaults to false and is enabled in the local test INI; the HUD is independently hidden for this test. The prototype uses ASCII `[percent]` for literal percent characters to avoid both format-string interpretation and uncertain full-width glyph coverage.

The native continuation is built from the verified 14-byte prefix plus an absolute indirect jump, protected RX, flushed and atomically published **before** calling RML's hook installer. This addresses the verified behavior that RML leaves the game thread running during installation. The host's returned continuation is not required for forwarding, and the private continuation lives for the process lifetime. Snapshot acquisition uses a nonblocking shared lock and bounded stack storage; all formatting and native UI calls occur after releasing it.

The full automated suite and independent review passed. Local installation and hash checks are recorded in `VALIDATION.md`; live menu discovery, script compilation and dialog usability remain unverified. The movable-player fallback remains deferred pending this test.

### Compact live native player — October 12, 2026

After the user accepted the snapshot prototype, they requested automatic refresh, a smaller window and four playback buttons. The implementation keeps the native Script action and game-owned window lifetime, but scopes three additional exact-prefix hooks to its frame drawing, type-1 body drawing and hit region. Stock kind-1 dimensions cannot be reduced through a supported field; changing body offsets alone would leave the old frame and click region.

The intercepted RWR marker gets a unique native duplicate reference (`0x525752`) and a cookie within the proven inline text allocation. Rendering only uses engine-supplied live window pointers; the music thread never touches a game window. All foreign windows forward to private continuations published before any host hook installation. Every instruction prefix and required engine export is checked first. The optional feature remains tied to the already verified executable/engine hashes.

Native panel and font primitives draw a compact frame. Literal text uses `PrintLeftUnicodeNoArg`, with width measurement through the corresponding NoArg font method, so percent-bearing names can display unchanged. Button actions enter a bounded nonblocking queue and are executed on the music thread. Explicit Restart and PreviousTrack actions preserve the older keyboard double-press semantics. Native input ownership and foreground checks govern clicks and dragging. Window position persistence is outside this prototype.

Automated and live acceptance results for this build are recorded separately in VALIDATION.md; implementation alone does not establish in-game correctness.

### Smaller icon player — October 13, 2026

`1.0.0-player2` changes presentation within the existing owned-window renderer. The frame is 360x128 UI units with a 26-unit title bar, two filename lines and four 44x28 control targets. Transport symbols are drawn with the verified solid-panel primitives, avoiding new texture files, font-glyph assumptions or native hooks. The title bar shows localized action names on hover. Existing command queue, input ownership, native window lifetime and playback behavior are retained. Layout acceptance is separate from the earlier text-button player's successful tests.
