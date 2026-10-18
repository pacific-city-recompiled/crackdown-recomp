# 1.0 feature pass (2026-10-17)

Work lands on `claude/project-thread-30a058` (GitHub main plus 1.0 features) and merges by PR. DLC
and the title update are 1.1. Build 1.0 release candidates from GitHub main, not the PC's local main
(it carries unpushed DLC commits).

| # | Feature | State |
|---|---|---|
| 1 | Launcher: 4K internal resolution, FXAA, anisotropic filtering | passed PC check (0290803) |
| 2 | Ultrawide / other aspect ratios (`--aspect_ratio`, launcher "Aspect ratio") | Austin: ultrawide looked good (PC check 2) |
| 3 | FPS counter (F3) and fullscreen/window toggle (F11) | passed PC check (0290803) |
| 4 | Achievements overlay (F7) | works with icons; text is small at 1440p |
| 5 | Frame rates above 60 (launcher 120 / 144; host frame pacing above 60) | PC check 3 passed except car acceleration (5-7% under 60); launcher marks them experimental |

## Rules for PC checks

Use a separate worktree of this branch and an SDK with GitHub main's patch stack (0001-0024, 0026;
no 0025). Don't touch the shared main checkout, its `sdk/` or `patches/`, or the user's saves (replay
against a scratch profile copy). Keep local paths out of committed docs.

## PC check 1 (0290803): passed 2026-10-17

Builds and launcher tests (6/6) pass. Launcher saves and reads back `resolution_scale`,
`swap_post_effect`, `anisotropic_override`, and removes them at the defaults. In game: 4K renders
sharper at 61 fps (capped); FXAA softens edges 6-22%; 16x filtering looks like 4x at spawn (no steep
surfaces there); F3 shows "Guest: 61.6 FPS (16.24 ms)"; F11 goes fullscreen -> window -> fullscreen
while rendering. Logs normal. F7 lists achievements with icons ("3 / 43 unlocked"). Ultrawide lead
confirmed (docs/re-notes.md, 0x82BAA498).

Nits fixed after it: combo entries had a leading space; removing a setting left a blank line in
`crackdown.toml`; the RenderDoc capture key moved from F11 to F9 so F11 only toggles fullscreen.

## PC check 2: aspect ratio

1. Build Release `crackdown.exe` and `CrackdownLauncher.exe`; run the launcher tests.
2. Launcher GRAPHICS tab: "Aspect ratio" (16:9 default, 16:10, 21:9, 32:9, 4:3) saves
   `aspect_ratio = "21:9"` etc. and removes it at 16:9; the combo labels line up; removing a setting
   leaves no blank line.
3. Game, windowed, at `--aspect_ratio=21:9`, `32:9`, `4:3` and default: log shows
   `--video_mode_width=1680/2560/960 --video_mode_height=720 (aspect_ratio)`. Screenshots at spawn,
   on the title screen, the main menu, the pause menu and the HUD in the city. Check that the 3D view
   is Hor+ (same vertical view, wider sides, nothing stretched) and whether menus, HUD, the minimap
   and videos are stretched, offset or cut off. Also try `--aspect_ratio=21:9 --resolution_scale=2`.
4. F9 under RenderDoc only if RenderDoc is installed; otherwise skip.

## PC check 3: 120 and 144 fps

Austin's rule: every rate above 60 is checked for audio, stairs speed, physics and driving before it
ships. Run each item at stock 30 (`--vsync=true`), 60 (default), 120 (`--fps_cap=120`) and 144
(`--fps_cap=144`). Use the bench recordings in `crackdown/bench/` and compare 120/144 against 30 and
60, as in the 2026-10-12 60 fps checks (docs/re-notes.md "Player character", "Vehicles", "Director
VO timeout investigation").

1. Pacing: the log shows `fps_cap: guest frame cap 32 -> 1000 (host paces 120 fps)`. Spawn stand
   (`--input_replay=bench/stand60.rec`) with `--frame_log` + `tools/frametimes.py`: average near the
   cap (120.0 / 144.0, not 125 / 166), even frame times. 60 stays on the guest limiter (about 61.6).
2. Stairs: `bench/stairs_tp.rec --teleport=1396.6,19.8,-1413.1 --diag_char_proxy`, peak horizontal
   speed about 12 m/s at every rate (30 fps 11.86, 60 fps 11.96).
3. Driving: `bench/car_tp.rec --diag_vehicle`, speed at 0.5 / 1.0 / 1.5 s about 7.9 / 14.1 / 18.5 m/s
   (30 fps) at every rate, and the same wall hit. Agency stairs drive
   (`--teleport_vehicle=1396.6,19.8,-1420,0`): damage about 12%, no instant wreck.
4. Physics: `--diag_physics_dt` shows the Havok step following the frame time (about 0.008 at 120,
   0.007 at 144), not 0.016 or 0.033. Note any physics-object jitter or launched props in a city
   drive (`bench/drive60.rec`).
5. Audio: `bench/drive60.rec` completes. Director lines are not cut off (`VOA_Eng[656]` about
   5.3 s with `--diag_audio`), and music does not stop. Error set same as at 60.
6. Look for anything else that speeds up: the anim clock sub_82330888 already runs 2x at 60 (crowd /
   packed anim), and the cosmetic timer sub_823FF3B8. Note anything worse at 120 / 144.
7. Launcher: Frame rate offers 60 / 30 / 120 / 144; saving 120 writes `fps_cap = 120` and no
   `vsync`; 60 removes both.

### Result (2026-10-18, 01745c5, 23 runs)

| Check | 30 | 60 | 120 | 144 |
|---|---|---|---|---|
| Pacing, spawn stand (avg fps / 1% low ms) | 30.0 / 36.4 | 61.6 / 18.0 | 119.9 / 9.4 | 124.4 / 9.8 (game ceiling there; garage holds 144.0) |
| Stairs peak speed (m/s) | 11.86 | 11.85 | 11.83 | 12.02 |
| Car speed at 0.5 / 1.0 / 1.5 s (m/s) | 7.8 / 14.0 / 18.4 | 7.3 / 13.1 / 18.1 | 6.5 / 12.3 / 17.2 | 6.8 / 12.5 / 17.3 |
| Wall hit velocity change (m/s) | 18.6 | 20.8 | 17.6 | 18.4 |
| Havok step dt (s) | 0.033 | 0.016 | 0.008-0.009 | 0.007-0.008 |
| Drive replay audio | complete | complete | complete | complete |

Crash damage is about 12-15% per hard hit at every rate, with no instant wreck. No voice was stopped by the timeout at any
rate, and the error set is the same as at 60. Open: car acceleration is 5-7% under 60 (repeatable; top speed about 19 vs
20 m/s); crowd animation speed and prop jitter need a live play at 120 (the drive replay stays in the garage).

## PC check 4: in-game PC Settings menu

The in-game PC Settings page (Options, screen 26) now matches the launcher: Frame rate (60 / 30),
Internal resolution (720p / 1440p / 4K), Aspect ratio (16:9 / 21:9 / 32:9), Anti-aliasing (Off /
FXAA) and Display (Fullscreen / Windowed). Hooks touched: ctor 0x8266B928 and notify 0x8269C3C0
(both already in docs/re-notes.md); no new guest addresses.

1. Regenerate the menu patch from the game files: `python tools/xui/mod_menus.py <game folder>`
   (writes `patches/xui/Scene.xpatch` and `Scene.audit.json`). The audit must still show
   `original_matching_literal_windows: 0`. Commit and push both files to this branch.
2. Build Release `crackdown.exe`; run `pc_settings_tests` and the XUI tests
   (`python -m unittest tools.xui.test_edit_patch tools.xui.test_xui`).
3. Game, scratch profile: open Options > PC Settings. Screenshot the page. Check that all five rows
   and the Back button fit on screen, labels aren't cut off or overlapping, the 3-button rows are
   evenly spaced, and left/right moves along a row (wrapping) while up/down moves between rows.
4. Change each row once, back out, and check `crackdown.toml` gets the matching key (`vsync = true`,
   `resolution_scale = 3`, `aspect_ratio = "21:9"`, `swap_post_effect = "fxaa"`,
   `fullscreen = false`), and that choosing the first option removes it again. Reopen the page: the
   log line `pc settings: initialized screen 26 (selections ...)` and the highlighted buttons match
   the file. Set `aspect_ratio = "4:3"` in the file: that row then shows nothing selected.
5. Restart with a couple of changed settings and check they took effect (F3 resolution, aspect).

## PC task 5: camera turn speed (stick sensitivity)

Austin finds the right-stick camera very slow at every frame rate, with no way to change it. Our input
path passes the stick through unscaled (XInput and SDL drivers copy the raw axis; the only deadzone in
the SDK is the idle check in `state_merge.cpp`), so full deflection already reaches the game. A
sensitivity option therefore has to scale the game's own turn rate, not the stick value.

1. Find where the right stick turns the on-foot camera: from the XamInputGetState callers, follow
   `sThumbRX`/`sThumbRY` (XINPUT_STATE +0x0C/+0x0E) to their float conversion and into the camera
   update, and find the yaw/pitch rate (likely an .rdata float or a tuning field multiplied with the
   stick value and dt). Note whether driving and aiming use the same rate or their own.
2. Prove it: temporarily double the rate (patch the constant or hook the reader), replay a held right
   stick (`--input_replay` recording, record one if none exists), and measure how far the camera turns
   per second at 1x and 2x, at 30 and 60 fps. Screenshots or a camera-heading log line are enough.
3. Add `crackdown/src/hooks_camera.cpp` with a `camera_sensitivity` cvar (double, default 1.0,
   0.25-4.0) that scales the rate for on-foot camera (and aiming/driving if they share it), with
   disc addresses in `game_addresses.h` (TU column left for the DLC thread). Record the addresses and
   evidence in `docs/re-notes.md`. Commit and push to this branch; the launcher slider and the PC
   Settings row are added on top of it.
