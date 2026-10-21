# Plan: 1.1 (DLC release)

1.1 follows 1.0 quickly. Branch `dlc/1.1` = main plus the DLC work; it is rebased whenever main moves.

## In 1.1

1. **DLC packs.** Launcher home page > INSTALL DLC (main's multi-game content installer,
   `crackdown/launcher/content.cpp`), unpacked once into the save folder and shared by every profile through
   SDK patch 0025 (`--dlc_root`). Four-Play Pack loads on the disc version.
2. **Title Update #6 build (`crackdown_tu/`).** Needed for the Free-For-All Pack (Keys to the City, Street
   Racing). Status and address table: `docs/plan-title-update.md`, `docs/tu-address-map.csv`.
   - Ported and PC-tested on the update: loading-screen hooks, frame pacing, 60 fps (stairs and driving
     replays, below), input replay, teleport, intro skip, PC menus/options, XAudio barrier, capture, co-op
     (60 fps lockstep), achievements (50), diagnostics.
   - Launcher: INSTALL TITLE UPDATE (Crackdown 1) unpacks the package into `tu\` only (never next to
     `default.xex`). Once `tu\default.xexp` and `crackdown_tu.exe` exist, "Play with the title update"
     (default on, `launcher.toml` `cd1_title_update`) makes PLAY start `crackdown_tu.exe
     --update_data_root=<install>\tu`. Archipelago always uses the disc exe.
   - Left for 1.2: the Archipelago bridge on the update (struct offsets moved, e.g. skills +9056 -> +7104).
3. **GitHub issues** (Austin, 2026-10-20):
   - #13: 21:9 in a 2560x1080 window is letterboxed into a ~1680-wide box. Cause: `--aspect_ratio` set a
     720-line width (1680) but the SDK takes the video mode height from `window_height` when
     `video_mode_height` holds its default, so the mode became 1680x1080 (14:9). Fix in `host_tuning.cpp`: size
     the mode from the window height. Checked windowed at 2560x1080 (mode 2520x1080, frame fills the window).
   - #16: stuck at ~33 fps on an RTX 5090 when the NVIDIA global profile forces V-Sync; the reporter traced it
     to an always-on overlay Present blocking the shared GPU queue. Workaround: per-exe profile with V-Sync off.
     Fixed by SDK patch 0027 (wait on the swap chain's frame-latency object before painting). Reproduced on the PC
     with `--d3d12_present_test_sync_interval=1`: 44.5 fps without the wait, 61.8 with it (62.0 without V-Sync).

## 60 fps checks on the update (2026-10-20, Release, same saves)

- Stairs, `stairs_tp.rec --teleport=1396.6,19.8,-1413.1 --diag_char_proxy`: peak 11.51 m/s (disc 30 fps 11.86,
  disc 60 11.96). Full walk `stairs.rec`: update 14.07, disc 14.05 (same landing).
- Driving, `car_tp.rec --teleport=1931.6,11.4,-1783.9 --diag_vehicle`: update 6.7 / 12.6 / 17.6 m/s at
  0.5 / 1 / 1.5 s, disc 60 7.3 / 13.1 / 18.0, disc 30 7.7 / 14.1 / 18.4. Same top speed (20.4) and wall hit.
  The update at 30 fps couldn't be measured with this recording (its menu inputs land on Co-op at 30 fps),
  so whether the extra ~4% is the update's own tuning or frame rate is open (1.2, with the 120/144 car work).
- Found and fixed on the way: an unresolved tail call at 0x8243A648 -> 0x8243A658 (REX_FATAL during gameplay
  on the update build) and the update's controller offsets for `--teleport`.

## 1.2

- 120/144 fps: NPCs sprinting too fast at 120 (another 30 fps timer); car acceleration falls further behind as
  the frame rate rises (5-7% at 60).
- Archipelago on the update build (carry 1.0.5's gated generals).
