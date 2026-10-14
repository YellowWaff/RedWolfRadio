# Small icon player prototype

`1.0.0-player2` refines the accepted live player. It keeps one movable window open and refreshes the source song name automatically. The frame is 360 by 128 UI units, about half the area of the 500 by 180 text-button version. Closing the window does not stop music.

The four icon buttons, from left to right, are **Previous**, **Restart**, **Play/Pause**, and **Next**. Hover to see the action name in the title bar. Previous goes directly to the preceding history entry; Restart starts the current song again. The existing keyboard Previous still restarts on one press and goes back on a quick second press. Navigation while paused remains paused. The icons are drawn from the game's existing panel primitives rather than unverified font symbols.

## Focused layout test (about 5 minutes)

The preceding live-player tests already passed. Focus on whether the smaller frame is comfortable to leave open, all four icons are readable and their hover captions match their actions, long names fit two lines, dragging works with the shorter title bar, and 1440x1080 still fits. Test one pause/resume and next/previous sequence, including navigation while paused; then exit normally. The broader checklist below remains a reference if any regression appears.

1. Launch through Steam and Republic Mod Loader with the existing Default playlist and local development radio enabled. Enter a republic and open **Red Wolf Radio: Now playing** from the Script menu.
2. Leave it open across a song ending and use Next. The name should update by itself. With the current two-track XWMA folder, expect **RWR Test 100%** or **Проверка музыки**. No extension or cache hash should appear. Percent signs are now printed literally through the engine's non-formatting text API.
3. Try all four buttons. Pause should change to Play; Restart should repeat the current song; Previous should immediately return to the preceding history entry. Check Next and Previous while paused, then resume.
4. Use the configured keyboard shortcuts while the window remains open. The name and state should follow those changes too.
5. Drag the red title bar. Try near screen edges. Check that clicks inside the player do not select or build on the map beneath it.
6. Select the Script action again while the player is already open. There should still be one radio window. Close with X and reopen; confirm music was uninterrupted.
7. Open an ordinary game information window over the player. Its controls should work normally; radio buttons underneath must not respond. Bring the player forward and try again.
8. Check readability at 1440×1080 and your usual resolution. Restore your preferred resolution afterward. Long names wrap to two lines and then truncate with an ellipsis.
9. Exit normally and relaunch. Reopen the player once from the Script menu. Position is not saved between sessions in this prototype.

If the fallback message includes `[RWR_NOW_PLAYING_V1]`, the script ran but the optional player bridge did not install. Report that exact message. Do not enable another radio copy to fix it.

`[Display] NativeSongPopup=false` disables the prototype on restart. `ShowTrackName` remains independent. The installer preserves the current music path and loader choices, updates only the five prototype-owned paths under local development item `900001`, and saves a hash-checked rollback script under `evidence/install-native-popup-<timestamp>`. It does not update the subscribed public release.
