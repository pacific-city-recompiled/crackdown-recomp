<div align="center">

# Crackdown Recomp

**Crackdown (Xbox 360, 2007), recompiled to run natively on Windows PC.**

60 fps · up to 4K · ultrawide · mouse and keyboard · co-op · Archipelago randomizer

[![Latest release](https://img.shields.io/github/v/release/Unowa001/crackdown-recomp?label=release&color=2ea44f)](https://github.com/Unowa001/crackdown-recomp/releases/latest)
[![Downloads](https://img.shields.io/github/downloads/Unowa001/crackdown-recomp/total?color=blue)](https://github.com/Unowa001/crackdown-recomp/releases)
[![License](https://img.shields.io/badge/license-BSD--3--Clause-lightgrey)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%2010%20%7C%2011-0078d4)](SETUP.md#what-you-need)

**[Download 1.0](https://github.com/Unowa001/crackdown-recomp/releases/latest)** ·
**[Setup guide](SETUP.md)** ·
**[Report a problem](SETUP.md#reporting-a-problem)**

<!-- HERO -->

</div>

> [!IMPORTANT]
> **Crackdown 2 is coming to the same launcher.** Support for Crackdown 2 is in development
> and will be added to this launcher in a future update, so one install will play both games.
> Watch the repository (Watch → Custom → Releases) to hear when it lands.

## About

Crackdown Recomp is an unofficial native PC port of Crackdown. It is not an emulator: the
game's original Xbox 360 code is translated ahead of time (static recompilation) into a
native Windows program, then fixed and extended for modern PCs. A launcher installs the game
from **your own** disc dump, checks every file, keeps your settings and updates itself.

The repository contains no game data. You need your own copy of Crackdown.

## Features in 1.0

| | |
|---|---|
| **60 fps by default** | Smooth gameplay with the game's physics, driving, stairs and audio fixed to match the original 30 fps timing. 30 fps is one click away. |
| **Fast** | The renderer's work is split across threads and repeated per-draw work is skipped. On an RTX 5080 at 1080p uncapped, the spawn point went from about 65 to 110 fps and camera pans from about 80 to 130+. |
| **Graphics options** | Internal resolution up to 4K, FXAA and texture filtering, in the launcher's GRAPHICS tab or the new **PC Settings** page in the game's own menus. |
| **Ultrawide** | Aspect ratios 16:9, 16:10, 21:9, 32:9 and 4:3. Wider ratios show more of Pacific City instead of stretching the picture. |
| **Mouse and keyboard** | Full mouse look and rebindable keys, plus any Xbox-style controller. A camera turn speed slider (0.25x to 4x) works for both. |
| **Co-op** | Two-player campaign over System Link between two PCs, set up from the launcher (Host or Join). |
| **Archipelago randomizer** | Join an [Archipelago](https://archipelago.gg) multiworld: orbs, supply points, weapon deposits and bosses hold items for every player in the room. |
| **10 languages** | Pick the game's language in the launcher (the disc may not include all of them). |
| **Save profiles and portable mode** | Separate save profiles, or keep saves next to the game on a USB drive. |
| **Quality of life** | F3 fps counter, F11 fullscreen/window toggle, skippable boot logos, one-click updates that keep your saves and settings. |

<!-- GALLERY -->

## Get started

1. **Download** `CrackdownRecomp-1.0.0.zip` from the
   [latest release](https://github.com/Unowa001/crackdown-recomp/releases/latest) and extract
   the whole ZIP into a folder you can write to (for example `Documents\CrackdownRecomp`).
2. **Run** `CrackdownLauncher.exe` and choose **Install game**. Select your Crackdown ISO
   (or a folder of extracted files). The launcher extracts and verifies it.
3. **Press Play.**

The full guide, including Windows warnings, mouse and keyboard controls, co-op and the
randomizer, is in **[SETUP.md](SETUP.md)**.

### Requirements

| | |
|---|---|
| OS | Windows 10 or 11, 64-bit. Steam Deck and Linux players report it runs under Proton ([notes](SETUP.md#what-you-need)). |
| CPU | x64 with AVX2 and FMA3 (most Intel since 2013, AMD since 2015) |
| GPU | DirectX 12 |
| Runtime | [Microsoft Visual C++ x64 Redistributable](https://aka.ms/vs/17/release/vc_redist.x64.exe) |
| Game | Your own dump of the **Crackdown World multi-language disc** (ISO, XISO or extracted folder) |

### Controls at a glance

| Key | Action |
|---|---|
| F3 | Show or hide the fps counter |
| F11 | Switch between fullscreen and a window |
| W A S D, mouse | Move and look (turn on Mouse / keyboard mode in the launcher's CONTROLS tab) |

Full key map: [SETUP.md → Mouse and keyboard](SETUP.md#mouse-and-keyboard).

## Known issues

- Windows may refuse to start it with **`0xc0e90002`** ("This app can't run on your PC").
  That is Smart App Control blocking unsigned programs; see
  [SETUP.md → Windows warnings](SETUP.md#windows-warnings-expected).
- Co-op runs at 30 fps only.

## Roadmap

- **1.1:** DLC support with the title update (the Four-Play Pack, and the Free-For-All Pack
  with Keys to the City and Street Racing), 120 and 144 fps, and an achievements check.
- **Crackdown 2:** support in the same launcher.
- Native Linux: requested and tracked in
  [#5](https://github.com/Unowa001/crackdown-recomp/issues/5), no plan or date yet. Proton works today.

## For developers

| Path | What's there |
|---|---|
| `crackdown/src` | Game hooks and runtime fixes on top of the [ReXGlue SDK](https://github.com/rexglue/rexglue-sdk) (submodule `sdk/`, pinned to a release) |
| `patches/sdk` | SDK fixes, applied by `build-and-run.bat` / `tools/apply-sdk-patches.sh` |
| `crackdown/launcher` | The launcher and installer |
| `docs/` | Build playbook ([`docs/playbook.md`](docs/playbook.md)), reverse-engineering notes ([`docs/re-notes.md`](docs/re-notes.md)), the [60 fps audit](docs/60fps-audit.md) and design plans |

The recompiled C++ is generated locally from your own `default.xex`; neither it nor any game
file is committed. Building from source is covered in the playbook.

## Legal

Crackdown Recomp is an unofficial fan project. It is not affiliated with or endorsed by
Microsoft, Xbox Game Studios or Realtime Worlds. Crackdown and all related names and assets
belong to their owners. No game data is distributed; you must own the game and supply your
own disc dump.

The source code is released under the BSD-3-Clause license, see [LICENSE](LICENSE).
Third-party code keeps its own notices (`tools/launcher-NOTICES.txt`, `sdk/LICENSE`).
