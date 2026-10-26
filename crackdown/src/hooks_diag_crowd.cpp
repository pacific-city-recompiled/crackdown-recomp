// crackdown - diagnostic: crowd pedestrian speed (--diag_crowd)
//
// sub_82338220 (called from sub_82343730) updates the ambient crowd: it loops
// over the [0x82E61D30+9036] pedestrians at [0x82E61D30+6024 + 4i]. Each one
// keeps its position at +128, a per-update step at +180 and a speed factor at
// +184; the move is (+184) * (+180) per call. The step is set from the walk
// speed +192 times the pooled 1/30 (0x820ED704), or, when bit 0 of +663 is
// set, the run speed +196 times 2/30 (0x82072048), both in the spawn
// sub_82330B68 and on a state change in the update (disassembly). Neither
// literal follows the frame time (fixed by --sim_crowd_30hz, hooks_framerate.cpp),
// so this logs how fast the crowd really moves: calls per second and the mean
// real-time speed of walking and running pedestrians, once per second. Disc
// build only (the update moved the members; see the hook).

#include "generated/default/crackdown_init.h"
#include "game_addresses.h"

#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <unordered_map>

#include <rex/cvar.h>
#include <rex/hook.h>

REXCVAR_DEFINE_BOOL(diag_crowd, false, "Crackdown",
                    "Diagnostic: log the crowd pedestrians' update rate and real-time walk/run "
                    "speed once per second (sub_82338220)");

#if !CRACKDOWN_TU  // disc addresses only (diagnostic)
namespace {
constexpr uint32_t kCrowdBase = 0x82E61D30;

struct Sample {
  float x, z;
};

void LogCrowd(uint8_t* base) {
  auto LoadF = [base](uint32_t ea) { return std::bit_cast<float>(REX_LOAD_U32(ea)); };
  using Clock = std::chrono::steady_clock;
  static Clock::time_point last{};
  static uint32_t calls = 0;
  static std::unordered_map<uint32_t, Sample> prev;
  ++calls;
  const Clock::time_point now = Clock::now();
  if (last != Clock::time_point{} && now - last < std::chrono::seconds(1)) return;
  const double secs = last == Clock::time_point{} ? 0.0 : std::chrono::duration<double>(now - last).count();
  const uint32_t count = REX_LOAD_U32(kCrowdBase + 9036);
  std::unordered_map<uint32_t, Sample> cur;
  double walk_sum = 0, run_sum = 0;
  int walk_n = 0, run_n = 0;
  float step_walk = 0, step_run = 0;
  for (uint32_t i = 0; i < count && i < 512; ++i) {
    const uint32_t ped = REX_LOAD_U32(kCrowdBase + 6024 + 4 * i);
    if (!ped) continue;
    const Sample s{LoadF(ped + 128), LoadF(ped + 136)};
    cur[ped] = s;
    const bool run = REX_LOAD_U8(ped + 663) & 1;
    (run ? step_run : step_walk) = LoadF(ped + 180);
    auto it = prev.find(ped);
    if (secs <= 0 || it == prev.end()) continue;
    const double d = std::hypot(s.x - it->second.x, s.z - it->second.z);
    if (d < 0.05 || d > 40.0) continue;  // standing, or respawned elsewhere
    (run ? run_sum : walk_sum) += d / secs;
    ++(run ? run_n : walk_n);
  }
  if (secs > 0) {
    REXLOG_INFO("diag_crowd: {:.1f} calls/s, {} peds, walk {:.2f} m/s (n={}, step {:.4f}), "
                "run {:.2f} m/s (n={}, step {:.4f})",
                calls / secs, count, walk_n ? walk_sum / walk_n : 0.0, walk_n, step_walk,
                run_n ? run_sum / run_n : 0.0, run_n, step_run);
  }
  prev.swap(cur);
  calls = 0;
  last = now;
}
}  // namespace

#endif

// Called at the top of the crowd update hook (hooks_framerate.cpp).
void CrackdownLogCrowd(uint8_t* base) {
#if !CRACKDOWN_TU
  if (REXCVAR_GET(diag_crowd)) LogCrowd(base);
#else
  (void)base;
#endif
}
