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
|-- plugin/
|   |-- RedWolfRadio.dll
|   `-- RedWolfRadio.ini
|-- README.md
|-- LICENSE
|-- THIRD-PARTY-NOTICES.md
`-- ARTWORK-TERMS.md
```

`previewimage.png` is a 512 by 512 PNG and remains below 1 MiB. Its name gives it the highest RML thumbnail score. Installing the plugin within its own Workshop item gives the DLL that item's Workshop identity, allowing RML to show the correct art.

## Direct local installation

For development, copy `RedWolfRadio.dll` and `RedWolfRadio.ini` into RML's plugin directory. RML can load and configure the plugin there, but it will not associate a local DLL with an independent preview image. Do not overwrite RML's own `previewimage.png`; that image belongs to the loader's Workshop item and is shared by every plugin installed inside that item.

## Unpublished local artwork preview

RML also has a **Development** view for local WRSR projects under `<game>/media_soviet/workshop_wip/<numeric-project-id>/`. Its source requires `workshopconfig.ini` at that project's root, discovers the same `previewimage.png` and `plugin/` layout, and renders the project's thumbnail in the Development detail panel. This provides a way to inspect the artwork before public Workshop publication.

This is separate from the **Plugins** detail panel: `ensurePluginImage` only resolves a `WorkshopID`, not a `DevelopmentID`. Moving a DLL into a local development project does not by itself add an image to that plugin panel.

The behavior is verified in [`discoverDevelopmentMods` and `ensureDevelopmentImage`](https://github.com/Ultimate-Universe/WRSR-RepublicModLoader/blob/57e567e5cccaa3c38aee7e9d599d2443d2460b10/Source/launcher/development_windows.go). RML accepts a numeric local project ID without checking Steam registration. Such an ID is only for local testing: create a real Workshop item through the game's authoring workflow before uploading, and do not substitute RML's item ID.

Development DLLs use an independent inventory, so leaving both the local project and a loose copy enabled can load the plugin twice. Preserve the current INI and archive the loose DLL with a non-`.dll` extension before enabling the Development copy.

The Development panel offers **Open project folder** and **Open workshopconfig.ini**, not the ordinary plugin **Open settings** button. Edit `<project>/plugin/RedWolfRadio.ini` directly at this stage. The standard settings button and the Plugins detail image still need verification during the later actual Workshop installation.

## Steam listing versus launcher image

Upload the approved image as the Steam listing preview and include `previewimage.png` in the item contents. RML reads the image from the installed item folder; it does not need a manually configured image URL. A Steam listing image alone does not replace the packaged file.

The current packaging script stages the DLL, defaults, documentation, and artwork. It does not create `workshopconfig.ini`, register an item with Steam, or publish it. Complete and test that authoring step before treating the Workshop archive as a finished upload.

## First Workshop upload and local migration

Create an item through **Workshop → Your items (WIP) → Create new item** in WRSR. The current game's editor offers **Script** and **Unpublished**. Use the registered item folder and preserve its generated `workshopconfig.ini`; a temporary local numeric folder is not a registration. Populate the item with the clean Workshop package, leaving personal music, logs, caches, and settings backups outside the upload folder.

Before the in-game save/upload action, inspect the item editor's visibility label. Do not interpret the metadata's numeric `$VISIBILITY` using the Steam API enum. After uploading, verify the listing's actual visibility and add Republic Mod Loader as a required item in Steam's owner controls.

For the subscription test, disable all local Development copies in RML, subscribe to the item, wait for Steam to finish downloading, and refresh RML. Enable the downloaded plugin in its own Workshop folder; do not copy its DLL into the loader's shared plugin folder. Keep the authoring project disabled so it remains available for future uploads without being loaded alongside the subscribed DLL.

Back up the user's current `plugin/RedWolfRadio.ini` outside both the Workshop download folder and authoring folder. With the game closed, restore that INI beside the downloaded DLL. Back up any optional local Music files too. The `%LOCALAPPDATA%/RedWolfRadio` cache is shared across installations and can remain in place. Repeat the backup/restore around updates or force-refresh until automatic preservation has been established.

Item `3805202523` has a reviewed payload staged and an in-game **Unpublished** visibility check. See [VALIDATION.md](VALIDATION.md); this does not yet establish a successful DLL upload or subscribed installation.
