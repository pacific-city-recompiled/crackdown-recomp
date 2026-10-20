# Plan: 1.1 (DLC release)

1.1 follows 1.0 quickly. Branch `dlc/1.1` = the 1.0 branch plus the DLC work; it is rebased whenever 1.0 moves.

## In 1.1

1. **DLC packs.** Launcher SAVES > Install DLC package (`crackdown/launcher/dlc.cpp`), shared by every profile
   through SDK patch 0025 (`--dlc_root`). Four-Play Pack loads on the disc version.
2. **Title Update #6 build (`crackdown_tu/`).** Needed for the Free-For-All Pack (Keys to the City, Street
   Racing). Status and address table: `docs/plan-title-update.md`, `docs/tu-address-map.csv`.
   - Ported and PC-tested on the update: loading-screen hooks, frame pacing, 60 fps, input replay, intro skip,
     PC menus/options, XAudio barrier, capture, co-op trace, diagnostics.
   - Ported, not yet run on the update: aspect ratio, camera turn speed, 1.0's 5-row PC Settings page
     (Scene.xpatch applies to the same Scene.xzp).
   - Left: the Archipelago bridge (struct offsets moved, e.g. skills +9056 -> +7104), launcher support for
     installing the update and choosing the update build, packaging.
3. **Carried over from 1.0** (Austin, 2026-10-18):
   - Achievements check (moved out of 1.0).
   - 120/144 fps, with the fixes they need: NPCs sprinting too fast at 120 (another 30 fps timer) and car
     acceleration.
   - The update mapping for 1.0's camera tuning block and PC Settings page (done on `dlc/1.1`; needs a run).

4. **GitHub issues** (Austin, 2026-10-20):
   - #13: 21:9 in a 2560x1080 window is letterboxed into a ~1680-wide box. Cause: `--aspect_ratio` set a
     720-line width (1680) but the SDK takes the video mode height from `window_height` when
     `video_mode_height` holds its default, so the mode became 1680x1080 (14:9). Fix in `host_tuning.cpp`: size
     the mode from the window height. Fullscreen at 21:9/32:9 still to check.
   - #16: stuck at ~33 fps on an RTX 5090 when the NVIDIA global profile forces V-Sync; the reporter traced it
     to an always-on overlay Present blocking the shared GPU queue. Workaround: per-exe profile with V-Sync off.
     Fixed by SDK patch 0027 (wait on the swap chain's frame-latency object before painting). Reproduced on the PC
     with `--d3d12_present_test_sync_interval=1`: 44.5 fps without the wait, 61.8 with it (62.0 without V-Sync).
     #13 fix checked windowed at 2560x1080 (mode 2520x1080, frame fills the window).

## Before release

- Build `dlc/1.1` with the full SDK patch stack (0001-0026) and re-test the disc build (Four-Play) and the update
  build (Keys to the City, Street Racing, 60 fps, menus).
- Launcher: detect/install the title update file, pick `crackdown_tu.exe` when it is present.
