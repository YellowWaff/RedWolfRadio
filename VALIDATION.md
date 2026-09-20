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
- Hotkey remapping, disabled bindings, and focus gating have automated coverage; an explicit in-game remapping/focus test is still useful.
- Compatibility with other executable, engine, or RML builds is not assumed. A separate `temporary_track`/`road_traffic` startup conflict was isolated before these tests and was not a Red Wolf Radio failure.
