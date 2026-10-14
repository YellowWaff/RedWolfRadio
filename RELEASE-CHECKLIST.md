# Public release checklist

## 1.0 publication status — October 13, 2026

The final `1.0.0` source and release are published. GitHub serves the explicitly named `docs-r1` download revision, containing documentation corrections with the same DLL, runtime files and INI; the `v1.0.0` source tag is unchanged. Steam's uploads/downloads, the subscribed player's live controls and normal shutdown are verified. The corrected bilingual and dedicated Russian listing text, both gallery guides and bilingual 1.0 change notes are public and checked. The final documentation-only Workshop refresh also completed successfully. The public plugin version remains `1.0.0`.

Update finding: the initial ordinary 1.0 Workshop download replaced the personalized INI with the shipped template. The external backup was restored byte-for-byte. The later `docs-r1` update retained that restored file, but keep a backup outside Steam and game folders before every update; compare settings after download and distinguish manual restoration from automatic preservation. Neither the successful 0.9.0 test nor this documentation-only update guarantees preservation during future updates.

- [x] External XWMA, percent/Cyrillic source names, natural transitions, pause and volume accepted in-game.
- [x] Compact player: live title updates, one window, all four buttons, dragging, close/reopen without interrupting playback, 1440x1080 layout and covered-button input accepted.
- [x] Smaller `player2` frame and icon controls accepted by the user after installation; automated UI and playback suites passed for that development build.
- [x] Review completed traces and latest RML diagnostics: normal exit, thirteen successful hooks, no hook failures or lost events. See [VALIDATION.md](VALIDATION.md).
- [x] Earlier rc2 preparation: both archives included script, icon, defaults, English/Russian instructions and matching DLL checksums. Full automated suite passed; each ZIP had 24 verified files, portable settings and no music/cache/log payload. See `evidence/release-1.0.0-rc2/archive-audit.json` for those historical checksums.
- [x] Build final `1.0.0`, pass the automated suites, and record the final DLL hash: `A519B312B2DF923B3885BA3C7849FFC5FCB5056E1782382B1D3010D16FC75CC2`. Build output is retained under `evidence/release-1.0.0/`; the `player2` and rc2 hashes identify earlier binaries.
- [x] Build and inspect both original final archives: each had exactly 23 expected files, byte-for-byte ZIP/stage/source agreement, a verified six-file runtime checksum manifest, final DLL/version, canonical English/Russian descriptions, current guides below 2 MB, Script action/icon, metadata fragment, portable defaults, and no personal music, caches, logs or Workshop metadata. See `evidence/release-1.0.0/archive-audit.json` for this historical baseline; the current public download revision is `docs-r1` below.
- [x] Publish release source commit `c5193ccdf6c8248643553125ab3d8093daed5d5b` on `main` and tag it as `v1.0.0`. The commit includes the new headers/tests, companion distribution files, guides and build changes; local installation helpers, evidence and `log.html` are excluded. Later documentation corrections may advance `main` without moving the release tag.
- [x] Stage the verified Workshop payload into registered item `3805202523`, preserving its metadata and adding the companion script registration once. The subsequent successful upload and downloaded companion files corroborate the staged item. The personal INI backup was kept outside Steam and game folders.
- [x] Smoke-test the final local packaged installation with one active radio copy: Script action opened the icon player; Next and Pause/Resume worked; thirteen hooks succeeded with zero failures. Direct installed/build DLL hashes matched `A519B312B2DF923B3885BA3C7849FFC5FCB5056E1782382B1D3010D16FC75CC2`. RML recorded PID `54088` exiting with code `0x0` at 19:47:32.351 local time, with zero final hook-audit issues. The earlier cached launcher-hash discrepancy and later corrected report are recorded in [VALIDATION.md](VALIDATION.md); the cached value is not used as final hash evidence.
- [x] Publish [GitHub release v1.0.0](https://github.com/YellowWaff/RedWolfRadio/releases/tag/v1.0.0), initially confirmed at 19:47:03 local time. The current public assets are `RedWolfRadio-1.0.0-docs-r1.zip` (SHA-256 `b414a676ac9aba98d6b47570c194e95297248d3d88dee710f43da806ea789ec8`) and `RedWolfRadio-Workshop-1.0.0-docs-r1.zip` (`1f86c525569fa23459f99a4c0fdd5c47e2c0fd93adfe440fa12b25a35139dc7d`). Their published digests match the verified documentation-only revisions; the original ZIP assets are no longer served by the release. Original archives and hashes remain local historical evidence. DLL, runtime files, INI, plugin version and source tag are unchanged. See `evidence/release-1.0.0/docs-r1/github-publication.json` and `archive-audit.json`.
- [x] Submit the Workshop payload. Steam recorded `Upload finished ... : OK` at 19:51:59 local time for item `3805202523`, manifest `1537676965026481806`. The refreshed authoring-session report identified final `1.0.0` and the exact `A519…` DLL hash, thirteen successful hooks and zero failures; that session exited normally at 19:53:00.833 with zero hook-audit issues.
- [x] Verify the ordinary subscribed download completed with `No Error` at 19:55:49 local time. The final DLL and companion files match; 22 of 23 release entries match byte-for-byte. The preview differs from the archive but matches the post-upload authoring preview, as documented in [VALIDATION.md](VALIDATION.md). The download replaced the personal INI with the shipped template; the saved personal file was then restored manually and verified against its original SHA-256 `1714311A3319A91078E8EDA90E12C18387F5D4682D66D7ADC6A1F9C85B1BD789`.
- [x] Save and read back the corrected bilingual Steam listing and dedicated Russian description, including the backup-before-any-update guidance and neutral artwork permission wording. The bilingual 1.0 change notes are also public. Current public descriptive text has passed the requested privacy check.
- [x] Complete the final public gallery check. The English guide replaced the old image in place (approximately 1.711 MB PNG), and the Russian guide was added (approximately 520 KB JPEG). Both were checked on the public listing; proof is retained at `evidence/release-1.0.0/workshop-public-release.png`.
- [x] Exercise the downloaded `1.0.0` player with only the subscribed copy active and the restored personal INI: the Script action opened the player with an original song filename, Next changed the displayed title, Pause changed its icon to Play, and Resume changed it back to Pause. Simulation remained paused on 22 October 1967 and no save was made. This completes the subscribed UI/control check; shutdown evidence is separate below.
- [x] Stage the `docs-r1` Workshop documentation refresh into the existing registered item. The staging audit confirms protected runtime files, packaged INI, metadata identity, subscribed personal INI and RML configuration remained unchanged. See `evidence/release-1.0.0/docs-r1/authoring-stage-20261013-201517-993/stage-audit.json`. The hashed ZIPs remain frozen; later source documentation updates do not alter them.
- [x] Close the subscribed test session normally. RML recorded PID `55880` exiting with code `0x0` at **20:27:53.645 local time**, after **10 minutes 4.457 seconds**. This is the downloaded-plugin session exercised above, separate from the earlier local smoke test. Detailed runtime and trace evidence is recorded in [VALIDATION.md](VALIDATION.md).
- [x] Submit and download the final documentation-only Workshop refresh. Steam recorded upload `OK` at **20:27:04 local time**, manifest **`177600152962843204`**. Its ordinary update download ran **20:31:33–20:31:37** and completed with **No Error**. The restored personal INI retained exact SHA-256 `1714311A3319A91078E8EDA90E12C18387F5D4682D66D7ADC6A1F9C85B1BD789` during this update. Runtime code, DLL version `1.0.0` and source tag were not changed. The complete payload comparison is recorded separately in [VALIDATION.md](VALIDATION.md); the two upload manifests identify distinct publication steps.

The remaining sections are the completed **0.9.0 release record**, not open 1.0 requirements.

## 0.9.0 historical release checklist

Planning review: September 19, 2026. Source-path candidate: `0.9.0-dev-source-paths` (automated suites and focused local Development tests passed). This checklist records remaining work; it does not certify a public release.

## Accepted playback baseline

- [x] Large external library, full-length playback, restart, background preparation, and cache eviction have recorded tests.
- [x] Gameplay Next, Previous, true pause/resume, and navigation while paused passed user acceptance and log review.
- [x] Automated converter, scheduler, forwarding, cache, and control suites passed. See [VALIDATION.md](VALIDATION.md) for the exact scope and build hashes.
- [x] Approved character artwork is included in both staged archive layouts as `previewimage.png`.

Another broad hour-long soak is not the next gate. Focus on the untested installation paths and settings below; repeat longer playback if subsequent changes justify it.

## 1. Correct source folders and original-track discovery

- [x] Resolve original music from the game executable's directory, not `host.baseDir`.
- [x] Resolve optional plugin-local `Music` from the actual Red Wolf Radio DLL directory, not `host.pluginDir`.
- [x] Add focused discovery tests with the game, RML, and plugin in separate directories.
- [x] Log resolved source folders and an original-track ID inventory so users can configure `ExcludedIds` without guessing.
- [x] Verify `Originals.Mode=all`, `selected`, and `none` against the discovered originals, including native XWMA calls/playing status and selected exclusions. Verify original files remain unchanged. See [VALIDATION.md](VALIDATION.md) for the exact test scope.

The candidate fixes the old `rebuildCatalog` use of RML's shared host directories. Automated and focused live tests cover the repaired paths and original-track modes. The actual 33 original music files retain their pre-work hashes.

## 2. Test the local package and artwork

- [x] Stage a local RML Development project with root `workshopconfig.ini`, `previewimage.png`, and `plugin/RedWolfRadio.dll`. The temporary local ID is not a Steam item; use the game's authoring workflow to obtain a real ID before Workshop publication.
- [x] Preserve the current tested DLL and user INI, and ensure only one Red Wolf Radio copy is active during the package test.
- [x] Confirm artwork in RML's **Development** detail panel and the correct project name. Edit `plugin/RedWolfRadio.ini` directly; RML's Development panel has no plugin **Open settings** button.
- [x] Confirm the optional local Music folder works from this separate installation path.

The Development artwork is now visually verified with the enabled project. RML's **Plugins** image lookup only uses the parent Workshop item, so a local development image will not appear in that separate plugin panel. See [DISTRIBUTION.md](DISTRIBUTION.md).

## 3. Finish targeted configuration checks

- [x] Verify portable `%USERPROFILE%\Music` defaults and blank-path Windows Music known-folder resolution. User reported step 3 worked as expected on September 20, 2026.
- [x] Check missing/empty folders, a rejected audio file, and a one-track library; playback should fail or continue predictably without a crash or busy loop. Confirm normal artwork files are ignored. User-reported acceptance; no new per-case logs reviewed.
- [x] Run a short mixed WAV/MP3/FLAC check on the release candidate after the path changes. User-reported acceptance.
- [x] Remap a shortcut in the INI, disable one binding, and check game-focus behavior in a loaded republic. User-reported acceptance.
- [x] Test the 0.9.0 INI update behavior. That ordinary Steam update retained the personalized INI byte-for-byte while replacing the DLL. The 1.0 download later replaced it; current instructions require an external backup before every update, including ordinary Steam updates.

Keep the existing defaults: upcoming five tracks, three protected previous tracks, and a 4096 MiB soft cache limit. Document that protected files may temporarily exceed the limit and that backward navigation remembers up to 64 entries, independently of cache retention.

The user's September 20 report accepts the targeted configuration checks. The later ordinary Workshop update test establishes preservation for that 0.9.0 download, not for all future incremental updates.

The user subsequently requested `Ctrl+Shift+Down` as the default Play/Pause binding because Space also paused the game. The rebuilt candidate and saved settings include this adjustment, automated suites pass, and the September 20 subscribed-package gameplay test accepted the revised binding.

## 4. Test an actual Workshop installation before public visibility

Workshop item `3805202523` is registered as Script. The initial candidate upload completed at 14:27:42 on September 20, 2026, with manifest `5000316681469668187`; a targeted Steam API download verified its 15 files. The 0.9.0 update-test upload completed at 20:09:32 with manifest `1652410860917990531`. After WRSR and RML closed normally, restarting Steam triggered its ordinary `updating` job, replaced the DLL, and preserved the personalized INI byte-for-byte. See [VALIDATION.md](VALIDATION.md).

- [x] Register the project through the game and preserve its generated metadata.
- [x] Include the approved preview and explainer gallery image, and add Republic Mod Loader item `3787969749` as a required item.
- [x] Complete the final listing description and supported-version review with the release candidate.
- [x] Upload with non-public visibility and verify the subscribed payload, including the candidate DLL hash.
- [x] Confirm RML associates the enabled plugin with its own Workshop item and shows the packaged character preview in the Plugins detail panel.
- [x] Verify **Open plugin INI Settings** opens the subscribed plugin's INI; preserve backups and restore the personal configuration beside that DLL.
- [x] Keep both Development copies disabled and confirm there is no active loose-DLL copy.
- [x] Confirm startup from the subscribed installation reaches the main menu with exactly one candidate instance, API 4 accepted, and nine successful hooks.
- [x] Confirm playback, normal exit, and relaunch from the subscribed installation. September 20 user acceptance and trace review cover mixed original/custom tracks and working controls; the latest RML report confirms exit code `0x0` and zero hook-audit issues.
- [x] Confirm the revised `Ctrl+Shift+Down` pause/resume binding in gameplay. User acceptance and successful shortcut events are recorded in [VALIDATION.md](VALIDATION.md).
- [x] Test the ordinary 0.9.0 Steam update. Steam installed manifest `1652410860917990531`; the personalized INI retained SHA-256 `527b7b93a0d4d31e0177472962498d4768b9f1fb5d215ab9eba8740255fcd2fb` while the DLL changed to the 0.9.0 hash. This historical result does not guarantee settings preservation in later updates.
- [x] Test disabling the subscribed plugin and confirm the game launches normally. RML omitted Red Wolf Radio, WRSR reached the main menu, and the session exited normally with code `0x0`; the subscribed plugin was then re-enabled.
- [x] Diagnose and clear the reported Steam running state after the game and launcher exit. A Notepad++ instance opened through RML's settings button retained a Steam overlay for WRSR. Exiting the saved editor normally returned Steam to **Play** and removed the app from its running list. The workaround is documented; no loader or plugin lifecycle code was changed.

The Steam listing preview and packaged `previewimage.png` serve different readers. Include both; no manually configured thumbnail URL is needed by RML. The current ZIP stages files but does not create Workshop metadata or publish an item.

## 5. Finalize the release candidate and publication

- [x] Update version strings and release notes together; rebuild and run the relevant suites after code changes.
- [x] Replace outdated baseline wording in the README, verify installation/update instructions, and record candidate hashes and focused acceptance results.
- [x] State supported Windows x64 / WRSR 1.1.1.9 / RML 1.0.1 compatibility and the enforced binary checks. Retain gameplay-only shortcut scope.
- [x] State WAV, MP3, FLAC, and original game XWMA support. Leave arbitrary external XWMA explicitly unsupported in this release.
- [x] Inspect the final archives and public source contents: DLL, defaults, preview, documentation, buildable source, and license notices; no personal songs, caches, diagnostic logs, or unapproved artwork. Both archives contain 14 expected entries and the DLL SHA-256 `22e3584d59c23a738e767e5614dd86feb4c934e9c8f2d18b2d944b854477d53b`.
- [x] Publish the matching source/tag and binaries on GitHub and make the Workshop item public once the candidate's remaining checks pass. GitHub release `v0.9.0` contains both verified archives, the public Workshop listing is item `3805202523`, and both descriptions link to the other release channel.

Red Wolf Radio 0.9.0 was published on GitHub and Steam Workshop on September 20, 2026.
