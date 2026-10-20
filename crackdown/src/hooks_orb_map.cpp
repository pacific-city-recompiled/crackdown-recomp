// Orb markers on the pause-menu map (--map_orb_markers).
//
// The map screen XUIFE_S_M_cMap is built by its constructor sub_826400D8
// (vtable 0x820E6160). Every icon on it is a small XUR scene:
//   sub_8213BF98(str, L"vfs://...xur")  wide-string object (0x20 bytes, SSO)
//   sub_82692D20(str) -> icon           loads the scene; icon+4 is the hObj
//   sub_8213BFF8(str, 1, 0)             string destructor
// The player icon (sub_82640D20, "vfs://mapiconplayer.xur") is then placed
// with sub_82641510(map, icon, &world_pos, &heading): world x/z (pos+0/+8)
// go through the map's world-to-map transform sub_826412E8 (main and
// district views both), the hObj becomes a child of the map scene, and the
// icon is kept in the map's icon list map+0xC0, which the destructor
// sub_82640A50 walks to destroy every icon. We add one icon per orb the same
// way, after the native constructor has finished. The orb scenes and images
// are drawn by tools/xui/orb_icons.py and added to the patched Scene.xzp.
//
// Orbs (docs/re-discovery-randomizer.md): pickup manager [0x82DE3F34],
// pointers at +12, count at +4012; pickup +264 type (0-3 agility, 4 hidden),
// +272/+276/+280 world x/y/z, +288 = 1 while player 0 has not collected it.
//
// Disc XEX addresses. The title-update build keeps per-version addresses in
// game_addresses.h on its own branch; map these there when this lands on it.

#include "generated/default/crackdown_init.h"

#include <bit>
#include <chrono>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging/macros.h>

#include "ap/bridge.h"

REXCVAR_DEFINE_STRING(map_orb_markers, "auto", "Crackdown",
                      "Mark orbs you have not collected on the pause map: auto (all, on randomized "
                      "Archipelago saves only), off, agility, hidden or all. On a randomized save "
                      "only orbs that are checks are marked")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);
REXCVAR_DEFINE_STRING(map_orb_icon_agility, "vfs://mapiconorb_agility.xur", "Crackdown",
                      "Map scene used for agility orb markers")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);
REXCVAR_DEFINE_STRING(map_orb_icon_hidden, "vfs://mapiconorb_hidden.xur", "Crackdown",
                      "Map scene used for hidden orb markers")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

namespace {
constexpr uint32_t kPickupManagerGlobal = 0x82DE3F34;
constexpr uint32_t kMapVtable = 0x820E6160;
// The orb scenes live in the patched Scene.xzp (tools/xui/orb_icons.py); with
// --pc_menu_mod=false they are missing and the supply-point icons stand in.
constexpr std::string_view kFallbackAgility = "vfs://XUI_MainMap_SupplyPoint_Locked.xur";
constexpr std::string_view kFallbackHidden = "vfs://XUI_MainMap_SupplyPoint_Unlocked.xur";

PPCContext SideCall(const PPCContext& parent, uint8_t* base) {
  PPCContext call = parent;
  call.r1.u32 -= 256;
  REX_STORE_U32(call.r1.u32, parent.r1.u32);
  return call;
}

// Loads one icon scene; 0 when the scene or its hObj is missing.
uint32_t LoadIcon(const PPCContext& ctx, uint8_t* base, std::string_view path) {
  auto call = SideCall(ctx, base);
  const uint32_t text = call.r1.u32 + 0x60;  // up to 0x60 bytes: 47 UTF-16 units
  const uint32_t str = call.r1.u32 + 0xC0;   // 0x20-byte string object
  if (path.empty() || path.size() > 47) return 0;
  for (size_t i = 0; i <= path.size(); ++i)
    REX_STORE_U16(text + static_cast<uint32_t>(2 * i), i == path.size() ? 0 : static_cast<uint8_t>(path[i]));
  call.r3.u64 = str;
  call.r4.u64 = text;
  sub_8213BF98(call, base);
  call = SideCall(ctx, base);
  call.r3.u64 = str;
  sub_82692D20(call, base);
  const uint32_t icon = call.r3.u32;
  call = SideCall(ctx, base);
  call.r3.u64 = str;
  call.r4.s64 = 1;
  call.r5.s64 = 0;
  sub_8213BFF8(call, base);
  return icon && REX_LOAD_U32(icon + 4) ? icon : 0;
}

// District view (map+0x144 != 0) shows only the island picked by map+0x140,
// like the supply points in sub_826400D8 (0x82640598..0x826405E0): the
// district grid [[0x82DE44F4]+0x104] maps world x/z to a cell
// (sub_826A4E70, leaf, f1 = x, f2 = z) whose +0x54 is the cell's district,
// compared with [[0x82DE27F4] + 4 * (slot + 0x24)] for the map's district
// (slot 5, 4, 6, 2 for 0, 1, 2, 3 at 0x82640400). District 3 also takes
// cells with no district. Without this, other islands' orbs land far outside
// the map panel.
struct DistrictFilter {
  bool active = false;
  uint32_t grid = 0, current = 0, index = 0;
};

DistrictFilter MakeDistrictFilter(uint8_t* base, uint32_t map) {
  DistrictFilter f;
  if (!REX_LOAD_U8(map + 0x144)) return f;
  f.active = true;
  f.index = REX_LOAD_U32(map + 0x140);
  const uint32_t slot = f.index == 0 ? 5 : f.index == 1 ? 4 : f.index == 2 ? 6 : 2;
  const uint32_t districts = REX_LOAD_U32(0x82DE27F4);
  f.current = districts ? REX_LOAD_U32(districts + 4 * (slot + 0x24)) : 0;
  const uint32_t world = REX_LOAD_U32(0x82DE44F4);
  f.grid = world ? REX_LOAD_U32(world + 0x104) : 0;
  return f;
}

bool InDistrict(const PPCContext& ctx, uint8_t* base, const DistrictFilter& f, uint32_t pos) {
  if (!f.active) return true;
  if (!f.current) return false;
  uint32_t district = 0;
  if (f.grid) {
    auto call = SideCall(ctx, base);
    call.r3.u64 = f.grid;
    call.f1.f64 = std::bit_cast<float>(REX_LOAD_U32(pos));
    call.f2.f64 = std::bit_cast<float>(REX_LOAD_U32(pos + 8));
    sub_826A4E70(call, base);
    if (call.r3.u32) district = REX_LOAD_U32(call.r3.u32 + 0x54);
  }
  return district == f.current || (f.index == 3 && !district);
}

// Our icons per map object (game thread only). The map owns and destroys
// them (list map+0xC0); entries are re-checked against that list before use.
std::unordered_map<uint32_t, std::vector<uint32_t>> g_markers;

void AddOrbMarkers(const PPCContext& ctx, uint8_t* base, uint32_t map) {
  std::string mode = REXCVAR_GET(map_orb_markers);
  if (mode == "auto") mode = crackdown::ap::OrbIsCheck(0).has_value() ? "all" : "off";
  const bool agility = mode == "agility" || mode == "all";
  const bool hidden = mode == "hidden" || mode == "all";
  if (!agility && !hidden) return;
  if (!map || REX_LOAD_U32(map) != kMapVtable) return;
  const uint32_t manager = REX_LOAD_U32(kPickupManagerGlobal);
  const uint32_t count = manager ? REX_LOAD_U32(manager + 4012) : 0;
  if (!count || count > 1000) return;
  const auto started = std::chrono::steady_clock::now();
  std::string agility_icon = REXCVAR_GET(map_orb_icon_agility);
  std::string hidden_icon = REXCVAR_GET(map_orb_icon_hidden);
  const DistrictFilter district = MakeDistrictFilter(base, map);
  auto& placed = g_markers[map];
  placed.clear();
  int shown = 0, failed = 0, randomized = 0, elsewhere = 0;
  for (uint32_t i = 0; i < count; ++i) {
    const uint32_t pickup = REX_LOAD_U32(manager + 12 + 4 * i);
    if (!pickup || REX_LOAD_U8(pickup + 288) == 0) continue;  // collected
    const bool is_hidden = REX_LOAD_U32(pickup + 264) == 4;
    if (is_hidden ? !hidden : !agility) continue;
    if (const auto check = crackdown::ap::OrbIsCheck(i)) {
      ++randomized;
      if (!*check) continue;
    }
    if (!InDistrict(ctx, base, district, pickup + 272)) { ++elsewhere; continue; }
    std::string& path = is_hidden ? hidden_icon : agility_icon;
    uint32_t icon = LoadIcon(ctx, base, path);
    if (!icon) {
      const auto fallback = is_hidden ? kFallbackHidden : kFallbackAgility;
      if (path != fallback) {
        REXLOG_WARN("map orbs: cannot load {}; using {}", path, fallback);
        path = fallback;
        icon = LoadIcon(ctx, base, path);
      }
    }
    if (!icon) { ++failed; continue; }
    auto call = SideCall(ctx, base);
    const uint32_t heading = call.r1.u32 + 0xE0;
    REX_STORE_U32(heading, 0);  // 0.0f
    call.r3.u64 = map;
    call.r4.u64 = icon;
    call.r5.u64 = pickup + 272;
    call.r6.u64 = heading;
    sub_82641510(call, base);
    placed.push_back(icon);
    ++shown;
  }
  REXLOG_INFO("map orbs: {} marker(s) ({}; {}; {} on other islands; {}randomized save; {} failed) in {:.1f} ms",
              shown, mode, district.active ? "district " + std::to_string(district.index) : std::string("whole map"),
              elsewhere, randomized ? "" : "no ", failed,
              std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count());
}
// The map screen keeps the map object at +0x1C and its tab control at +0x70
// (sub_826243F8 returns the tab). Its tab handlers set every icon's Show
// with sub_826FAAF8(hObj, show): sub_8263BE88 (tab 2, "Boss Locations")
// hides the supply records (map+0xD4, 0x30 apart) and shows the gang icons
// (map+0xF4); sub_8263BCD0 (tab 1, "Supply Points") hides the gang icons.
// Orb markers follow the supply points: hidden on tab 2, shown otherwise.
void SyncOrbMarkers(const PPCContext& ctx, uint8_t* base, uint32_t screen) {
  const uint32_t map = screen ? REX_LOAD_U32(screen + 0x1C) : 0;
  const auto found = g_markers.find(map);
  if (found == g_markers.end() || found->second.empty() || REX_LOAD_U32(map) != kMapVtable) return;
  auto call = SideCall(ctx, base);
  call.r3.u64 = REX_LOAD_U32(screen + 0x70);
  sub_826243F8(call, base);
  const bool show = call.r3.u32 != 2;
  std::unordered_set<uint32_t> live;
  for (uint32_t it = REX_LOAD_U32(map + 0xC4), end = REX_LOAD_U32(map + 0xC8); it && it < end; it += 4)
    live.insert(REX_LOAD_U32(it));
  for (const uint32_t icon : found->second) {
    if (!live.contains(icon)) continue;
    call = SideCall(ctx, base);
    call.r3.u64 = REX_LOAD_U32(icon + 4);
    call.r4.u64 = show;
    sub_826FAAF8(call, base);
  }
}
}  // namespace

// sub_826400D8 ends "mr r3,r31" (the map) after placing supply points, gang
// icons and players (sub_82640D20 at 0x826409A8 / 0x826409CC).
REX_HOOK_RAW(sub_826400D8) {
  const uint32_t map = ctx.r3.u32;
  __imp__sub_826400D8(ctx, base);
  AddOrbMarkers(ctx, base, map);
}

REX_HOOK_RAW(sub_8263BCD0) {
  const uint32_t screen = ctx.r3.u32;
  __imp__sub_8263BCD0(ctx, base);
  const auto result = ctx.r3;
  SyncOrbMarkers(ctx, base, screen);
  ctx.r3 = result;
}

REX_HOOK_RAW(sub_8263BE88) {
  const uint32_t screen = ctx.r3.u32;
  __imp__sub_8263BE88(ctx, base);
  const auto result = ctx.r3;
  SyncOrbMarkers(ctx, base, screen);
  ctx.r3 = result;
}
