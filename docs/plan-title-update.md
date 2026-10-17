# Plan: port to Title Update #6 (Free-For-All / Keys to the City)

Decision (Austin, 2026-10-17): ship 1.0 on the disc XEX and start the title update port alongside it.

## What the update is

- `TU_16L61US_0000004000000.00000000001G1` (user-supplied, gitignored in `DLC/TU/`): STFS LIVE, type 000B0000,
  media 596F9615. Contents: `Default.xexp` + `Local/<lang>/GameStrings_DLC.str`.
- The SDK applies a sibling `Default.xexp` at load, in codegen and at runtime: 0.0.0.1 -> 0.0.6.1, XEX filetime
  2009-10-09. The patched image has `Digest2.txt` and `Cheat Mode` (the disc image has neither).
- It is a full rebuild: no function keeps its address. Sections shift (`.text` 0x82130000 -> 0x82140000, size
  0xA37FA4 -> 0xAB264C; `.data` 0x82B80000 -> 0x82C10000).

## Reproduce the probe (all output under gitignored `out/tu-probe/`)

1. `python -I tools/re/stfs_extract.py DLC/TU/<file> out/tu-probe/stfs`; copy `game/Default.xex` and
   `stfs/Default.xexp` into `out/tu-probe/game/`.
2. Copy `crackdown/crackdown_manifest.toml` into `out/tu-probe/proj/` with
   `includes = ["config/tu_functions.toml"]`, where that file holds the two thunk starts codegen asks for:
   `0x824D2920`, `0x82AA6D90`. `rexglue codegen` then succeeds (42558 functions; disc has 42295).
3. Image for other tools: run the disc exe with `--game_data_root=out/tu-probe/game --fps_cap=0 --vsync=true
   --dump_image=...` (the default `fps_cap` write to 0x82BAA330 crashes on the update image).
4. `tools/re/tu_match.py` (usage line at the top) matches functions and maps every guest address our sources use.

## Matching results (2026-10-17)

- Functions: 23844 of 42295 matched (56%): exact instruction text with addresses masked 9689, same text with all
  numbers masked +11532, opcode sequence +16, call-graph propagation +2607.
- Our 211 guest addresses: code 126 mapped, 9 in a matched function of different length, 42 in unmatched
  functions; data 20 mapped, 14 not (12 of them are import thunks at 0x82B67xxx, which map by import name).
- Confirmed by eye: 0x827457E8 -> 0x827CECB8 (storage hook). Data 0x82BAA330 (frame cap) -> 0x82C3A9A4.
- Not trustworthy yet: tiny functions. 0x82A4FA70 (events hook, `li r11,0; stw r11,4(r3); blr`) mapped to
  0x821512C0 through the call graph, but that body is common; check its callers before using it.

### Second pass (2026-10-17): `docs/tu-address-map.csv`

- Matcher now adds a neighbor + fuzzy-diff pass (unmatched functions between two matched neighbors, best diff
  ratio >= 0.6) and maps offsets inside changed functions by alignment: 31024 of 42295 functions (73%).
- Every hook address is in `docs/tu-address-map.csv` with its evidence. 147 high (function diff ratio >= 0.85 and
  a mapped caller agrees, or an independent check), 22 data by reference votes, 4 manual, 13 review, 8 open.
- Independent check: the frame-ms `li rX,33` pattern finds 11 sites on the disc and 12 in the update. All 11
  disc sites map to update sites that the matcher found too. The 12th (0x82559004) is new in the update.
- Struct layouts changed, not just addresses: the character skill block moved from +9056 to +7104 (getter
  0x822C1B10, slot 305 of the character vtable now at 0x8208FBE8), and the pickup manager field read after
  the save call moved from +60 to +64. Every struct offset in `crackdown/src/ap/` must be re-derived.
- The update adds two more pickup-manager load calls next to the disc's one (0x825B86F0, 0x825B87A0;
  the disc's is 0x825B8748), likely for the DLC.
- Open: the intro/XZP hooks 0x8261F740, 0x8261FC80, 0x82621990. The scene and VFS loaders were rewritten (best
  candidates 0x826938E8 at ratio 0.32, 0x82694530 at 0.41), likely for `Scene_CommonDLC.xzp`. Data globals
  0x82C63578, 0x82CFE4B4, 0x82D99228 (frame timing) and two diagnostics addresses still need mapping by use.

## REA

REA cannot analyze this binary on this PC: its Windows Ghidra lane only takes native PE and raw 6502, and it
rejects PowerPC PE (COFF machine 0x1F2) and PowerPC ELF. Its Ghidra provider is also unconfigured here
(`GHIDRA_INSTALL_DIR` unset, it picks the 32-bit Java 8). Ghidra 12.1.4 itself is at
`C:\Users\unowi\OneDrive\Desktop\ghidra_12.1.4_PUBLIC` with JDK 21 at `C:\Program Files\Java\jdk-21.0.12.1`;
its Version Tracking could cross-check matches, but it has no VMX128, and our codegen disassembly does.

## Next steps

1. Review the 51 unresolved or doubtful code addresses by hand (callers, strings, constants), smallest first.
2. Map import thunks by name; map the remaining data addresses from the matched functions that use them.
3. Port the codegen config: thunk starts (functions.toml), gapfill (re-run the gap scan from the playbook on the
   update), midasm hooks.
4. Build a second target from the update (separate generated dir; the launcher picks it when `Default.xexp` is
   installed), then rerun the New Game, 60 fps and audio checks and the Free-For-All pack.
5. Keep every new hook's disc and update addresses side by side so 1.0 changes carry over.
