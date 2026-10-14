# Red Wolf Radio 1.0 acceptance record

The 1.0 feature set is accepted for release. External XWMA, the live player, and the final compact icon layout have passed their local gameplay checks. The user approved the `1.0.0-player2` layout and proceeding to publication. The historical build identifiers below distinguish those tests from the final `1.0.0` release artifact; they are not separate installation choices. Exact DLL hashes, automated results and retained logs are recorded in [VALIDATION.md](VALIDATION.md).

## Completed gameplay checks

The user's gameplay reports and recorded live checks establish the following:

- External XWMA playback, complete tracks and automatic transitions, game music volume, Next/Previous, pause and paused navigation.
- Source filenames without extensions, Cyrillic names, literal `%`, five-second notification timing, Russian notification label and hiding the notification.
- The player updates its song title automatically, and its Previous, Restart, Play/Pause and Next actions work. The button's Previous action goes back directly; keyboard Previous retains the restart-first behavior.
- Dragging the red title bar, reopening the Script action with only one player window, and closing/reopening the window without interrupting music.
- The earlier `1.0.0-player1` layout at 1440×1080 and normal resolution, including native windows covering the player and blocking its buttons.
- Reopening centers the window again; position persistence is not implemented.
- Normal shutdown and relaunch, corroborated by the completed `player1` traces and the latest retained successful process exit.
- The final `1.0.0-player2` refinement: a 360×128 UI-unit window, four icon buttons and action descriptions displayed in the title bar on hover. The user accepted the smaller presentation.

The icon refinement changes presentation without adding hooks or changing the playback/control implementation. Its automated UI tests cover English/Russian hover captions, the Play versus Pause icon, empty-title fallback, click targets, layout bounds, drag/close behavior and native ownership/input handling. These checks complement the user's visual acceptance; they do not substitute for audible playback evidence.

## XWMA coverage

The external fixture set includes copied game music with arbitrary ASCII and Cyrillic filenames and a subfolder. Copying those files outside the game exercises the custom-source path without changing the soundtrack. The validator supports RIFF XWMA/WMAudio2, mono/stereo, 16-bit decoded samples, 8–48 kHz; other WMA profiles are rejected. The existing game-file corpus contains 33 stereo/48 kHz containers. This establishes the supported path, not compatibility with every possible Workshop encoding. Additional independently encoded samples can extend coverage after release.

XWMA uses `direct native XWMA` or an `unchanged XWMA cache copy`; it does not generate a converted WAV. Existing WAV cache entries and the plugin's silent WAV may legitimately remain present.

## Release settings and package

The 1.0 defaults are:

```ini
[Display]
ShowTrackName=false
NativeSongPopup=true
Language=en
```

Existing explicit INI settings remain authoritative. In particular, `ShowTrackName=true` from an earlier test keeps the optional notification enabled. `Language=ru` selects Russian hover descriptions and messages; filenames retain their original spelling. Missing keys use the defaults above. Edit the existing section rather than adding a duplicate, save UTF-8, and restart the game after changes.

The complete package includes the companion script, icon and metadata registration described in [DISTRIBUTION.md](DISTRIBUTION.md). DLL and INI files alone provide playback and hotkeys but do not register the Script-menu player. Users should enable only one copy of Red Wolf Radio.

To open the player in a loaded republic, use **Settings → Other → Scripts** (the **0101** icon), select **Red Wolf Radio**, and confirm with the green checkmark.

## Publication handoff

Gameplay acceptance is complete. The remaining release work is to publish the final versioned package and documentation, then verify the Workshop/GitHub publication results and downloaded files. This record does not itself assert that an upload has completed. Keep personal music, customized paths, cache files, logs and local test fixtures outside the published package.

For later bug reports, retain `RedWolfRadio-logs` and the RML runtime report before another launch overwrites the latter. Check for failed commands, load timeouts, dropped events and loader hook failures. If a Script action displays `[RWR_NOW_PLAYING_V1]`, retain that message: the script ran but the native player bridge did not install. Distinguish a documented clean process exit from an absent process-exit trace tail.
