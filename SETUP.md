# Setup

## What you need

- Windows 10 or 11, 64-bit.
- **Linux / Steam Deck:** there's no native Linux build yet, but players report the Windows
  build runs under Proton (add `CrackdownLauncher.exe` to Steam as a non-Steam game and force
  a Proton version) or through launchers such as Faugus. If it doesn't start, run it once with
  the launch option `PROTON_LOG=1 %command%` and attach the `steam-<id>.log` from your home
  folder to a [GitLab issue](https://gitlab.com/pacific-city-recompiled/crackdown-recomp/-/issues).
- A CPU with AVX2 and FMA3 (most Intel since 2013 / AMD since 2015) and a DirectX 12 GPU.
- The [Microsoft Visual C++ x64 Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe).
- Your own dump of the **Crackdown World multi-language disc** (an ISO, full XGD2 or trimmed
  XISO, or a folder of already-extracted files). The launcher checks every file against the
  supported version (`default.xex` SHA256 `7aaa7a8920d1c3c737e58cf583debafe6013be0a96bec3ec136e5de7e3377e5a`).

## Install

1. Download `CrackdownRecomp-<version>.zip` from the
   [latest release](https://gitlab.com/pacific-city-recompiled/crackdown-recomp/-/releases).
2. Extract the **whole** ZIP into a folder you can write to, e.g. `Documents\CrackdownRecomp`.
   Don't use `Program Files`, and don't run it from inside the ZIP.
3. Run `CrackdownLauncher.exe`.
4. Choose **Install game** and select your ISO (or a folder with the extracted game).
   The launcher extracts it into `game\` next to itself and checks every file. This takes a
   minute or two. An existing folder you select is used in place and never modified.
5. Press **Play**.

### Windows warnings (expected)

The files are not code-signed, so Windows may warn you:

- **SmartScreen** ("Windows protected your PC"): click **More info → Run anyway**.
- **Microsoft Defender** has flagged earlier builds of the launcher as
  `Trojan:Win32/Bearfoos.A!ml`. This is a machine-learning ("!ml") heuristic match on an
  unsigned new program; we believe it is a false positive. If it happens:
  open **Windows Security → Virus & threat protection → Protection history**, select the
  entry and choose **Restore** (or **Allow on device**). If you are unsure, don't run it, and
  tell us instead.
- **Known issue: "This app can't run on your PC" / "contains an error", status
  `0xc0e90002`.** This is Windows **Smart App Control**, which blocks unsigned programs and
  has no per-app exception. The recomp isn't code-signed, so the only way past it is
  turning Smart App Control off (**Windows Security → App & browser control → Smart App
  Control settings**). That lowers protection for every app, so it's your call.

## Playing

- Controller recommended (Xbox-style controllers work out of the box). For mouse and
  keyboard, see below.
- 60 fps is the default. In the launcher's settings, **30 fps** restores the original timing.
  Co-op runs at 60 fps too; both players should pick the same frame rate.
- **Save profiles** in the launcher keep separate saves; a new profile starts a fresh game.
- Saves live in `Documents\crackdown`; logs are in the `logs\` folder next to the game. For
  **portable mode**, tick **Keep saves with the game** in the launcher's SAVES tab: the first
  time, it copies your saves into `userdata\` next to the launcher, then uses them from there
  (a `portable.txt` file marks it). Turning it off goes back to `Documents\crackdown` without
  copying newer portable saves back; nothing is deleted either way.
- The boot logo videos are skipped by default (launcher setting).
- **Graphics** (launcher → GRAPHICS, or the PC Settings page in the game's menus): internal
  resolution up to 4K, FXAA, texture filtering, and **Aspect ratio** (16:9, 16:10, 21:9,
  32:9, 4:3). Wider ratios show more of the scene instead of stretching it.
- **Camera turn speed:** launcher → CONTROLS slider (0.25x to 4x, also scales mouse look).
- **Keys:** F3 shows an fps counter, F11 switches between fullscreen and a window.
- **Language:** launcher → GAMEPLAY → **Language** (or `user_language` in `crackdown.toml`:
  1 English, 2 Japanese, 3 German, 4 French, 5 Spanish, 6 Italian, 7 Korean,
  8 Traditional Chinese, 9 Portuguese, 10 Simplified Chinese; the game ignores other values
  and uses English). Takes effect on the next Play. French is confirmed; the disc may not
  include every other language.

### Mouse and keyboard

Turn on **Mouse / keyboard mode** in the launcher's CONTROLS tab (it also has a mouse
sensitivity slider). The keyboard stands in for a controller:

| Controller | Keys |
|---|---|
| Left stick (move) | W A S D |
| Right stick (camera) | Mouse, or the arrow keys |
| A (jump) | Space (or ;) |
| B | Backspace (or ') |
| X / Y | L / P |
| Left trigger (lock on) / right trigger (fire) | Right-click, Q or I / left-click, E or O |
| Left / right bumper | 1 / 3 |
| Start | Enter or X |
| Back | Tab or Z |
| D-pad | Shift + arrow keys |
| Left / right stick click | F / K |

Every key can be rebound in `crackdown.toml` (`keybind_a`, `keybind_start`,
`keybind_left_shoulder`, `keybind_lstick_press`, ...). `mnk_mouse = false` turns mouse look off.

## DLC and the title update

Both are optional and come from your own Xbox 360 (or a backup of its `Content` folder):

1. **Install DLC** on the launcher's home page takes a DLC package from
   `Content\0000000000000000\4D5307DC\00000002` (keep its original file name). It is unpacked
   once into your save folder and every save profile can use it. The **Four-Play Pack** works
   with the original game.
2. The **Free-For-All Pack** (Keys to the City, Street Racing) needs **Title Update #6**:
   **Install title update** takes `TU_16L61US_0000004000000.00000000001G1` from
   `Content\0000000000000000\4D5307DC\000B0000`. It goes into `tu\` next to the launcher;
   your game files are not changed.
3. With the update installed, **Play with the title update** (home page, on by default) starts
   the title update build. Untick it to play the original version. Archipelago always uses the
   original version.

## Updates

The first time you open the launcher it asks whether to check for updates. You can also
use **Check for updates** at any time. Updates download from this project's
[GitLab releases](https://gitlab.com/pacific-city-recompiled/crackdown-recomp/-/releases),
are verified (SHA256) and replace only the program files. Your game files, saves and
settings are kept, and a failed update rolls back.

> [!NOTE]
> **Launcher 1.2.1 and older can't update any more.** They checked GitHub, where this project
> no longer publishes. Download `CrackdownRecomp-1.2.2.zip` (or newer) from the
> [releases page](https://gitlab.com/pacific-city-recompiled/crackdown-recomp/-/releases) once
> and copy everything inside its `CrackdownRecomp-1.2.2` folder over your install folder (the
> package has only program files: your game files, saves and settings stay). From 1.2.2
> on, Crackdown and Crackdown 2 update from GitLab in the launcher again.

## Crackdown 2

The launcher also runs [Crackdown 2 Recomp](https://gitlab.com/pacific-city-recompiled/crackdown-2-recomp) (beta).
Pick **CRACKDOWN 2** in the header. It lives in the `cd2` folder next to the launcher, with its own
settings, saves (`Documents\crackdown2`) and logs:

1. **Download runtime** installs the latest Crackdown 2 release from its repository (pre-releases
   included). **Check for updates** on that tab updates it later; Crackdown 1 keeps updating from
   this repository.
2. **Install game** takes your Crackdown 2 World multi-language disc image or extracted folder.
3. **Install title update** takes the title update 0.0.5.1 package (`TU_16L625S_...`, from a
   console's `Content\0000000000000000\4D5308BC\000B0000`). Crackdown 2 needs it to run.
4. **Install DLC** (optional) takes Toy Box, Deluge or Pre-Order Armor packages (from
   `...\4D5308BC\00000002`, original file names). They are shared by every save profile.

## Reporting a problem

In the launcher, use **Open logs folder** and **Copy system info**. Send:
the newest `crackdown_NNN.log`, the copied system info, `VERSION.txt` from the install
folder, what you were doing, and whether it happens again.

## Archipelago randomizer (experimental)

Crackdown can join an [Archipelago](https://archipelago.gg) multiworld: your orbs, supply
points, weapon deposits and bosses hold items for every player in the room, and other
players' games send you Crackdown's skills and weapons. It needs **Archipelago 0.6.8 or newer**.

**What's randomized:** 500 agility orbs, 300 hidden orbs (optional), 15 supply points,
19 weapon deposits and 21 bosses are checks. Items are Progressive skills (Agility,
Strength, Firearms, Explosives, Driving), the weapons, and Agency Commendations (filler).
**Goal:** defeat the three gang kingpins in your own game.

### Make a player file

1. Download `crackdown.apworld` from the release and double-click it (or copy it into
   Archipelago's `custom_worlds` folder).
2. Open the **Options Creator** from the Archipelago Launcher, pick Crackdown and export your
   YAML. (Or edit `Crackdown.yaml` from the release.) Set `name` to your slot name.

### Generate and host the room

Whoever generates the seed needs `crackdown.apworld` installed. Put everyone's YAMLs in
Archipelago's `Players` folder and run **Generate**. Host the zip from `output`:

- **archipelago.gg:** open [archipelago.gg/uploads](https://archipelago.gg/uploads), upload the
  zip and create a room. The room page shows the server address, e.g. `archipelago.gg:38281`.
- **Self-host:** run `ArchipelagoServer` with the zip (port 38281 by default; other players
  need that port forwarded to you).

### Connect the game

1. In the launcher, **create a new save profile for this seed** (SAVES tab). A profile's save
   is bound to the first seed it connects to; other seeds are refused, so use one profile per seed.
2. Open the **ARCHIPELAGO** tab, enter the server address, your slot name and the room
   password (if any), and press **Play**. The game connects by itself; there is no separate
   client window. The password is kept in the profile, not on the command line.
3. Optional: in Archipelago's `host.yaml`, set `crackdown_options.launcher_path` to your
   `CrackdownLauncher.exe`. The **Crackdown Client** entry in the Archipelago Launcher, and
   `archipelago://` links on room pages, then open the launcher with the room filled in
   (press **Use this room link for this profile** to apply it).

This is the first multiworld release; please report anything odd with the steps under
"Reporting a problem", plus the room/seed if you can.

## Co-op (experimental)

Two players can play the Campaign together over **System Link**, each on their own PC
with their own copy of the game and the **same release version**. Co-op runs at 60 fps
(both games step in lockstep); both players should use the same frame rate setting and the
same build (both with or both without the title update).

So far this has only been tested with two copies of the game on one PC. Please tell us how
it goes between two PCs.

### 1. Connect the two PCs

- **Same home network:** nothing to install. Find each PC's IPv4 address (Settings →
  Network → your connection, or `ipconfig` in a command prompt), e.g. `192.168.1.20`.
- **Over the internet:** install [Tailscale](https://tailscale.com/download) (free) on both
  PCs and put them on the same tailnet (one person can share their device with the other).
  Each PC then has a `100.x.y.z` address, shown in the Tailscale menu. This avoids port
  forwarding.
- Both PCs must reach each other on **UDP ports 1000-1001 and TCP port 1002**. When Windows
  Firewall asks about `crackdown.exe`, allow it. If it never asks and nobody can connect,
  allow `crackdown.exe` in **Windows Security → Firewall → Allow an app**.

### 2. Tell each game where the other player is

In the launcher, open **SETTINGS → GAMEPLAY**, tick **Play co-op with another PC** and fill in:

- **Role:** one player picks **Player 1 (hosts)**, the other **Player 2 (joins)**.
- **Your address:** this PC's address (the buttons next to it list the ones the launcher found).
- **Other player's address:** your partner's address.

Then **Save settings**. Untick the box again to play solo with your normal settings.
Without these settings **Play Now - Co-op** stays greyed out in the game.

Player 2 plays co-op with a separate save, which starts fresh. To keep an existing save,
copy the `B13EBABEBABEBABE` folder in `Documents\crackdown` to a new folder named
`B100000000000002` before playing.

The launcher writes these lines to `crackdown.toml` next to `CrackdownLauncher.exe`; you can
also add them by hand. Player 1:

```toml
net_mode = "direct"
net_bind_ip = "100.101.102.1"            # your own address
net_peer_endpoints = "100.101.102.2:1001" # your partner's address, then :1001
coop_profile_name = "Player1"
```

Player 2:

```toml
net_mode = "direct"
net_bind_ip = "100.101.102.2"
net_peer_endpoints = "100.101.102.1:1001"
coop_profile_xuid = "12754194144713244674"  # must differ from player 1
coop_profile_name = "Player2"
```

### 3. Play

1. Both: **Play**, then **Campaign → Play Now - Co-op → Host/Join System Link Game**.
2. Player 1: **Host A Game**. It shows "Waiting for Other Player".
3. Player 2: **Join Another Player**. Player 1's name appears under Match Selection; press **A**.
4. Player 1: **Launch Game**. Both pick a supply point and drop in.

If either side shows "Connection to other player lost", send both players' newest
`crackdown_NNN.log` (see "Reporting a problem").
