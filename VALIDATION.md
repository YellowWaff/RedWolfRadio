# Validation record

Tested environment: Workers & Resources: Soviet Republic 1.1.1.9, Republic Mod Loader 1.0.1, Windows x64. Both development builds below use the same verified executable, engine, and runtime hashes enforced by the plugin.

## Native popup prototype — October 12, 2026

Built and installed **1.0.0-popup1** into local Development item `900001`. DLL SHA-256: `091097e68f1397ed619f6179be1c3be524253630bc899a0a8d7e80d7ac1960a3`. The game, engine and RML runtime hashes were rechecked against the existing baseline before installation. The game was closed; no game session was launched during this installation.

The full `build.ps1` suite passed, including the new `native_song_popup` suite. Its tests exercise the real private executable continuation during a fake host's installation callback, proving forwarding is available before the host returns. They also check the nine-argument ABI, exact caller/marker matching, unchanged unrelated calls and return/last-error values, unknown-owner/callsite refusal before hook requests, source-name snapshots independent of the HUD/cache, busy/empty/loading/paused/playing states, English/Russian labels, percent/control sanitization and 250-unit UTF-16 bounds across surrogate boundaries. Independent code review found the initial hook-publication race; the reviewed fix publishes the private RX continuation atomically before installation. Automated tests do not establish actual game dialog behavior or Pyrois compilation.

Installation added one self-contained script and its icon/metadata registration, **Red Wolf Radio: Now playing**, and replaced only the local Development DLL and INI. The INI has `NativeSongPopup=true`, `ShowTrackName=false`, `Language=en`; the two-track external XWMA test path and `Mode=none` are preserved. RML's existing Default selection remains unchanged and enables only the local radio copy. Installed file hashes were read back against the manifest, and RML configuration plus subscribed public DLL/INI/metadata were confirmed unchanged.

The popup is a snapshot: fully close it before reopening to refresh. This diagnostic build displays ASCII `%` as `[percent]`, without altering source filenames. The script's fallback message appears when the bridge is unavailable. Placement, Cyrillic glyph appearance, current-track/state accuracy in the actual dialog, modal behavior, other-dialog coexistence and normal relaunch remain user acceptance gates. No movable-player functionality is claimed.

Backups and a hash-checked restore script are in `evidence/install-native-popup-20261012-193449-364/`. The reproducible script and focused acceptance steps are under `prototype/native-popup/`. The prototype has not been published or included in the release packaging flow.

## 1.0.0-rc1 local candidate — October 10, 2026

DLL SHA-256: `6fb31638ec14ad15c572118a88629c38379e351e8676ab4b5461d6d631ee2bcb`.

The full build passes the existing converter, forwarding/scheduler, hotkey, voice-control, music-control and isolated source-path suites. New native/display acceptance covers unchanged direct XWMA, compressed Unicode-path copies, digest-checked cache reuse and repair, malformed chunk/packet rejection, nondecreasing padding packets, source timestamp preservation, owned-cache enumeration, Unicode/percent filenames, and actual-start notification lifecycle. Rendering tests use fake game resources and verify the optional ForceNodes callback, native fallback, unknown-owner refusal, five-second expiry, editable display defaults and last-error preservation. These are automated checks, not live render/audio acceptance.

The XWMA validator also inspected all **33** files in the installed game's Music folder read-only: **33 accepted, 0 rejected**. Two files repeat the last `dpds` count for a padding packet; the validator was corrected accordingly. This corpus is WMAudio2 stereo/48 kHz. No audio fixtures from the game or another Workshop author are redistributed. No other music Workshop item with XWMA was present locally for a broader corpus test.

Current installed binary hashes were rechecked and still match the enforced baseline:

- EXE: `296644a9f207d609031fc2ae73fed2dcb34619a1d55a35d1c7b51965ce6841b8`.
- Engine: `65fcb6845c3cb7c25a22121f25904a855ea88657e31a3b50a15adac26e231516`.
- RML runtime: `761ca3aee81c77034059258310e5eb4e3df14f94d21d030a96b9665879fd19d0`.

API research used RML revision `57e567e5cccaa3c38aee7e9d599d2443d2460b10` and ForceNodes revision `7cd8c8e82ace5f7ac08ac28b7975ba19115cdef2` (version 1.8.0), plus the matching local game disassembly. RML API4 config reads raw UTF-8 and skips bilingual comments. ForceNodes publishes its frame service in Init, before Start, making optional callback registration available to this plugin. A standalone fallback requires the verified untouched 14-byte frame prefix.

English/Russian JPEG guides were visually inspected for wording, layout, black body text and square arrow keycaps. English: 635,833 bytes; Russian: 662,842 bytes, both 1536×1024 and below 2 MB. The Russian guide's large title and game-name subtitle are translated. Both upper-left character logos were restored using the original high-resolution artwork as reference to remove faceted shading. Independent visual review found improved logo detail with no text regressions. Translated INI help, README and Workshop descriptions are prepared locally. The public description and subscribed DLL were not changed. A native-speaker copy review remains useful before publication.

**Outstanding after the October 11 tests below:** resolve HUD positioning/visibility at smaller resolutions, representative other-Workshop XWMA files, and a 1.0 mixed-library regression. The direct/copied routes, visible ASCII/Cyrillic filenames, natural transitions, current plugin coexistence, latest normal exit, five-second expiry, Russian HUD-label/display-toggle settings, pause/resume and music-volume controls are now covered by logs and/or the user acceptance described below. See [TESTING-1.0.md](TESTING-1.0.md). Final 1.0 publication is gated on the remaining user acceptance.

## 1.0.0-rc1 external XWMA and HUD first pass — October 11, 2026

Installed into local Development item `900001`; the subscribed and separate authoring copies stayed disabled. The latest runtime log loaded exactly one Red Wolf Radio instance with the candidate hash above. The user reported hearing both songs, seeing the Russian filename and `RWR TEST 100%`, natural track endings/transitions, and working Next/Previous controls. These two fixtures are unchanged copies of game music in a separate external folder; familiar vanilla audio is expected and does not indicate the original-source playlist was enabled.

- Runtime preparation recorded `direct native XWMA` for the ASCII/percent filename and `unchanged XWMA cache copy` for the Cyrillic filename in a subfolder. Originals mode was `none`, with all 33 originals ineligible.
- Startup recorded 2 native custom XWMA tracks, 0 WAV conversions, 1 compressed cache hit and 0 rejected inputs. Compared with the pre-install inventory, the shared cache gained only one 2,819,478-byte XWMA, byte-identical by SHA-256 to the Cyrillic source; no WAV entries were added or modified. Both original game files and their external test copies still match their installation hashes.
- The latest trace completed the Cyrillic track in approximately 116.278 seconds (expected 116.267) and the ASCII track in 111.962 seconds (expected 111.957), then selected the Cyrillic track again. Later rapid changes correspond to applied hotkeys.
- Next was applied four times and Previous twice in the latest trace. The first Previous restarted the current track; another 0.861 seconds later selected the prior track. Across all three retained sessions there were no failed-load timeout events or dropped diagnostic events; the middle session's 25 rapid changes were applied Next commands.
- The HUD registered with the ForceNodes frame service while the user's other enabled plugins loaded. Actual name visibility is user-reported; logs do not establish visual placement, five-second disappearance or glyph quality.
- Three separate traces establish relaunch use. The latest session exited normally with code `0x0` after 4m26.972s. Earlier exit reports were overwritten. No Red Wolf Radio errors were found in the retained latest runtime log.

Pause/resume and deliberate music-volume adjustment were not established in these traces. The INI remained `ShowTrackName=true`, `Language=en`: a Cyrillic source filename does not establish the Russian HUD-label setting. No settings were changed during this review. Evidence is retained locally in `evidence/acceptance-1.0-xwma-20261011-183952/` (three traces, runtime/session logs, runtime report, INI and cache comparison).

### Follow-up display and playback acceptance — October 11, 2026

The user subsequently confirmed five-second title disappearance, pause/resume, game music-volume adjustment, the Russian HUD label with `Language=ru`, and hiding titles with `ShowTrackName=false`. These follow-up results are user-reported; no new per-setting log or screenshot verification is claimed.

The user also reported a positioning defect at 1440×1080 and smaller resolutions: menus/bars obscure most of the title. The current renderer uses an X offset of 20 UI-scaled pixels and Y at 78% of viewport height. It reads live viewport dimensions and UI scale but does not avoid menu rectangles. The user subsequently prioritized a simpler on-demand current-song menu idea before the movable player fallback; the earlier positioning-settings recommendation is superseded. Static inspection found a concrete Script-menu/native text-dialog boundary at `SOVIET64.exe+0x6ee2a0`, with synchronous UTF-16 copying, but no menu feature is implemented or live-tested yet. See `ARCHITECTURE-DECISION.md` and `evidence/menu-hook-research/` for the bounded prototype plan and unresolved rendering/input behavior. Keep the smaller-resolution display issue open before release.

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
- The public 0.9.0 build rejects external XWMA. The 1.0.0-rc1 candidate enables validated WMAudio2; audible external playback still needs acceptance. Original game XWMA playback was verified in earlier work.
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

- The Development panel visibly displayed Red Wolf Radio's name, approved character thumbnail, one embedded DLL, and enabled state. The old loose DLL was archived with a non-DLL extension before activation. Each session loaded exactly one Red Wolf Radio instance with the candidate hash and nine successful hooks.
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

The full build and existing automated suites passed, including the default-binding and music-control fixtures. The rebuilt `0.9.0-dev-source-paths` DLL has SHA-256 `5bbe5c9c486bbbcaea9e95dc82556fbb27d9c45799a17b63b1d11ea758253622`. This differs from the earlier source-path candidate solely because the default shortcut was adjusted. The subsequent subscribed-package gameplay acceptance below includes the Down-arrow binding.

## Subscribed Workshop payload and launcher integration — September 20, 2026

The current staged item was uploaded through the in-game green checkmark. Both the success dialog and Steam's 14:27:42 upload log confirmed completion, producing manifest `5000316681469668187`. The signed-in listing showed **Hidden**, **4.990 MB**, a 14:27 update, and three change notes. Republic Mod Loader item `3787969749` was added as a required item, and the approved explainer graphic was present in the gallery.

After subscription, the ordinary automatic download had not supplied the updated payload. A temporary helper requested only `SteamUGC::DownloadItem(3805202523, true)`; it received the matching success callback (`result=1`) and exited with code 0. This was an assisted Steam download, not RML's destructive force-update procedure, and does not establish that automatic downloading works without intervention.

- Before restoring personal settings, all 15 downloaded files matched the authoring folder byte-for-byte, including the DLL SHA-256 `5bbe5c9c486bbbcaea9e95dc82556fbb27d9c45799a17b63b1d11ea758253622`. Local evidence: `evidence/workshop-subscription-20260920/downloaded-hashes.json`.
- The previous local project's personal INI was restored beside the subscribed DLL, with SHA-256 `527b7b93a0d4d31e0177472962498d4768b9f1fb5d215ab9eba8740255fcd2fb`. Its external backup and the downloaded default INI were retained separately.
- Both Development copies remained disabled, and no active loose Red Wolf Radio DLL remained in RML's shared plugin directory.
- RML's Plugins panel visibly showed the enabled plugin associated with Workshop item `3805202523` and the approved character preview. **Open plugin INI Settings** opened the exact subscribed `plugin/RedWolfRadio.ini` in Notepad++, confirmed by its command line.

At 14:36:34, the subscribed candidate launched through RML and reached the visible main menu. The retained startup log contains exactly one queued Red Wolf Radio instance, from item `3805202523`, with the verified candidate hash; both Development copies were explicitly disabled. RML accepted API 4 and all nine hooks succeeded. The catalog contained 61 tracks, including 33 originals in `all` mode, and resolved the configured external test folder. Startup recorded one conversion, one cache hit, zero rejected inputs, and the Down-arrow Play/Pause binding (`40/3`). No configuration-malformed report or matching ERROR/WARN entry was found in this startup review. Evidence: `evidence/workshop-subscription-20260920/runtime-startup.log`.

These checks establish payload delivery, manual settings migration, artwork, Workshop association, settings-button routing, and startup from the subscribed DLL. The game was left at the main menu for the later user gameplay acceptance recorded below; this startup check alone does not establish audible playback, working controls, or a normal exit. Ordinary Steam update preservation and a subscribed-plugin disabling test remain pending.

## Subscribed Workshop playback and relaunch acceptance — September 20, 2026

Build: `0.9.0-dev-source-paths`, DLL SHA-256 `5bbe5c9c486bbbcaea9e95dc82556fbb27d9c45799a17b63b1d11ea758253622`, subscribed item `3805202523`.

The user confirmed that the subscribed-package test succeeded: all keyboard shortcuts worked with a mixture of original game music and personal tracks, and normal exit followed by relaunch worked. This accepts the revised `Ctrl+Shift+Down` pause/resume shortcut in gameplay.

The retained traces under the subscribed package's `plugin/RedWolfRadio-logs` corroborate the playback and control activity. Filenames use UTC; the sessions occurred on September 20 in the user's local time.

| Trace | Time to final completed Stop | Original selections | External selections | Applied Next / Previous / Pause-or-resume events |
| --- | --- | --- | --- | --- |
| `trace-20260921-000450-57860.tsv` | 15 minutes 25 seconds | 12 | 2 | 3 / 4 / 4 |
| `trace-20260921-002709-42928.tsv` | 1 minute 24 seconds | 14 | 12 | 8 / 16 / 8 |

- Both traces contain zero failed or ignored shortcut results, zero failed-load timeout recoveries, and zero dropped diagnostic events. Selection counts include restarts and navigation; they are not counts of unique or fully completed songs.
- The latest trace includes track selection while user-paused, deferred native Play calls, and successful resume of the selected track. Both traces end with a completed native Stop call. The earlier startup trace, `trace-20260920-193634-64720.tsv`, also ends with a completed Stop and records no failed-load timeout or dropped events.
- The latest RML runtime report contains exactly one Red Wolf Radio entry with the expected DLL hash, API 4, nine successful hooks, and zero hook failures. The current `rml-runtime.log` has no warning, error, or fatal entries.
- At **19:28:34.480 local time**, `rml-runtime.log` records WRSR PID **42928** exiting with code **0x0** after **1 minute 25.628 seconds**. `rml-session.log` confirms normal completion and zero final hook-chain audit issues. The previous session's exit code was overwritten on relaunch; its normal exit is user-reported and its final Stop is retained in the trace.

The user also reported that Steam continued to display the game as running after the game and launcher windows closed. The diagnosis and successful session cleanup are recorded below. This acceptance does not establish automatic Workshop update/settings preservation or the behavior with the subscribed plugin disabled; those checks remain pending.

## Steam running-state diagnosis — September 20, 2026

WRSR PID `42928` exited normally at 19:28:34.480, and neither `SOVIET64.exe` nor `RepublicModLoader.exe` was present during the subsequent process inspection. Steam nevertheless still showed **Stop**.

- Notepad++ PID `69920` remained alive from the earlier RML settings-button check at 14:32:21. Its parent was the earlier RML PID `56888`, and its command line opened the subscribed `plugin/RedWolfRadio.ini`.
- A surviving Steam `gameoverlayui64.exe` PID `74000` explicitly targeted editor PID `69920` with `-gameid 784150`. This established that Steam's WRSR overlay had attached to the settings editor.
- The editor showed no modified tabs, with Save and Save All disabled. It was exited normally through its UI; no processes were force-terminated and no save/discard prompt was accepted. Once the editor had exited, no WRSR, RML, Notepad++, or game-overlay process remained in the focused process check.
- At **19:39:46**, Steam's `gameprocess_log.txt` recorded both latest game/loader PIDs as no longer tracked with exit code 0, followed by `Remove 784150 from running list`. The client visibly returned to the green **Play** button without restarting Steam or pressing its Stop button.
- The subscribed INI retained SHA-256 `527b7b93a0d4d31e0177472962498d4768b9f1fb5d215ab9eba8740255fcd2fb` before and after cleanup.

This session's lingering status cleared when the RML-opened editor exited. The exact internal Steam tracking mechanism was not instrumented, and no change to Red Wolf Radio's shutdown code is justified by these results. The documented workaround is to save and fully exit editors opened through RML's settings button when finishing a session. Local diagnostic evidence and copies of the three playback traces are retained under ignored `evidence/steam-running-20260920/`; they are not part of the public package.

## Subscribed-plugin disabled launch — September 20, 2026

The subscribed Red Wolf Radio entry was disabled in RML while both Development copies remained disabled. RML's dashboard changed from 14 to 13 enabled plugins, and its runtime log explicitly recorded the subscribed DLL as `[PLUGIN DISABLED]`.

WRSR 1.1.1.9 reached the visible main menu through RML 1.0.1 without loading Red Wolf Radio. The session lasted 45 seconds, exited normally with code `0x0`, and reported zero final hook-chain audit issues. Red Wolf Radio was re-enabled afterward. The retained logs are under ignored `evidence/release-0.9.0-20260920/` and are not included in release packages.

## Ordinary Workshop update and settings preservation — September 20, 2026

The 0.9.0 payload was uploaded through WRSR's green checkmark at 20:09:32 local time. Steam recorded manifest `1652410860917990531`, and the signed-in listing refreshed to version 0.9.0, 4.913 MB, four change notes, and the GitHub source link while remaining **Hidden**.

Before the update, the subscribed installation contained the earlier DLL SHA-256 `5bbe5c9c486bbbcaea9e95dc82556fbb27d9c45799a17b63b1d11ea758253622` and the user's personalized 1,012-byte INI SHA-256 `527b7b93a0d4d31e0177472962498d4768b9f1fb5d215ab9eba8740255fcd2fb`. The INI included the selected external music folder and the accepted `Ctrl+Shift+Down` pause binding.

WRSR and RML closed normally. Steam initially showed no queued download, so the client was exited and relaunched through its normal UI. Its Workshop log then recorded detection of the new manifest, `Starting Workshop download job ( updating )`, and `Finished Workshop download job : No Error`. This path did not use RML force-refresh or the targeted download helper.

After the update, the installed DLL matched the 0.9.0 release SHA-256 `22e3584d59c23a738e767e5614dd86feb4c934e9c8f2d18b2d944b854477d53b`. The personalized INI retained its original length, timestamp, contents, and SHA-256. This establishes settings preservation for an ordinary Steam Workshop incremental update. Backups remain advised for RML force-refresh, unsubscribe/resubscribe, and manual replacement because those operations may recreate the item folder.

## Public release — September 20, 2026

The final Workshop payload was submitted through WRSR's green checkmark. Steam recorded manifest `6687241742565758771` and `Upload finished for workshop item 3805202523 : OK` at 20:22:18 local time. The refreshed listing showed version 0.9.0, the final settings-preservation wording, 4.916 MB, and five change notes before its visibility was changed from **Hidden** to **Public**.

Commit `37a1298b3ffc9535836dcaec6521866264d5170a` and annotated tag `v0.9.0` were pushed to the public GitHub repository. The GitHub release is marked latest and contains `RedWolfRadio-0.9.0.zip` plus `RedWolfRadio-Workshop-0.9.0.zip`; its displayed SHA-256 values match the locally verified archives. An unauthenticated GitHub read confirmed that the repository is public. The Workshop page and GitHub release link to each other.

## Native current-song popup prototype acceptance — October 12, 2026

Build: `1.0.0-popup1`, local Development item `900001`, installed DLL SHA-256 `091097e68f1397ed619f6179be1c3be524253630bc899a0a8d7e80d7ac1960a3`.

The user reported completing the popup test instructions and confirmed that the prototype works. Those instructions covered opening the native Script-menu window, closing/reopening it after song and playback-state changes, the Cyrillic and percent-containing test names, the smaller-resolution check, ordinary windows, and relaunch. Visual correctness, window size/position, dragging and per-step outcomes are user-reported rather than independently established by the logs below. The user then requested one window that refreshes automatically, less unused height, and optional Play/Pause, Next, Beginning and Previous controls. The accepted `popup1` is a snapshot window and does not provide those refinements.

Two retained `popup1` traces were captured from `plugin/RedWolfRadio-logs`. Their UTC filenames fall on October 13; both sessions occurred on October 12 in America/Chicago:

| Trace | Time to final completed Stop | Applied Next / Previous / Pause-or-resume | Deferred Play while paused |
| --- | --- | --- | --- |
| `trace-20261013-004811-32620.tsv` | 3 minutes 43.128 seconds | 4 / 0 / 1 | 1 |
| `trace-20261013-005555-10008.tsv` | 47.767 seconds | 3 / 0 / 0 | 0 |

- Both traces identify `1.0.0-popup1`, contain the two external XWMA selections, end with a completed native Stop, and record zero failed/ignored controls, failed-load timeouts or nonzero dropped-event counts. The longer trace establishes a successful pause and selection of another track while paused; it does not contain a resume or Previous command. These short sessions do not add full-track natural-transition coverage.
- The latest runtime report contains exactly one Red Wolf Radio plugin instance with the installed hash above, API 4, ten successful hooks and zero hook failures. Its native popup hook is active at `SOVIET64.exe+0x6EE2A0`; the latest runtime log records one popup snapshot request at 19:56:28.776 local time. The preceding session's runtime log was overwritten, so no total popup-request count is claimed across both sessions.
- The latest runtime log records PID `10008` exiting normally with code `0x0` after 49.427 seconds at 19:56:44.904 local time. The session summary reports zero final hook-chain audit issues. No Red Wolf Radio error was found; an unrelated helicopter-plugin informational warning about its native menu classification mask remains in the log.
- The captured INI has `NativeSongPopup=true`, `ShowTrackName=false`, `Language=en`, the external XWMA test folder, originals mode `none`, and cache history protection set to three tracks. Runtime preparation reports direct ASCII-path XWMA plus the unchanged compressed Cyrillic-path cache copy, zero WAV conversions, one cache hit and zero rejected inputs.

Evidence is retained locally under ignored `evidence/acceptance-native-popup-20261012/`: both traces, current runtime/session logs, runtime report, INI, source/copy hash manifest and parsed trace summary. No installed files were changed during this acceptance review. Automatic refresh, compact height, UI buttons and any changes to native-window lifetime remain untested until implemented.

## Compact live player implementation — 2026-10-12

Candidate `1.0.0-player1`, DLL SHA256 `1987FEF7353B4715CB104E4B923ADA86924358F4FB00D8714C6E0729B2FEA670`.

- Full `build.ps1` suite passed: converters, forwarding, hotkeys, voice control, music commands/history, native XWMA/display, popup adapters, compact native UI, and source discovery/preservation.
- Native UI tests exercise ownership/foreign forwarding, all four commands, one-shot click consumption, covered-window/input gates, dragged mouse capture, viewport changes, close/minimized geometry, literal Unicode/percent measurement/printing, snapshot throttling and panel state restoration. Integration probes execute each verified private continuation prologue and confirm ABI/last-error preservation before hook publication.
- Local installation verified identical DLL bytes at development item `900001`; rollback in `evidence/install-native-popup-20261012-201614-071`. Existing source folder, disabled HUD, enabled native player, RML configuration and public subscribed files preserved.
- Startup succeeded: RML reports the live compact player and all three additional frame/body/hit hooks installed. Further gameplay acceptance is recorded below after UI testing.

## Compact live player acceptance — October 12, 2026

Build: `1.0.0-player1`, local Development item `900001`, installed DLL SHA-256 `1987fef7353b4715cb104e4b923ada86924358f4fb00d8714c6e0729b2fea670`.

The user confirmed that the compact player moves normally, updates the song title automatically, keeps only one window when opened repeatedly, and can be closed and reopened without interrupting music. The user also accepted the `1440x1080` layout and confirmed that another native window covering the player blocks its controls. The window returns to the center when reopened; position persistence is not implemented. These visual, interaction and audible outcomes are user-reported, rather than inferred from the playback traces. Earlier live checks exercised the four buttons, paused navigation, literal percent text and Cyrillic names.

Three completed `player1` traces corroborate playback and control behavior. Filenames are UTC; the sessions took place October 12 in America/Chicago:

| Trace | Time to final completed Stop | Selections | Applied Next / Previous-track / Restart / Play-pause |
| --- | --- | --- | --- |
| `trace-20261013-011621-30352.tsv` | 9 minutes 56.323 seconds | 13 | 3 / 2 / 3 / 4 |
| `trace-20261013-012647-19316.tsv` | 1 minute 26.108 seconds | 5 | 1 / 1 / 1 / 2 |
| `trace-20261013-013021-35652.tsv` | 1 minute 8.517 seconds | 2 | 0 / 0 / 0 / 0 |

- All three traces identify `1.0.0-player1`, end in a completed native Stop, and record zero failed/ignored controls, failed-load timeouts or nonzero dropped-event counts. Selections include restarts and navigation, and are not unique-song counts. `Previous-track` is the button's direct history action; the keyboard Previous shortcut is unchanged.
- The first trace records two deferred native Play calls while user-paused, followed by successful resume of the selected track. It also contains three uninterrupted natural transitions: `RWR Test 100%` after 111.974 seconds, the Cyrillic fixture after 116.275 seconds, and `RWR Test 100%` after another 111.977 seconds. No control or Stop call interrupts those three playback intervals.
- The latest runtime report contains exactly one Red Wolf Radio instance, the expected hash, API 4, thirteen successful hooks and zero hook failures. The compact frame/body/hit hooks and scoped song-window adapter are active. Bootstrap completed with status zero.
- The latest runtime log records three native-window requests. The user's one-window and close/reopen confirmations establish their visible behavior; request counts alone do not establish window lifetime or duplication.
- At **20:31:31.493 local time**, RML records WRSR PID **35652** exiting normally with code **0x0** after **1 minute 12.193 seconds**. The session summary reports zero final hook-chain audit issues. The earlier runtime reports were overwritten on relaunch, so their exit codes are not claimed from the current report; both earlier traces retain completed Stop calls.
- No WARN/ERROR/FATAL severity entry was found in the latest runtime log. The unrelated helicopter plugin still logs an INFO-level warning about an unavailable native menu classification mask. No Red Wolf Radio error was found.
- The captured configuration retains the XWMA fixture folder, originals mode `none`, `NativeSongPopup=true`, `ShowTrackName=false` and `Language=en`. Latest startup reports two native custom XWMA files, zero WAV conversions, one cache hit, zero rejected inputs and a well-formed configuration.

Evidence is retained locally under ignored `evidence/acceptance-native-player-20261012/`: three completed traces, current runtime/session logs, runtime report, INI, matching source/copy hash manifest, parsed trace summary and evidence notes. Earlier files explicitly marked `live` are partial snapshots. This review changed no installed files or public package. The compact player is accepted locally; final package installation remains a separate release check. Another long soak is optional unless further playback or UI changes justify one.

## Release candidate preparation — October 12, 2026

Build `1.0.0-rc2`, DLL SHA-256 `CEF906B6047E1F8D451B41443F63951D8D91D4D07EC0C165559CEB104377ACF6`.

- Full `build.ps1` suite passed after adapting the optional HUD lifecycle test to explicitly enable the notification. The new missing-key defaults enable the native player and disable the transient HUD; tests also verify preservation of explicit true/false settings. Playback and native player implementations are unchanged from accepted `player1`.
- English/Russian instructions and listing drafts now explain the compact player, its Script-menu entry, all four buttons and the distinct keyboard Previous behavior. Both updated JPEG guides were visually reviewed and are below Steam's 2 MB limit (493,450 bytes English; 520,848 bytes Russian).
- Packaging now includes the companion script, matching menu icon, and a single metadata registration fragment in both layouts. The fragment is merged into existing registered metadata rather than replacing the item ID or visibility. Runtime-file checksums and embedded-version checks guard the staged artifacts.
- The accepted `player1` DLL remains installed in local Development item `900001`; its hash was rechecked unchanged. This rc2 binary has passed automated tests but has not yet received an in-game package smoke test. No public Workshop or GitHub update was submitted.

Build and package evidence is retained under ignored `evidence/release-1.0.0-rc2/`.

## Small icon player implementation — October 13, 2026

Local build `1.0.0-player2`, DLL SHA-256 `0B566841F4C88853E8F81BABE5C425A9FE8C34D2F9F234A40DCC06754608FABA`.

- Reduces the frame from 500x180 to 360x128 UI units (48.8% less area), with a 26-unit draggable header and two filename lines. Four 44x28 targets contain geometric Previous, Restart, Play/Pause and Next icons. Hover descriptions replace the title inside the existing header, in English or Russian.
- Uses the existing verified rectangle/white-texture drawing primitives. Fixed 16-row triangles bound icon drawing work regardless of scale. No additional hooks, imports, textures or symbol-font assumptions were introduced; playback/control implementation is unchanged.
- Full automated build suite passed. Updated UI checks cover compact frame and usable targets, literal filename refresh, English/Russian hover captions, paused Play versus active Pause geometry, empty-title state fallback, layout bounds, cached measurements, native ownership/input consumption, drag/close/minimized geometry and shared panel restoration. Independent static review found no blocker.
- Installed into local Development item 900001 with the game closed. Installed DLL matches the build hash. The installer retained the music path and loader choices, kept the native player enabled and timed HUD hidden, and confirmed subscribed files were unchanged. Backup and scoped rollback script are at `evidence/install-native-popup-20261013-191903-360/`.
- In-game visual comfort and the new icon/header layout still need focused user acceptance. Previous successful text-button acceptance does not establish that the new presentation is readable. Earlier rc2 archives still contain the accepted larger presentation; publication/repackaging is pending this refinement.

Full build output: ignored `build/player2-build.log` and a retained copy in the installation evidence folder.

## Small icon player acceptance — October 13, 2026

Build: `1.0.0-player2`, local Development item `900001`, installed DLL SHA-256 `0b566841f4c88853e8f81babe5c425a9fe8c34d2f9f234a40dcc06754608faba`.

The user accepted the smaller icon layout and authorized proceeding toward final 1.0 publication. Visual comfort and layout acceptance are user-reported. The following completed traces corroborate playback-control processing; they do not independently establish pixel placement or audible quality. Their UTC filenames fall on October 14, while the sessions occurred October 13 in America/Chicago.

| Trace | Time to final completed Stop | Selections | Applied Next / Previous-track / Restart / Play-pause |
| --- | --- | --- | --- |
| `trace-20261014-002221-50880.tsv` | 2 minutes 52.624 seconds | 7 | 3 / 1 / 1 / 4 |
| `trace-20261014-002538-40820.tsv` | 2 minutes 32.516 seconds | 8 | 2 / 3 / 1 / 2 |
| `trace-20261014-002850-53420.tsv` | 1 minute 21.223 seconds | 2 | 0 / 0 / 0 / 0 |

- All three traces identify `1.0.0-player2`, end with a completed native Stop, and record zero failed/ignored controls, failed-load timeouts or nonzero dropped-event counts. These sessions contain successful pause/resume, Next, direct Previous-track and Restart commands. Selection counts include navigation and restarts. No deferred Play event is present.
- These focused sessions do not contain a complete natural track ending. Full natural transitions are established by the earlier `player1` evidence above; this acceptance does not claim a new long-duration audio test.
- The latest runtime report contains exactly one Red Wolf Radio instance with the installed candidate hash above, API 4, thirteen successful hooks and zero hook failures. Bootstrap completed with status zero.
- At **19:30:13.315 local time**, RML records WRSR PID **53420** exiting normally with code **0x0** after **1 minute 22.834 seconds**. The session summary reports zero final hook-chain audit issues. Earlier process exit codes were overwritten on relaunch; their traces retain completed Stop events.
- No WARN/ERROR/FATAL severity entry or Red Wolf Radio error was found in the latest runtime log. The unrelated helicopter plugin emits an INFO-level warning about its unavailable native menu classification mask. Latest startup reports two native custom XWMA files, zero WAV conversions, one cache hit, zero rejected inputs and a well-formed configuration. The final session records one native-player request.
- The captured INI retains the external XWMA fixture folder, originals mode `none`, `NativeSongPopup=true`, `ShowTrackName=false` and `Language=en`.

Three completed traces, current runtime/session logs, runtime report, INI, matching source/copy hash manifest, parsed totals and evidence notes are retained under ignored `evidence/acceptance-native-player2-20261013/`. No source code or installed files were changed by this review. This acceptance applies to the `player2` candidate; final `1.0.0` artifact hashes and publication evidence are recorded separately.

## Final 1.0.0 build — October 13, 2026

DLL SHA-256 `A519B312B2DF923B3885BA3C7849FFC5FCB5056E1782382B1D3010D16FC75CC2`. The only implementation change since accepted `player2` is the release version string. The full automated build suite passed, including converter, forwarding, scheduler/cache, controls, source preservation, native XWMA/display, native popup ABI and compact icon UI tests. Build output and PE report are retained under ignored `evidence/release-1.0.0/`.

The final package includes portable default settings, the companion Script action/icon/registration fragment, English/Russian documentation and guide JPEGs, approved artwork and license notices. Archive verification, registered-item upload and downloaded-payload checks are recorded separately as they complete; the successful build itself is not evidence of publication.

## Final 1.0.0 local smoke test — October 13, 2026

After installation into Development item `900001`, the final version ran in session `trace-20261014-004159-54088.tsv` (UTC filename October 14; local date October 13). The live UI check opened the compact icon player, displayed Cyrillic and percent-containing source names, and exercised Next and Pause/Resume while game simulation remained paused. These screen observations are separate from the playback-log evidence below.

- The trace header, RML plugin version and session summary identify `1.0.0`. Independently measured installed and build DLL hashes both equal `A519B312B2DF923B3885BA3C7849FFC5FCB5056E1782382B1D3010D16FC75CC2`.
- **RML metadata discrepancy:** its queued-plugin entry and runtime report retain the previous `player2` hash `0b566841f4c88853e8f81babe5c425a9fe8c34d2f9f234a40dcc06754608faba`. This is consistent with cached launcher discovery metadata, but that mechanism was not independently instrumented. The captured report is not treated as final-DLL hash verification; the direct file hashes and executing plugin's final version string are recorded separately. Original evidence is retained unchanged.
- The trace records three external selections, one applied Next and two applied Play/Pause actions (pause, then resume), with zero failed/ignored controls, failed-load timeouts or nonzero dropped events. Its final completed native Stop occurs at **331.811 seconds**. No full natural track ending occurs in this session: menu playback stops for loading, the Cyrillic gameplay track is manually advanced, and the final track is paused/resumed before exit.
- The runtime report contains exactly one active Red Wolf Radio instance, API 4, thirteen successful hooks and zero hook failures. Bootstrap status is zero. The native-player request and both custom XWMA preparations are recorded; no WAV conversion was needed.
- RML records PID **54088** exiting normally at **19:47:32.351 local time**, code **0x0**, after **5 minutes 33.497 seconds**. The session summary reports zero final hook-chain audit issues. There are no WARN/ERROR/FATAL severity entries or Red Wolf Radio errors; the unrelated helicopter-plugin INFO warning about its native menu mask remains.

The completed trace, runtime/session logs, runtime report, INI, five matching source/copy hashes, independent installed/build hashes and parsed totals are preserved under ignored `evidence/release-1.0.0/smoke-local/`. This is local final-binary smoke evidence, not downloaded Workshop-payload acceptance. Published release archives were not modified by this follow-up documentation.

## Final 1.0.0 publication and ordinary Workshop download — October 13, 2026

The public GitHub release `v1.0.0` was published at **19:47:03 local time** from tag `v1.0.0`, resolving to commit `c5193ccdf6c8248643553125ab3d8093daed5d5b`. Its recorded asset digests match the unchanged, independently hashed local archives:

- `RedWolfRadio-1.0.0.zip`: `9e7972ee9365bcfb32bd60f29a228b59e98491f117f1709771c679d325f5358e`.
- `RedWolfRadio-Workshop-1.0.0.zip`: `3658e776b53fd40b09f8e0f2086ae55261db10eb3fee50bc61d5384ba9d65d49`.

The in-game upload of Workshop item `3805202523` completed successfully at **19:51:59 local time**, with Steam recording manifest **`1537676965026481806`** and `Upload finished ... : OK`. The authoring session's refreshed RML report identifies final version `1.0.0` with exact DLL hash `A519B312B2DF923B3885BA3C7849FFC5FCB5056E1782382B1D3010D16FC75CC2`, API 4, thirteen successful hooks and zero failures. This supplies the corrected final hash metadata after refreshing the launcher inventory. PID **52736** exited normally at **19:53:00.833**, code **0x0**, after **2 minutes 42.523 seconds**; the final hook-chain audit had zero issues.

After a normal Steam client restart, Steam detected the new manifest at **19:55:45**, started an ordinary updating Workshop download at **19:55:46**, and finished with **No Error** at **19:55:49**. The downloaded DLL matches the final `A519...` hash. Twenty-two of the twenty-three release payload entries match the archive byte-for-byte, including DLL, companion script/icon, both guides, documentation and default INI. The sole differing file is `previewimage.png`: the downloaded file and current authoring file both hash to `DEFA3C7E297058C6B2452DF4B6ADD321A87F46D26CDA948460ED0273865D2863`, whereas the archive and pre-upload staging preview hash to `EF4BEF20BD0995C4CDE3B804ABB7F32CE4B5CE076E1426BE7800F70DB003E3BD`. This is consistent with upload-side preview processing; the evidence does not establish its precise mechanism.

**Settings were replaced in this ordinary update.** The backed-up personal INI hash `1714311A3319A91078E8EDA90E12C18387F5D4682D66D7ADC6A1F9C85B1BD789` contained `ExternalPath=C:\Users\david\Music\W&RSR Music`, originals mode `none`, and no new Display keys. After download, the installed INI was the packaged default, hash `F4E37227F94FCADE81AF34B0EA43E408907BB8AFE5E116CDD6C3236C2F05ECCC`, with `%USERPROFILE%\Music` and originals mode `all`. Both versions were preserved before restoration. This result limits the September settings-preservation observation above: ordinary updates must not be assumed to preserve personal settings when the shipped INI changes. This capture itself does not establish settings restoration or subscribed-plugin playback.

Evidence is retained under ignored `evidence/release-1.0.0/workshop-upload/`, together with the parent folder's `github-publication.json` and `archive-audit.json`. Original published archives were not changed by this documentation follow-up.

The saved personal INI was subsequently restored manually to the subscribed installation with the game closed. An independent readback confirmed exact SHA-256 `1714311A3319A91078E8EDA90E12C18387F5D4682D66D7ADC6A1F9C85B1BD789`, matching the pre-update backup and retaining the personal music folder and originals mode `none`. This was a recovery from the explicitly preserved backup, not an automatic backup or automatic settings migration supplied by version `1.0.0`. The received default and restored personal copy remain in the publication evidence. The public `1.0.0` archives remain unchanged; subscribed runtime verification follows separately.

## Final 1.0.0 subscribed runtime acceptance — October 13, 2026

RML's launch log queued Red Wolf Radio from the subscribed item path `steamapps/workshop/content/784150/3805202523/plugin/RedWolfRadio.dll`, with final SHA-256 `A519B312B2DF923B3885BA3C7849FFC5FCB5056E1782382B1D3010D16FC75CC2`. Development item `900001` was explicitly disabled, and the runtime report contains exactly one active Red Wolf Radio instance: version `1.0.0`, API 4, thirteen successful hooks and zero failures. The restored personal INI retained hash `1714311A3319A91078E8EDA90E12C18387F5D4682D66D7ADC6A1F9C85B1BD789` and resolved the personal music folder with originals mode `none`. Startup prepared five converted tracks, with zero rejected inputs.

The live UI smoke check opened the player through the subscribed Script action. It displayed a source filename containing Latin/Cyrillic text, changed to another source name after Next, and changed between Pause bars and the Play triangle when paused/resumed. Simulation remained paused during the gameplay check; no save was made. The player was closed and the game returned to the main menu before the later documentation upload. These observations are recorded separately from trace evidence; screenshot `native-player-next-resume.png` is retained in the local evidence folder.

Trace `trace-20261014-011749-55880.tsv` records four external selections, one applied Next and two applied Play/Pause actions. It ends with a completed native Stop at **603.531 seconds** and has zero failed/ignored controls, failed-load timeouts or nonzero dropped events. Manual navigation and menu transitions interrupt these songs, so this session does not establish another complete natural track ending. The selected personal tracks exercise converted playback; native XWMA coverage remains established by the earlier focused tests.

RML records PID **55880** exiting normally at **20:27:53.645 local time**, code **0x0**, after **10 minutes 4.457 seconds**. Its session summary has zero final hook-chain audit issues. No WARN/ERROR/FATAL severity entry or Red Wolf Radio error was found; the unrelated helicopter-plugin INFO warning remains. After the game and RML exited, the Steam client visibly returned to its green Play button.

Evidence is retained under ignored `evidence/release-1.0.0/smoke-subscribed/`. The five completed trace/runtime/session/report/INI captures have matching source/copy hashes. The Steam log was copied and its snapshot hashed, but the live source was locked during hashing; that entry explicitly makes no source/copy equality claim. Timestamped live snapshots are distinguished from completed evidence.

## Documentation revision delivery and privacy verification — October 13, 2026

The public `v1.0.0` GitHub release now provides explicitly named documentation-revision archives. Runtime version, INI template and DLL are unchanged; the DLL remains `A519...`. Server-reported asset digests match the local revised ZIPs:

- `RedWolfRadio-1.0.0-docs-r1.zip`: `b414a676ac9aba98d6b47570c194e95297248d3d88dee710f43da806ea789ec8`.
- `RedWolfRadio-Workshop-1.0.0-docs-r1.zip`: `1f86c525569fa23459f99a4c0fdd5c47e2c0fd93adfe440fa12b25a35139dc7d`.

The Workshop documentation revision uploaded successfully at **20:27:04 local time**, manifest **`177600152962843204`**. Following a normal Steam restart, Steam detected that manifest at **20:31:31**, started an ordinary update at **20:31:33**, and finished with **No Error** at **20:31:37**.

Of the twenty-three revised release payload entries, twenty-one downloaded files match the revised ZIP hashes exactly. The only differences are expected: the restored personal INI remains `1714311A3319A91078E8EDA90E12C18387F5D4682D66D7ADC6A1F9C85B1BD789`, and the Workshop preview remains the previously verified processed file `DEFA3C7E297058C6B2452DF4B6ADD321A87F46D26CDA948460ED0273865D2863`. There are no other payload mismatches. This documentation-only update preserved the personalized INI; it does not undo the earlier 1.0 update's replacement result or establish an automatic settings-preservation guarantee.

All sixteen downloaded Markdown, text and INI files outside runtime logs/cache were scanned: no private character-name references remain, and the previous blanket ordinary-update preservation claim is absent. The downloaded README now instructs users to back up settings before any update and check/restore their preferences afterward. The English/Russian descriptions, guides and unchanged executable were delivered in the verified payload. Public listing/gallery review was performed separately through the browser; the file audit alone does not establish web-page rendering.

Final download logs, revised payload/text audits, retained personal INI and GitHub asset verification are in `evidence/release-1.0.0/smoke-subscribed/`; documentation-revision publication records are in `evidence/release-1.0.0/docs-r1/`. No further game launch was performed for this documentation-only delivery check, and the original local release archives remain unchanged.
