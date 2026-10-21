// crackdown - experimental variable simulation step (--sim_variable_step,
// --sim_game_clock)
//
// Guest 0x826A7698 runs one frame of the game loop (disassembly): it calls the
// update sub_826A6B50 with dt = [0x82CFE4B4] * [0x820ED704], where 0x820ED704
// is the float 1/30 (the only copy in the image), then renders, runs the frame
// limiter (Sleep((1000 / [0x82BAA330]) - elapsed)) and returns. The step query
// sub_8254C568 multiplies by the same constant in 30 Hz mode, and about a
// dozen other sites use it to turn frame counts into seconds.
//
// So the step is a fixed amount per frame. PC tests (2026-10-11): uncapped at
// 60 fps the game ran at 2x speed; with the constant set to 1/60 (--sim_hz=60)
// it ran at the right speed while it held 60 fps, and slowed down in busy
// scenes where the frame rate fell to 35..50 (sim60b.csv).
//
// --sim_variable_step writes the measured time since the previous frame into
// the constant before each frame (clamped to 1/240..1/20 s, the clamp keeping
// hitches and loading stalls from becoming huge steps), so speed no longer
// follows frame rate. ApplySimRate (host_tuning.cpp) leaves the constant's
// page writable.
//
// That alone isn't enough (PC run crackdown_039, 2026-10-11 21:32): at 60 fps
// speech lines cut off early, the car elevator in the Agency base moves fast,
// and driving feels fast. The game has a second clock (disassembly): the
// object at [0x82DE25C0] (constructor sub_823225C0) has a "locked" byte at
// +13 that the constructor always sets to 1. Thirteen inlined sites (listed
// in config/midasm_hooks.toml) then add a fixed per-frame ms count to their
// timers:
//   ms = locked ? 33 : (byte [0x82DE3FB0] ? u32 [0x82D99228] : 16)
//
// Unlocking the clock to use the game's own variable path (a first version of
// --sim_game_clock, PC run crackdown_018) broke physics: the player was thrown
// into a wall and cars flew around, with nothing else rewriting the clock
// during the frame. The unlocked mode changes more than the ms count (the
// step query sub_8254C568, the frame counter at +16, the limiter).
//
// So --sim_game_clock keeps the clock locked and replaces only the literal:
// a mid-asm hook after each `li rX,33` puts the measured frame time, in whole
// ms, in the register. The rounding remainder carries to the next frame, and
// the step constant gets the same value (ms / 1000), so both clocks agree.

#include "generated/default/crackdown_init.h"
#include "game_addresses.h"

#include "ap/bridge.h"
#include "midasm_hooks.h"

#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/ui/overlay/debug_overlay.h>

REXCVAR_DEFINE_BOOL(sim_variable_step, false, "Crackdown",
                    "Experimental: rewrite the step constant every frame with the measured "
                    "frame time (clamped to 1/240..1/20 s), so game speed doesn't follow "
                    "frame rate");
REXCVAR_DEFINE_BOOL(sim_game_clock, false, "Crackdown",
                    "Like --sim_variable_step, and also feed the measured frame time to the "
                    "game's fixed 33 ms per-frame timers (speech, timed objects). On by "
                    "default (host_tuning.cpp ApplyGameDefaults)");
REXCVAR_DEFINE_BOOL(sim_step_literal, false, "Crackdown",
                    "With --sim_game_clock/--sim_variable_step: leave the shared 1/30 literal "
                    "(0x820ED704) alone and scale only the main-loop update dt and the step "
                    "query. On by default");
REXCVAR_DEFINE_BOOL(sim_frame_counter_30hz, false, "Crackdown",
                    "Experimental, with --sim_game_clock: advance the game clock's frame "
                    "counter ([0x82DE25C0]+16) at 30 Hz instead of once per frame");
REXCVAR_DEFINE_BOOL(sim_anim_step, false, "Crackdown",
                    "With --sim_game_clock: also write the measured step into the animation "
                    "clock's own 1/30 (0x820ED7CC, sub_82330888) and the 0.033 s timer step "
                    "(0x82CFF2B4, sub_823FF3B8). On by default");
REXCVAR_DEFINE_BOOL(sim_ground_snap_30hz, true, "Crackdown",
                    "Above 30 fps, give the player's step-down snap (sub_822B4D40) the "
                    "velocity it has at 30 fps instead of drop / frame time (stairs speed bug)");
REXCVAR_DEFINE_BOOL(sim_vehicle_30hz, true, "Crackdown",
                    "Above 30 fps, run each car's Havok vehicle action (sub_824A1708) once per "
                    "1/30 s of accumulated physics time, so cars accelerate as at 30 fps");
REXCVAR_DEFINE_BOOL(sim_lockstep_step, false, "Crackdown",
                    "With --sim_game_clock: step exactly 1/60 s per frame (16/17/17 ms keyed to "
                    "the game's frame counter) instead of the measured frame time, so System "
                    "Link peers simulate identically. On by default in co-op");
REXCVAR_DECLARE(bool, diag_char_proxy);
REXCVAR_DEFINE_STRING(frame_log, "", "Crackdown",
                      "Benchmark: write each guest frame's host time (frame_time_us, one row per "
                      "frame, tools/frametimes.py reads it) to this CSV; works in Release builds");
REXCVAR_DEFINE_INT32(sim_game_clock_sites, CD_ADDR(0x1FFF, 0x3FFF), "Crackdown",
                     "Experimental: bitmask of the 33 ms sites --sim_game_clock rewrites "
                     "(bit order as in config/midasm_hooks.toml); for bisecting");

namespace {
constexpr uint32_t kStepConstantAddress = CD_ADDR(0x820ED704, 0x820F8084);
constexpr uint32_t kLockedFrameMs = 33;
constexpr double kMinStep = 1.0 / 240.0;
constexpr double kMaxStep = 1.0 / 20.0;

using Clock = std::chrono::steady_clock;
Clock::time_point g_last_frame{};
double g_ms_carry = 0.0;
// Read by the mid-asm hooks, which may run on other threads.
std::atomic<uint32_t> g_frame_ms{kLockedFrameMs};
// --sim_step_literal: measured step / (1/30), applied to the update dt and the
// step query instead of rewriting the literal.
std::atomic<float> g_step_scale{1.0f};
// This frame's step in seconds (1/30 until the first measured frame).
std::atomic<float> g_step{1.0f / 30.0f};
double g_counter_carry = 0.0;
constexpr uint32_t kClockPointerAddress = CD_ADDR(0x82DE25C0, 0x82E79A14);
constexpr uint32_t kAnimStepAddress = CD_ADDR(0x820ED7CC, 0x820F8154);   // 1/30, read only by sub_82330888
constexpr uint32_t kTimerStepAddress = CD_ADDR(0x82CFF2B4, 0x82D927E0);  // 0.033, read only by sub_823FF3B8

// The flags are read once, at the first hook call (the guest starts after
// ApplyGameDefaults has set them). The hooks rewrite guest literals and keep
// clock state, which switching a flag off mid-game would not undo (Codex
// review 2026-10-12), so they only take effect at startup.
struct SimFlags {
  bool variable_step = REXCVAR_GET(sim_variable_step);
  bool game_clock = REXCVAR_GET(sim_game_clock);
  bool step_literal = REXCVAR_GET(sim_step_literal);
  bool anim_step = REXCVAR_GET(sim_anim_step);
  bool frame_counter_30hz = REXCVAR_GET(sim_frame_counter_30hz);
  bool ground_snap_30hz = REXCVAR_GET(sim_ground_snap_30hz);
  bool lockstep_step = REXCVAR_GET(sim_lockstep_step);
  int32_t game_clock_sites = REXCVAR_GET(sim_game_clock_sites);
};
const SimFlags& Sim() {
  static const SimFlags flags;
  return flags;
}

// The ground-snap fix applies only to frames clearly shorter than 1/30 s: a
// measured 30 fps frame is 33 or 34 ms, which must stay stock.
constexpr float kGroundSnapMaxDt = 1.0f / 32.0f;
}  // namespace

// --sim_vehicle_30hz. The game's vehicle action sub_824A1708 (Havok vehicle
// kit, re-notes "Vehicles") takes the step's dt ([stepInfo+8]) and scales its
// forces by it, but the result still depends on the step size: on the garage
// replay (bench/car_tp.rec, --diag_vehicle, 2026-10-21) the launch matches
// 30 fps for 0.5 s, then 60 fps falls behind with more engine torque
// (lower rpm), i.e. a per-step loss: 14.1 m/s at 1 s at 30 fps, 13.0 at 60,
// 12.3 at 120. Running the action only every other step at 60 fps with twice
// the dt (and nothing else changed) gave 13.7 at 1 s and the 30 fps curve
// after. So the action runs once per 1/30 s of physics time, with the time
// accumulated since its last run; Havok still integrates the chassis every
// step. Per vehicle, from the physics steps only, so co-op peers (identical
// steps in lockstep) stay identical.
float CrackdownVehicleActionStep(uint32_t vehicle, float dt) {
  constexpr float kPeriod = 1.0f / 30.0f;
  static const bool enabled = REXCVAR_GET(sim_vehicle_30hz);
  // Steps of 25 ms and more (under 40 fps) run as they are.
  if (!enabled || dt >= kPeriod * 0.75f) return dt;
  static std::mutex mutex;
  static std::unordered_map<uint32_t, float> pending;
  std::lock_guard lock(mutex);
  float& t = pending[vehicle];
  t += dt;
  // Run on the step that brings the total closest to one period (the guest
  // limiter's 60 fps is ~61.6, 16.2 ms steps: 2 of them make a period, not 3).
  if (t + dt * 0.5f < kPeriod) return 0.0f;
  const float step = t;
  t = 0.0f;
  return step;
}

// Consumers of 30 Hz counters use the same clamped simulation
// step as the game update. Stock clocks must advance exactly one tick.
double CrackdownMeasuredStepSeconds() {
  if (!Sim().variable_step && !Sim().game_clock) return 1.0 / 30.0;
  return double(g_step.load(std::memory_order_relaxed));
}

// Leaves the 33 in place unless the site's bit is set. g_frame_ms stays 33
// while --sim_game_clock is off.
#define CRACKDOWN_DEFINE_FRAME_MS(bit, addr)                  \
  void CrackdownFrameMs_##addr(PPCRegister& r) {              \
    if (Sim().game_clock_sites & (1 << (bit))) {             \
      r.u64 = g_frame_ms.load(std::memory_order_relaxed);     \
    }                                                         \
  }
CRACKDOWN_FRAME_MS_SITES(CRACKDOWN_DEFINE_FRAME_MS)
#undef CRACKDOWN_DEFINE_FRAME_MS

// --frame_log: written only from this hook (the guest's once-per-frame timer
// update). The window-close path fflushes all streams (FlushPerfCsvLog); a
// row cut short there is dropped by tools/frametimes.py.
static void LogFrameTime() {
  static std::FILE* f = [] {
    const std::string& path = REXCVAR_GET(frame_log);
    std::FILE* file = path.empty() ? nullptr : std::fopen(path.c_str(), "w");
    if (file) {
      std::setvbuf(file, nullptr, _IOFBF, 1 << 16);
      std::fputs("frame_time_us\n", file);
    }
    return file;
  }();
  static Clock::time_point last{};
  if (!f) return;
  const Clock::time_point now = Clock::now();
  if (last != Clock::time_point{}) {
    std::fprintf(f, "%lld\n", static_cast<long long>(
        std::chrono::duration_cast<std::chrono::microseconds>(now - last).count()));
  }
  last = now;
}

// Guest frame rate for the SDK's F3 overlay (CrackdownApp passes
// GuestFrameStats to SetGuestFrameStats). Averaged over half-second windows so
// the number is readable; updated from the same once-per-frame hook.
static std::atomic<uint64_t> g_stats_frames{0};
static std::atomic<double> g_stats_fps{0.0};
static void CountFrame() {
  static Clock::time_point window_start{};
  static uint64_t window_frames = 0;
  const Clock::time_point now = Clock::now();
  g_stats_frames.fetch_add(1, std::memory_order_relaxed);
  if (window_start == Clock::time_point{}) {
    window_start = now;
    return;
  }
  ++window_frames;
  const double seconds = std::chrono::duration<double>(now - window_start).count();
  if (seconds >= 0.5) {
    g_stats_fps.store(window_frames / seconds, std::memory_order_relaxed);
    window_start = now;
    window_frames = 0;
  }
}

namespace crackdown {
rex::ui::FrameStats GuestFrameStats() {
  rex::ui::FrameStats stats;
  stats.frame_count = g_stats_frames.load(std::memory_order_relaxed);
  stats.fps = g_stats_fps.load(std::memory_order_relaxed);
  stats.frame_time_ms = stats.fps > 0 ? 1000.0 / stats.fps : 0;
  return stats;
}
}  // namespace crackdown

// Above 60 fps the guest limiter is too coarse: it sleeps (1000 / cap) whole ms,
// so 120 would give 125 fps and 144 would give 166 (1000 / 144 = 6). ApplyFpsCap
// (host_tuning.cpp) then sets the guest cap to 1000 and this paces frame starts
// to exactly 1 / fps_cap: sleep to about 1.5 ms before the deadline, then spin.
// At 60 and below the guest limiter stays in charge (the verified setup).
// --sim_lockstep_step (co-op) steps exactly 1/60 s per frame, so frames must
// also be exactly 1/60 s apart or game time drifts from real time: the guest
// limiter's 16 ms sleeps ran 61.5 fps (coop60 soak 2026-10-20, frames-t2.csv),
// i.e. the game 2.5% fast. So in that mode this paces 60 fps itself, and
// ApplyFpsCap moves the guest limiter out of the way as above 60.
static void PaceFrame() {
  static const int32_t cap = Sim().lockstep_step ? 60 : rex::cvar::Query<int32_t>("fps_cap");
  if (cap <= 60 && !Sim().lockstep_step) return;
  static const Clock::duration period = std::chrono::duration_cast<Clock::duration>(
      std::chrono::duration<double>(1.0 / cap));
  static Clock::time_point deadline{};
  Clock::time_point now = Clock::now();
  if (now >= deadline) {
    deadline = now + period;  // first frame, or this frame ran long: don't catch up
    return;
  }
  while (deadline - now > std::chrono::microseconds(1500)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    now = Clock::now();
  }
  while (now < deadline) {
    std::this_thread::yield();
    now = Clock::now();
  }
  deadline += period;
}

// Stores one frame's step everywhere the measured-clock modes put it.
static void StoreStep(float step, uint8_t* base) {
  g_step.store(step, std::memory_order_relaxed);
  if (Sim().anim_step) {
    REX_STORE_U32(kAnimStepAddress, std::bit_cast<uint32_t>(step));
    REX_STORE_U32(kTimerStepAddress, std::bit_cast<uint32_t>(step));
  }
  if (Sim().step_literal) {
    g_step_scale.store(step * 30.0f, std::memory_order_relaxed);
  } else {
    REX_STORE_U32(kStepConstantAddress, std::bit_cast<uint32_t>(step));
  }
}

// --sim_lockstep_step (co-op at 60 fps). System Link runs the game in
// lockstep (disassembly): with a session up, the frame loop sub_82325850
// calls sub_8246A5C0 every frame, which waits for the peer's input for frame
// [[0x82DE25C0]+16] - delay and, while it is missing, sets the clock's pause
// byte +14 so sub_82324938 doesn't advance the counter
// (crackdown_recomp.152.cpp:6365-6388, .78.cpp:11067-11117). Once the input is
// there, sub_8246A828 compares the two peers' state hash for that frame,
// `lfd f0,16(r31); lfd f13,16(r30); fcmpu` (.120.cpp:9571-9578, 9632-9639),
// and on a mismatch raises error 8 through sub_823C49A0 (.120.cpp:9666-9672),
// shown as "Connection to other player lost". The hash is the double at
// 0x82BAEB78+56, written by sub_8231B190 (.143.cpp:6773-7407, past frame 10)
// under sub_823C4018, which only the update sub_826A6B50 calls. With the
// measured clock each peer fed that update its own frame time, so the hashes
// split: out/coop-live 2026-10-13 (A crackdown_013, B crackdown_009, 60 fps)
// shows the 49-byte lockstep packets every ~16 ms from 21:43:26.393 and all
// traffic stopping on both peers at 21:43:26.610; at stock 30 fps (A 014,
// B 010) it runs on. So in this mode the step is a function of the frame
// counter, which both peers share: frame n gets
// floor((n + 1) * 1000 / 60) - floor(n * 1000 / 60) ms (16/17/17, exactly
// 1000/60 on average), and the step is that ms count / 1000 like the measured
// path. A stalled frame (counter paused) repeats the same values, as stock's
// fixed 33 ms does.
static void LockstepStep(uint8_t* base) {
  const uint32_t clock = REX_LOAD_U32(kClockPointerAddress);
  uint32_t ms = kLockedFrameMs;  // no clock yet: stock 33 ms, 1/30
  float step = 1.0f / 30.0f;
  if (clock) {
    const uint64_t n = REX_LOAD_U32(clock + 16);
    ms = uint32_t((n + 1) * 1000 / 60 - n * 1000 / 60);
    step = float(ms) / 1000.0f;
  }
  g_frame_ms.store(ms, std::memory_order_relaxed);
  StoreStep(step, base);
}

CD_HOOK_RAW(CD_SUB(826A7698, 8272F7C0)) {
  PaceFrame();
  LogFrameTime();
  CountFrame();
  const bool game_clock = Sim().game_clock;
  if (game_clock && Sim().lockstep_step) {
    LockstepStep(base);
  } else if (Sim().variable_step || game_clock) {
    const Clock::time_point now = Clock::now();
    if (g_last_frame != Clock::time_point{}) {
      const double dt = std::chrono::duration<double>(now - g_last_frame).count();
      float step = float(std::clamp(dt, kMinStep, kMaxStep));
      if (game_clock) {
        g_ms_carry += double(step) * 1000.0;
        const uint32_t ms = uint32_t(std::floor(g_ms_carry));  // >= 4: step >= 1/240 s
        g_ms_carry -= ms;
        g_frame_ms.store(ms, std::memory_order_relaxed);
        step = float(ms) / 1000.0f;
      }
      StoreStep(step, base);
    }
    g_last_frame = now;
  }
  CD_IMP(826A7698, 8272F7C0)(ctx, base);
#if !CRACKDOWN_TU  // the AP bridge is not ported to the title update yet
  crackdown::ap::OnFrame(ctx, base);  // AP bridge: queued network work + grants
#endif
}

// The constant is the image's only 1/30, so it is also Havok's default
// hkWorldCinfo::m_expectedMinPsiDeltaTime: the hkWorldCinfo constructor
// sub_8298D618 does `lfs f9,[0x820ED704]; stfs f9,92(r3)`, and Havok's own
// reflection table for hkWorldCinfo puts expectedMinPsiDeltaTime at +92.
// Havok sizes its collision tolerances from it when a world is built, so
// with the constant rewritten every frame the world got whatever frame time
// was current at load. PC run crackdown_031 (2026-10-12, --fps_cap=60
// --sim_game_clock): physics got the right step (--diag_physics_dt), but
// running down stairs still built up huge speed and cars hitting the bottom
// of the stairs were wrecked instantly. Keep Havok's 1/30 there.
CD_HOOK_RAW(CD_SUB(8298D618, 82A16508)) {
  const uint32_t cinfo = ctx.r3.u32;
  CD_IMP(8298D618, 82A16508)(ctx, base);
  if (Sim().variable_step || Sim().game_clock) {
    constexpr uint32_t kOneThirtieth = 0x3D088889;  // 1/30, the image's literal
    const uint32_t got = REX_LOAD_U32(cinfo + 92);
    if (got != kOneThirtieth) {
      REXLOG_INFO("sim_game_clock: hkWorldCinfo {:08X} expectedMinPsiDeltaTime {} -> 1/30",
                  cinfo, std::bit_cast<float>(got));
      REX_STORE_U32(cinfo + 92, kOneThirtieth);
    }
  }
}

// --sim_step_literal (bisecting the 60 fps stairs bug): the literal 0x820ED704
// has a dozen direct readers besides the game step (see docs/60fps-audit.md).
// Leave it at 1/30 and scale only the two step sources:
// - the main loop's update, sub_826A6B50(f1 = [0x82CFE4B4] * [0x820ED704]),
//   called only from sub_826A7698;
// - the step query sub_8254C568, which with the clock locked returns
//   [0x82C63578] * [0x820ED704] (the unlocked paths aren't used).
CD_HOOK_RAW(CD_SUB(826A6B50, 8272ED00)) {
  if (Sim().step_literal) {
    ctx.f1.f64 = double(float(ctx.f1.f64) * g_step_scale.load(std::memory_order_relaxed));
  }
  CD_IMP(826A6B50, 8272ED00)(ctx, base);
}

CD_HOOK_RAW(CD_SUB(8254C568, 825B7020)) {
  CD_IMP(8254C568, 825B7020)(ctx, base);
  if (Sim().step_literal) {
    ctx.f1.f64 = double(float(ctx.f1.f64) * g_step_scale.load(std::memory_order_relaxed));
  }
}

// --sim_frame_counter_30hz (bisecting the 60 fps stairs bug): sub_82324938,
// called once per frame from the main loop, does `[clock+16] += 1` while the
// clock is locked (+13) and not paused (+14). About a dozen functions use that
// counter for frame deadlines (docs/60fps-audit.md), which expire twice as
// fast at 60 fps. Re-advance it by the number of whole 30 Hz ticks elapsed.
CD_HOOK_RAW(CD_SUB(82324938, 82358AD0)) {
  const uint32_t clock = REX_LOAD_U32(kClockPointerAddress);
  const uint32_t before = clock ? REX_LOAD_U32(clock + 16) : 0;
  CD_IMP(82324938, 82358AD0)(ctx, base);
  if (!clock || !Sim().frame_counter_30hz || !Sim().game_clock) {
    return;
  }
  if (REX_LOAD_U32(clock + 16) != before + 1) {
    return;  // paused, unlocked or reset this frame: leave it alone
  }
  g_counter_carry += double(g_step.load(std::memory_order_relaxed)) * 30.0;
  const uint32_t ticks = uint32_t(std::floor(g_counter_carry));
  g_counter_carry -= ticks;
  REX_STORE_U32(clock + 16, before + ticks);
}

// Stairs speed bug at 60 fps. The character controller update sub_822A9E78
// builds the velocity it hands Havok's character proxy at controller+352:
// move velocity, plus the correction of either sub_822B4100 (step up) or
// sub_822B4D40 (step down / stick to ground), plus gravity, then
// sub_822ACF68(dt) does setLinearVelocity and hkCharacterProxy::integrate
// (sub_822BF268). sub_822B4D40(controller r3, dt f1, out r5) ends with
// (0x822B562C):
//     out.y = -clamp(target - height, 0, max) * gain / dt
// i.e. it pulls the player down the whole drop in one step. A stair step is a
// fixed height, so at 60 fps that velocity doubles. The proxy's solver
// projects it onto the stair-edge plane, which turns part of it into forward
// speed, and the controller keeps the proxy's output velocity: --diag_char_proxy
// on the same replayed stairs run (bench/stairs.rec) shows input vy down to
// -7.6 m/s and a 13.8 m/s peak at 30 fps (crackdown_047), vy -19.2 and
// 28-32 m/s at 60 fps (crackdown_048/049); running speed is 11.2 m/s.
// Dividing by max(dt, 1/30) instead gives the snap its 30 fps velocity, so it
// covers the drop over the same time instead of in one 60 Hz frame. At 30 fps
// (measured 33/34 ms frames included, see kGroundSnapMaxDt) nothing changes.
CD_HOOK_RAW(CD_SUB(822B4D40, 822DB248)) {
  const uint32_t out = ctx.r5.u32;
  const float dt = float(ctx.f1.f64);
  const uint32_t before = REX_LOAD_U32(out + 4);
  CD_IMP(822B4D40, 822DB248)(ctx, base);
  const uint32_t after = REX_LOAD_U32(out + 4);
  if (after == before) {
    return;  // no snap this step
  }
  float y = std::bit_cast<float>(after);
  const float y_game = y;
  if (Sim().ground_snap_30hz && dt > 0.0f && dt < kGroundSnapMaxDt) {
    y *= dt * 30.0f;
    REX_STORE_U32(out + 4, std::bit_cast<uint32_t>(y));
  }
  if (REXCVAR_GET(diag_char_proxy) && std::fabs(y_game) > 0.5f) {
    REXLOG_INFO("diag_char_proxy: ground snap dt={:.4f} out.y={:.3f} -> {:.3f} ret={}", dt, y_game, y,
                ctx.r3.u32 & 0xFF);
  }
}
