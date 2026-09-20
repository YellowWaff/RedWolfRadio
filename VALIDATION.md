# Validation record

Tested environment: Workers & Resources: Soviet Republic 1.1.1.9, Republic Mod Loader 1.0.1, Windows x64. Both development builds below use the same verified executable, engine, and runtime hashes enforced by the plugin.

## Large-library playback — September 19, 2026

Build: `0.9.0-dev-bounded-cache`, DLL SHA-256 `5bed21c229ba85eec774e54a02056ec73cca572cfbd0b4ba36c48e1cecdd4234`.

- Scanned 186 custom audio files with original music disabled.
- Recorded 65 minutes 31 seconds in the first run and a 10 minute 38 second restart.
- Twelve completed gameplay tracks matched source WAV durations within 0.04–0.09 seconds.
- Restart reused cached audio and continued background preparation: two initial conversions, three cache hits, zero rejections.
- No recorded failed-load recoveries or dropped diagnostic events; the retained restart report confirms a normal exit and zero hook-audit issues.

The user confirmed normal exit from both runs. The first run's loader report was overwritten on restart, so its exit code was not retained. Every played song in this test was WAV; this test adds no MP3, FLAC, or external XWMA playback coverage. A separate controlled 256 MiB cache test verified eviction, reducing the cache from approximately 381.64 MiB to 246.50 MiB while protecting playback.

## Gameplay shortcuts — September 19, 2026

Build: `0.9.0-dev-hotkeys`, source implementation commit `dab4ac2`, DLL SHA-256 `c88df741d7a8425e554e7d5f7bc479011882831604ea92598dd54d52d7dd5d69`.

The user confirmed that Next, Previous, pause/resume, and navigation while paused behaved correctly. Three retained traces show:

- Successful native source-voice pause and resume, including a pause of approximately 28 seconds.
- Next and Previous selections while paused with deferred native Play calls; the chosen song starts only after a resume request.
- First Previous press restarts the current song. A quick second press selects earlier history.
- In the final run, eight backward moves reached the first gameplay history entry; additional presses restarted that entry.
- No recorded failed shortcut results or failed-load recoveries. The latest loader report confirms a normal exit and zero hook-audit issues.

`Cache.HistoryTracks=3` protects three previous entries from eviction. It is not a three-track navigation limit: up to 64 playback entries are remembered for the current launch, and an older evicted track can be prepared again. Three protected cache entries and a deeper navigation history are separate behaviors.

The automated suites cover configuration parsing, duplicate and disabled bindings, focus and held-key suppression, history retention, rapid Previous presses during loading, deferred playback, changed/missing voice handling, and recovery from failed historical entries. A silent test with the actual Windows XAudio2 2.8 interface confirmed that the sample cursor stopped advancing during pause and advanced after resume.

## Remaining coverage

- The shortcuts run in a loaded republic; main-menu control is not implemented.
- Arbitrary external XWMA files remain disabled pending compatibility validation. Original game XWMA playback was verified in earlier work.
- WAV, FLAC, and MP3 conversion have automated fixture coverage and earlier user playback tests; broader input variants remain useful release coverage.
- Hotkey remapping, disabled bindings, and focus gating have automated coverage and September 20 user-reported acceptance; no separate per-case trace review was performed for that report.
- Compatibility with other executable, engine, or RML builds is not assumed. A separate `temporary_track`/`road_traffic` startup conflict was isolated before these tests and was not a Red Wolf Radio failure.

## Source-directory regression and local Development installation — September 19, 2026

Build: `0.9.0-dev-source-paths`, DLL SHA-256 `4d71b9dee08b5dc52d86d98d0eddd2095b2a4f1f3a3f241342a29081119dcd8d`.

The full build and automated suites passed. A new isolated-process test runs an executable from a fixture game folder, loads the real built DLL from a separate package folder, and supplies misleading RML directory values and a loader working directory. It confirms:

- Original and plugin-local discovery follow the executable and DLL directories, respectively; decoy tracks under the loader are absent.
- Recursive arbitrary names, ignored artwork, WAV/FLAC/MP3 preparation, and a file added between catalog builds work.
- `all`, `selected`, and `none` apply to the discovered original catalog; logs expose every original ID and eligibility status.
- Read-only original fixtures retain their bytes and modification times, and test-created files stay within the isolated fixture tree.

The XWMA fixtures establish discovery and routing metadata only. Separate live game checks then verified the candidate from an independent local Development package:

- The Development panel visibly displayed Red Wolf Radio's name, approved Sangie thumbnail, one embedded DLL, and enabled state. The old loose DLL was archived with a non-DLL extension before activation. Each session loaded exactly one Red Wolf Radio instance with the candidate hash and nine successful hooks.
- **Local source / originals none:** resolved the package's own `plugin/Music` and the game's original Music folder. The catalog contained the generated local WAV plus 33 originals, with every original ineligible. The trace selected only the local WAV; three complete gameplay intervals measured approximately 11.99 seconds against its 12-second source duration. The session exited normally after two minutes.
- **Selected originals:** external and local sources were disabled, and 32 discovered originals were excluded. Only `original:music/track2.xwma` was eligible. Both menu and gameplay requests for excluded tracks were redirected to that permitted original; the game's playing status remained active during the short gameplay check. No new shortcut acceptance is claimed from this run.
- **All originals:** retained the same 32-ID list but changed mode to `all`. All 33 originals became eligible, including the listed entries, and the menu loaded and played `music/track3.xwma`. This was a startup/menu check, not another gameplay soak.

The live checks used normal RML diagnostics, not its deep monitor. Playback evidence comes from native call/status traces and loader reports; it is not an independent listening assessment. All sessions exited normally with no recorded failed-load recovery or dropped events.

Afterward, the user's original INI was restored byte-for-byte, the generated local WAV was removed, and all 33 original music files still matched their pre-work SHA-256 hashes. At that point the Development package remained enabled; its numeric identifier was local-only, not a registered Steam Workshop item. The panel's metadata button opens `workshopconfig.ini`; music settings remain in `plugin/RedWolfRadio.ini`. The later subscribed installation checks are recorded below.

## Targeted configuration acceptance — September 20, 2026

The user reported that step 3 worked as expected. This is user-reported acceptance of the listed portable/default source paths, missing/empty and rejected-input cases, one-track playback, ignored artwork, mixed WAV/MP3/FLAC playback, and configurable controls/focus checks. No fresh per-case logs or timing measurements were reviewed for this report. The local Development candidate remains the source-path build described above.

This report does not establish settings preservation through a Steam Workshop update or RML force-refresh. The later subscribed installation checks are recorded below; automatic update preservation remains unverified.

## Registered Workshop project staging — September 20, 2026

The user created item `3805202523` through the game. Its authoring folder was populated with 14 files from the rebuilt Workshop package while preserving the game-generated `workshopconfig.ini` byte-for-byte. An independent review checked every packaged file hash, the source-path candidate DLL hash, portable defaults, artwork and license notices. No personal audio, cache, traces, settings backups, or username paths were included in the payload. The user's current settings were backed up outside the authoring folder.

RML showed both local Development projects disabled. WRSR launched normally for authoring inspection and displayed the new item as Script, approximately 4 MB, and **Unpublished**; the Edit Item screen retained that visibility and showed no validation error. No save/upload action was submitted in this check. The already-created Steam copy still contained only the initial metadata and preview when inspected. DLL upload and subscribed runtime acceptance were not established by this check.

Later that day, after the user signed into the browser, the live Steam item page showed **Current visibility: Hidden**, file size **4.988 MB**, an update at 12:19 PM, and two change notes. This confirmed the listing's non-public visibility and a larger uploaded item. The page still offered Subscribe, and the local download inspected at that time contained only the earlier metadata and preview. The subscribed payload was verified in the subsequent check below.

## Default Play/Pause shortcut adjustment — September 20, 2026

The user reported that `Ctrl+Shift+Space` also paused WRSR and requested `Ctrl+Shift+Down` instead. The current default INI, generated INI, initial configuration and absent-key fallback now use the Down arrow. Next and Previous are unchanged, and existing INIs keep their configured value unless edited.

The full build and existing automated suites passed, including the default-binding and music-control fixtures. The rebuilt `0.9.0-dev-source-paths` DLL has SHA-256 `5bbe5c9c486bbbcaea9e95dc82556fbb27d9c45799a17b63b1d11ea758253622`. This differs from the earlier source-path candidate solely because the default shortcut was adjusted; new in-game user acceptance of the Down-arrow binding is pending.

## Subscribed Workshop payload and launcher integration — September 20, 2026

The current staged item was uploaded through the in-game green checkmark. Both the success dialog and Steam's 14:27:42 upload log confirmed completion, producing manifest `5000316681469668187`. The signed-in listing showed **Hidden**, **4.990 MB**, a 14:27 update, and three change notes. Republic Mod Loader item `3787969749` was added as a required item, and the approved explainer graphic was present in the gallery.

After subscription, the ordinary automatic download had not supplied the updated payload. A temporary helper requested only `SteamUGC::DownloadItem(3805202523, true)`; it received the matching success callback (`result=1`) and exited with code 0. This was an assisted Steam download, not RML's destructive force-update procedure, and does not establish that automatic downloading works without intervention.

- Before restoring personal settings, all 15 downloaded files matched the authoring folder byte-for-byte, including the DLL SHA-256 `5bbe5c9c486bbbcaea9e95dc82556fbb27d9c45799a17b63b1d11ea758253622`. Local evidence: `evidence/workshop-subscription-20260920/downloaded-hashes.json`.
- The previous local project's personal INI was restored beside the subscribed DLL, with SHA-256 `527b7b93a0d4d31e0177472962498d4768b9f1fb5d215ab9eba8740255fcd2fb`. Its external backup and the downloaded default INI were retained separately.
- Both Development copies remained disabled, and no active loose Red Wolf Radio DLL remained in RML's shared plugin directory.
- RML's Plugins panel visibly showed the enabled plugin associated with Workshop item `3805202523` and the approved Sangie preview. **Open plugin INI Settings** opened the exact subscribed `plugin/RedWolfRadio.ini` in Notepad++, confirmed by its command line.

At 14:36:34, the subscribed candidate launched through RML and reached the visible main menu. The retained startup log contains exactly one queued Red Wolf Radio instance, from item `3805202523`, with the verified candidate hash; both Development copies were explicitly disabled. RML accepted API 4 and all nine hooks succeeded. The catalog contained 61 tracks, including 33 originals in `all` mode, and resolved the configured external test folder. Startup recorded one conversion, one cache hit, zero rejected inputs, and the Down-arrow Play/Pause binding (`40/3`). No configuration-malformed report or matching ERROR/WARN entry was found in this startup review. Evidence: `evidence/workshop-subscription-20260920/runtime-startup.log`.

These checks establish payload delivery, manual settings migration, artwork, Workshop association, settings-button routing, and startup from the subscribed DLL. The game was left at the main menu for user gameplay acceptance; this check does not claim audible playback, working controls, or a normal exit. Gameplay acceptance of `Ctrl+Shift+Down`, ordinary Steam update preservation, and a subscribed-plugin disabling test remain pending.
