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
