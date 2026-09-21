# Public release checklist

Planning review: September 19, 2026. Source-path candidate: `0.9.0-dev-source-paths` (automated suites and focused local Development tests passed). This checklist records remaining work; it does not certify a public release.

## Accepted playback baseline

- [x] Large external library, full-length playback, restart, background preparation, and cache eviction have recorded tests.
- [x] Gameplay Next, Previous, true pause/resume, and navigation while paused passed user acceptance and log review.
- [x] Automated converter, scheduler, forwarding, cache, and control suites passed. See [VALIDATION.md](VALIDATION.md) for the exact scope and build hashes.
- [x] Approved Sangie artwork is included in both staged archive layouts as `previewimage.png`.

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
- [x] Define and test INI preservation during installation and updates. An ordinary Steam update retained the personalized INI byte-for-byte while replacing the DLL. The README still requires an external backup before RML force-refresh, unsubscribe/resubscribe, or manual replacement because those workflows may recreate the item folder.

Keep the existing defaults: upcoming five tracks, three protected previous tracks, and a 4096 MiB soft cache limit. Document that protected files may temporarily exceed the limit and that backward navigation remembers up to 64 entries, independently of cache retention.

The user's September 20 report accepts the targeted configuration checks. The later ordinary Workshop update test establishes preservation for Steam's normal incremental update path.

The user subsequently requested `Ctrl+Shift+Down` as the default Play/Pause binding because Space also paused the game. The rebuilt candidate and saved settings include this adjustment, automated suites pass, and the September 20 subscribed-package gameplay test accepted the revised binding.

## 4. Test an actual Workshop installation before public visibility

Workshop item `3805202523` is registered as Script. The initial candidate upload completed at 14:27:42 on September 20, 2026, with manifest `5000316681469668187`; a targeted Steam API download verified its 15 files. The 0.9.0 update-test upload completed at 20:09:32 with manifest `1652410860917990531`. After WRSR and RML closed normally, restarting Steam triggered its ordinary `updating` job, replaced the DLL, and preserved the personalized INI byte-for-byte. See [VALIDATION.md](VALIDATION.md).

- [x] Register the project through the game and preserve its generated metadata.
- [x] Include the approved preview and explainer gallery image, and add Republic Mod Loader item `3787969749` as a required item.
- [x] Complete the final listing description and supported-version review with the release candidate.
- [x] Upload with non-public visibility and verify the subscribed payload, including the candidate DLL hash.
- [x] Confirm RML associates the enabled plugin with its own Workshop item and shows Sangie's packaged preview in the Plugins detail panel.
- [x] Verify **Open plugin INI Settings** opens the subscribed plugin's INI; preserve backups and restore the personal configuration beside that DLL.
- [x] Keep both Development copies disabled and confirm there is no active loose-DLL copy.
- [x] Confirm startup from the subscribed installation reaches the main menu with exactly one candidate instance, API 4 accepted, and nine successful hooks.
- [x] Confirm playback, normal exit, and relaunch from the subscribed installation. September 20 user acceptance and trace review cover mixed original/custom tracks and working controls; the latest RML report confirms exit code `0x0` and zero hook-audit issues.
- [x] Confirm the revised `Ctrl+Shift+Down` pause/resume binding in gameplay. User acceptance and successful shortcut events are recorded in [VALIDATION.md](VALIDATION.md).
- [x] Test an ordinary Steam update retaining settings and finish the documented preservation strategy. Steam installed manifest `1652410860917990531`; the personalized INI retained SHA-256 `527b7b93a0d4d31e0177472962498d4768b9f1fb5d215ab9eba8740255fcd2fb` while the DLL changed to the 0.9.0 hash.
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
