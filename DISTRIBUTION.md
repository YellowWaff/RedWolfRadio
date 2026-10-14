# Distribution layout

## Republic Mod Loader behavior

Republic Mod Loader 1.0.1 does not provide a per-plugin icon or image field. For a plugin discovered inside a Steam Workshop item, RML uses the parent item's thumbnail as the plugin detail image.

This was verified against Republic Mod Loader source revision `57e567e5cccaa3c38aee7e9d599d2443d2460b10`:

- [`findWorkshopThumbnail`](https://github.com/Ultimate-Universe/WRSR-RepublicModLoader/blob/57e567e5cccaa3c38aee7e9d599d2443d2460b10/Source/launcher/workshop_windows.go#L257) scans the Workshop root and one directory below it. Image filenames containing `preview` receive the highest preference.
- [`ensurePluginImage`](https://github.com/Ultimate-Universe/WRSR-RepublicModLoader/blob/57e567e5cccaa3c38aee7e9d599d2443d2460b10/Source/launcher/workshop_windows.go#L402) loads the selected plugin's parent Workshop thumbnail.
- [`findWorkshopPluginDirs`](https://github.com/Ultimate-Universe/WRSR-RepublicModLoader/blob/57e567e5cccaa3c38aee7e9d599d2443d2460b10/Source/launcher/workshop_windows.go#L88) discovers top-level `plugin` or `plugins` directories and the same directories one level below the item root.

## Workshop package

Use this structure for Red Wolf Radio's own Workshop item:

```text
RedWolfRadioWorkshopItem/
|-- previewimage.png
|-- rwr_now_playing.txt
|-- rwr_now_playing.png
|-- workshop-script-entry.txt
|-- plugin/
|   |-- RedWolfRadio.dll
|   `-- RedWolfRadio.ini
|-- README.md
|-- LICENSE
|-- THIRD-PARTY-NOTICES.md
`-- ARTWORK-TERMS.md
```

`previewimage.png` is a 512 by 512 PNG and remains below 1 MiB. Its name gives it the highest RML thumbnail score. Installing the plugin within its own Workshop item gives the DLL that item's Workshop identity, allowing RML to show the correct art.

The compact player also needs the companion Script-menu action. `rwr_now_playing.txt` contains that action, and `rwr_now_playing.png` is its menu icon, copied from the canonical `previewimage.png`. Keep both files at the Workshop item root, alongside `workshopconfig.ini`, rather than inside `plugin/`.

The package deliberately supplies a **fragment**, `workshop-script-entry.txt`, instead of a replacement `workshopconfig.ini`. In the game's existing registered Script item, merge this line once, immediately before `$END`:

```text
$OBJECT_SCRIPT rwr_now_playing "Red Wolf Radio: Now playing"
```

Preserve the registered item ID, type, title, description, visibility, and all other metadata. If an `rwr_now_playing` entry is already present, update it rather than adding another. A snippet or numeric local folder does not register a Steam item. The repository's canonical companion source lives under `distribution/`; the earlier `prototype/native-popup/` files are development evidence.

With `[Display] NativeSongPopup=true`, selecting **Red Wolf Radio: Now playing** in the game's Script menu opens one movable player that refreshes automatically. The script and metadata registration are both required for this menu entry; the DLL and INI alone provide playback and hotkeys. The shipped defaults enable the player bridge and disable the separate timed HUD notification. Steam updates can replace a customized INI, so back it up outside Steam and game folders before any update and check it afterward. A restored INI with `NativeSongPopup=false` must be changed to `true`, followed by a restart, to use the player.

## Direct local installation

The standalone archive places `RedWolfRadio.dll` and `RedWolfRadio.ini` at its root, and also includes the companion action, icon, and metadata fragment described above. A loose DLL in RML's plugin directory provides playback and hotkeys, but does not register the Script-menu player or an independent preview image. Do not overwrite RML's own `previewimage.png`; that image belongs to the loader's Workshop item and is shared by every plugin installed inside that item.

For a complete local installation, use a Script project created through the game's authoring workflow. Place the standalone archive's DLL and INI in that project's `plugin/` directory, put its `rwr_now_playing.txt`, `rwr_now_playing.png`, and `previewimage.png` at the project root, and merge `workshop-script-entry.txt` into the project's existing metadata as described above. The Workshop archive already has this directory layout, so it is the simpler choice for a local project. Enable only one Red Wolf Radio installation in RML. Back up an existing customized INI before replacing package files, and restore its settings afterward.

## Unpublished local artwork preview

RML also has a **Development** view for local WRSR projects under `<game>/media_soviet/workshop_wip/<numeric-project-id>/`. Its source requires `workshopconfig.ini` at that project's root, discovers the same `previewimage.png` and `plugin/` layout, and renders the project's thumbnail in the Development detail panel. This provides a way to inspect the artwork before public Workshop publication.

This is separate from the **Plugins** detail panel: `ensurePluginImage` only resolves a `WorkshopID`, not a `DevelopmentID`. Moving a DLL into a local development project does not by itself add an image to that plugin panel.

The behavior is verified in [`discoverDevelopmentMods` and `ensureDevelopmentImage`](https://github.com/Ultimate-Universe/WRSR-RepublicModLoader/blob/57e567e5cccaa3c38aee7e9d599d2443d2460b10/Source/launcher/development_windows.go). RML accepts a numeric local project ID without checking Steam registration. Such an ID is only for local testing: create a real Workshop item through the game's authoring workflow before uploading, and do not substitute RML's item ID.

Development DLLs use an independent inventory, so leaving both the local project and a loose copy enabled can load the plugin twice. Preserve the current INI and archive the loose DLL with a non-`.dll` extension before enabling the Development copy.

The Development panel offers **Open project folder** and **Open workshopconfig.ini**, not the ordinary plugin **Open settings** button. Edit `<project>/plugin/RedWolfRadio.ini` directly at this stage. During the later subscribed installation check, the Plugins panel displayed the correct artwork and **Open plugin INI Settings** opened the subscribed INI.

## Steam listing versus launcher image

Upload the approved image as the Steam listing preview and include `previewimage.png` in the item contents. RML reads the image from the installed item folder; it does not need a manually configured image URL. A Steam listing image alone does not replace the packaged file.

The packaging script stages the DLL, defaults, documentation, artwork, companion action/icon, and registration fragment. It checks that the DLL's embedded version matches `VERSION` and the package label; it also verifies the required runtime files against their sources and writes their hashes into `SHA256SUMS.txt`. Both archives include the companion files, current English/Russian guides, and canonical listing descriptions (`workshop-description.txt` and `workshop-description.ru.txt`). Internal acceptance instructions and superseded listing drafts are not included. It does not create `workshopconfig.ini`, register an item with Steam, or publish it. Merge the fragment and test the resulting authoring item before treating the Workshop archive as a finished upload. A locally built `1.0.0` archive alone does not establish that the public update has been uploaded or downloaded successfully; those results are recorded separately in [VALIDATION.md](VALIDATION.md).

## Updating the registered item to 1.0

The current GitHub downloads for plugin version `1.0.0` are the explicit **`docs-r1` documentation revision**. They correct update guidance and public descriptive text without changing the DLL, runtime files or default INI. The source tag `v1.0.0` remains at the original release commit. The older ZIP assets are no longer served from the GitHub release; their original bytes and hashes remain in local historical evidence.

| Current download | SHA-256 |
|---|---|
| `RedWolfRadio-1.0.0-docs-r1.zip` | `b414a676ac9aba98d6b47570c194e95297248d3d88dee710f43da806ea789ec8` |
| `RedWolfRadio-Workshop-1.0.0-docs-r1.zip` | `1f86c525569fa23459f99a4c0fdd5c47e2c0fd93adfe440fa12b25a35139dc7d` |

The GitHub release's asset digests match the frozen revisions. The corresponding Workshop documentation refresh was uploaded successfully on October 13, 2026, as manifest `177600152962843204`; its ordinary subscribed download completed with `No Error`. The restored personal INI stayed byte-for-byte intact during this documentation-only update. This does not change the backup guidance: the initial 1.0 update had replaced that file. The public plugin version remains `1.0.0`. Later source documentation updates do not rewrite the frozen ZIPs. See [RELEASE-CHECKLIST.md](RELEASE-CHECKLIST.md) and [VALIDATION.md](VALIDATION.md) for publication and runtime evidence.

Use the existing registered authoring project for item `3805202523`; do not create a replacement item or copy the temporary development ID. With the game closed, back up the authoring project and the subscribed personalized INI outside both item folders. Stage only the verified Workshop archive contents into the existing authoring project. Preserve its `workshopconfig.ini` and merge the companion registration line exactly once before `$END`. Keep the packaged INI portable and free of the maintainer's music paths; retain personalized test settings separately.

Verify the staged DLL hash, companion files, guides, default INI and existing registered item ID before using the game's upload action. After submission, check Steam's upload result and the listing. Then test the actual subscribed download with only that Red Wolf Radio copy active and confirm its DLL and companion files match the release. Compare the downloaded INI with the external backup; with the game closed, restore personal settings if replaced, then verify them and the Script-menu player. Record any replacement rather than treating a later manual restoration as automatic preservation. Keep the local authoring copy disabled while using the subscribed copy. Upload and subscription verification are separate from building an archive or passing local gameplay tests.

## First Workshop upload and local migration

Create an item through **Workshop → Your items (WIP) → Create new item** in WRSR. The current game's editor offers **Script** and **Unpublished**. Use the registered item folder and preserve its generated `workshopconfig.ini`; a temporary local numeric folder is not a registration. Populate the item with the clean Workshop package, leaving personal music, logs, caches, and settings backups outside the upload folder.

Before the in-game save/upload action, inspect the item editor's visibility label. Do not interpret the metadata's numeric `$VISIBILITY` using the Steam API enum. After uploading, verify the listing's actual visibility and add Republic Mod Loader as a required item in Steam's owner controls.

For the subscription test, disable all local Development copies in RML, subscribe to the item, wait for Steam to finish downloading, and refresh RML. Enable the downloaded plugin in its own Workshop folder; do not copy its DLL into the loader's shared plugin folder. Keep the authoring project disabled so it remains available for future uploads without being loaded alongside the subscribed DLL.

Back up the user's current `plugin/RedWolfRadio.ini` outside Steam and game folders before any update, including ordinary Workshop updates, RML force-refresh, unsubscribe/resubscribe and manual replacement. With the game closed, restore that INI beside the downloaded DLL if it was replaced or when migrating from a local installation. Back up any optional local Music files too. The `%LOCALAPPDATA%/RedWolfRadio` cache is shared across installations and can remain in place. The 0.9.0 update test retained the customized INI; the ordinary 1.0 download replaced it with the shipped template, and the user's backup was then restored byte-for-byte. Settings preservation cannot be assumed from the earlier test.

Item `3805202523` was uploaded through the game's green checkmark at 14:27:42 on September 20, 2026, including the `Ctrl+Shift+Down` default. The signed-in listing showed **Hidden**, 4.990 MB, three change notes, the explainer gallery image, and Republic Mod Loader item `3787969749` as a required dependency.

The subscribed payload required an assisted download using a targeted `SteamUGC::DownloadItem` request; RML's force-update procedure was not used. All 15 downloaded files then matched the authoring payload before the personal INI was restored. RML showed the enabled subscribed plugin with the correct Workshop identity and approved character preview, and its settings button opened that installation's INI. Both local Development copies remained disabled and the old loose DLL remained inactive. See [VALIDATION.md](VALIDATION.md) for hashes and evidence.

The subscribed DLL subsequently passed mixed original/custom playback, normal exit and relaunch, the revised pause binding, and all three keyboard controls. A separate launch with the subscribed plugin disabled reached the main menu and exited normally; the plugin was re-enabled afterward.

The 0.9.0 update test uploaded manifest `1652410860917990531` through the game, closed WRSR and RML normally, and restarted Steam. Steam detected the new manifest and completed an ordinary Workshop update without RML force-refresh or a targeted helper. The installed DLL changed to the 0.9.0 SHA-256 `22e3584d59c23a738e767e5614dd86feb4c934e9c8f2d18b2d944b854477d53b`; the personalized INI remained byte-for-byte identical at SHA-256 `527b7b93a0d4d31e0177472962498d4768b9f1fb5d215ab9eba8740255fcd2fb`.
