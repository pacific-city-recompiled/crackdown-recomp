// Camera turn speed (--camera_sensitivity).
//
// The right stick reaches the game unscaled (deadzone 8689 removed and
// rescaled to +-127 in sub_82413178), so full deflection already turns the
// camera at its top rate; a faster camera has to scale the game's rates.
//
// The player-camera updates sub_824FAC28 and sub_824F9518 read their turn
// rates from a tuning block whose pointer is at 0x82DE258C (disc build):
//   +28 / +36  yaw rate when the stick starts moving (0.75 rad/s)
//   +32 / +40  pitch rate (-0.625; the sign is the pitch direction)
//   +60 / +64  top yaw rate the turn ramps up to while held (3.0 rad/s)
// (+28/+32 and +36/+40 are the two modes chosen in sub_82503F20 /
// sub_82503FD8, which return them; +60/+64 are read inline.) The update
// multiplies these by stick^2 and dt, so the turn speed is the same at every
// frame rate (measured 172 deg/s at 30 and 60 fps = +60's 3.0 rad/s).
//
// Scaling the data rather than the stick: the block is loaded from game data
// and the hook rescales it whenever its values change (a reload), keeping the
// original values to scale from. sub_82503F20 runs once per camera update, so
// it is the tick.
//
// Disc XEX addresses. The title-update build keeps per-version addresses in
// game_addresses.h on its own branch; map these there when this lands on it.

#include "generated/default/crackdown_init.h"
#include "game_addresses.h"

#include <algorithm>
#include <array>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging/macros.h>
#include <rex/memory.h>

REXCVAR_DEFINE_DOUBLE(camera_sensitivity, 1.0, "Crackdown",
                      "Camera turn speed multiplier for the right stick (0.25 - 4.0; 1 = the "
                      "game's own speed)")
    .range(0.25, 4.0)
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

namespace {
constexpr uint32_t kCameraTuningPointer = CD_ADDR(0x82DE258C, 0x82E799DC);
constexpr std::array<uint32_t, 6> kRateOffsets = {28, 32, 36, 40, 60, 64};

struct State {
  uint32_t block = 0;
  std::array<float, kRateOffsets.size()> original{};
  std::array<float, kRateOffsets.size()> written{};
} g_state;

void ApplySensitivity(uint8_t* base) {
  const uint32_t block = rex::memory::load_and_swap<uint32_t>(base + kCameraTuningPointer);
  if (!block) {
    return;
  }
  const float scale = float(std::clamp(REXCVAR_GET(camera_sensitivity), 0.25, 4.0));
  for (size_t i = 0; i < kRateOffsets.size(); ++i) {
    uint8_t* field = base + block + kRateOffsets[i];
    const float current = rex::memory::load_and_swap<float>(field);
    // A new block, or the game wrote the field since we did: take it as the
    // original value.
    if (block != g_state.block || current != g_state.written[i]) {
      g_state.original[i] = current;
    }
    const float wanted = g_state.original[i] * scale;
    if (current != wanted) {
      rex::memory::store_and_swap<float>(field, wanted);
    }
    g_state.written[i] = wanted;
  }
  if (block != g_state.block) {
    REXLOG_INFO("camera_sensitivity: {} (tuning block {:08X}, top yaw rate {} -> {})", scale,
                block, g_state.original[4], g_state.written[4]);
    g_state.block = block;
  }
}
}  // namespace

CD_HOOK_RAW(CD_SUB(82503F20, 825677B0)) {
  ApplySensitivity(base);
  CD_IMP(82503F20, 825677B0)(ctx, base);
}
