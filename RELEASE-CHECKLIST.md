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
- [ ] Define and test INI preservation during installation and updates. The plugin only creates an absent INI, but the archive ships a live default INI. RML's force-refresh removes the item folder; editable settings need a documented backup/restore or persistence strategy. Ordinary Steam update preservation is not yet established.

Keep the existing defaults: upcoming five tracks, three protected previous tracks, and a 4096 MiB soft cache limit. Document that protected files may temporarily exceed the limit and that backward navigation remembers up to 64 entries, independently of cache retention.

The user's September 20 report accepts the targeted configuration checks. Actual Workshop update preservation remains open and must be verified during step 4; the report does not establish Steam update behavior.

## 4. Test an actual Workshop installation before public visibility

The user created Workshop item `3805202523` through the game's authoring workflow on September 20, 2026. Its generated metadata identifies Red Wolf Radio as a Script item. The clean payload is staged and independently checked, and the in-game editor visibly shows **Unpublished**. Upload of the staged DLL, Steam-side visibility, the required-item relationship, subscription, and update tests remain pending.

- [ ] Complete project metadata and prepare the listing description, RML dependency, supported versions, and approved preview.
- [ ] Upload with non-public visibility for testing and install the item through Steam. Confirm the authoring workflow and visibility before submission.
- [ ] Confirm RML associates the plugin with Red Wolf Radio's own Workshop item and shows the packaged image in its plugin detail panel.
- [ ] Test clean installation, an update retaining settings, and disabling the plugin. Confirm there is no remaining active loose-DLL copy.

The Steam listing preview and packaged `previewimage.png` serve different readers. Include both; no manually configured thumbnail URL is needed by RML. The current ZIP stages files but does not create Workshop metadata or publish an item.

## 5. Finalize the release candidate and publication

- [ ] Update version strings and release notes together; rebuild and run the relevant suites after code changes.
- [ ] Replace outdated baseline wording in the README, verify installation/update instructions, and record candidate hashes and focused acceptance results.
- [ ] State supported Windows x64 / WRSR 1.1.1.9 / RML 1.0.1 compatibility and the enforced binary checks. Retain gameplay-only shortcut scope.
- [ ] State WAV, MP3, FLAC, and original game XWMA support. Leave arbitrary external XWMA explicitly unsupported in this release.
- [ ] Inspect the final archives and public source contents: DLL, defaults, preview, documentation, buildable source, and license notices; no personal songs, caches, diagnostic logs, or unapproved artwork.
- [ ] Publish the matching source/tag and binaries on GitHub and make the Workshop item public once the candidate's remaining checks pass. Add the real Workshop link to the README and link the listing back to GitHub.

Public visibility and external publication have not been changed by this planning review.
