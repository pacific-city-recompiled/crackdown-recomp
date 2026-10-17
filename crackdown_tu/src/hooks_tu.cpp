// The two hooks the disc build needs to get past the New Game loading screen, at their
// Title Update #6 addresses (docs/tu-address-map.csv; see crackdown/src/hooks_events.cpp and
// hooks_storage.cpp for the reasoning). Disc 0x82A4FA70 -> 0x821512C0 (bl from the job
// dispatcher 0x82281200 at 0x822812DC, disc 0x8225C614); disc 0x827457E8 -> 0x827CECB8.
#include "generated/default/crackdown_init.h"
#include <rex/hook.h>
#include <rex/logging.h>

namespace {
constexpr uint32_t kXObjSignature = 0x52455800;
constexpr uint32_t kErrorDeviceNotConnected = 1167;
}  // namespace

REX_HOOK_RAW(sub_821512C0) {
  ctx.r11.s64 = 0;
  REX_STORE_U32(ctx.r3.u32 + 4, 0);
  if (REX_LOAD_U8(ctx.r3.u32 + 0) <= 1 && REX_LOAD_U32(ctx.r3.u32 + 8) == kXObjSignature) {
    const uint64_t event_ptr = ctx.r3.u64;
    __imp__KeResetEvent(ctx, base);
    ctx.r3.u64 = event_ptr;
    ctx.r11.s64 = 0;
  }
}

REX_HOOK_RAW(sub_827CECB8) {
  (void)base;
  REXKRNL_INFO("XMountUtilityDrive (0x827CECB8) -> no utility drive; streaming from game:");
  ctx.r3.u64 = kErrorDeviceNotConnected;
}

#include "input_replay.h"

// Character proxy integration (disc sub_822BF268, update 0x822ED748, identical 757
// instructions): marks gameplay for --input_replay's second segment.
REX_HOOK_RAW(sub_822ED748) {
  crackdown::NoteCharacterUpdate();
  __imp__sub_822ED748(ctx, base);
}
