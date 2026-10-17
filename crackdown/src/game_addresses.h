// Guest addresses that differ between the disc XEX and Title Update #6.
//
// The disc build (crackdown/) and the update build (crackdown_tu/, compiled with
// CRACKDOWN_TU=1) share the hook sources. Every address a hook uses goes through
// one of these macros with both values side by side; the update value comes from
// docs/tu-address-map.csv and is checked against the update's own disassembly.
#pragma once

#ifndef CRACKDOWN_TU
#define CRACKDOWN_TU 0
#endif

#if CRACKDOWN_TU
#define CD_ADDR(disc, tu) tu
#define CD_SUB(disc, tu) sub_##tu
#define CD_IMP(disc, tu) __imp__sub_##tu
#else
#define CD_ADDR(disc, tu) disc
#define CD_SUB(disc, tu) sub_##disc
#define CD_IMP(disc, tu) __imp__sub_##disc
#endif

// REX_HOOK_RAW pastes its argument; this layer expands CD_SUB(...) first.
#define CD_HOOK_RAW(name) REX_HOOK_RAW(name)
