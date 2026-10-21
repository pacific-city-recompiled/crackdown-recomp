// Opt-in transparent wrapper tracing. Generated disassembly references are
// recorded in docs/coop-evidence.md. The stock guest implementation runs once.
// Raw keys and packet payloads are never written. net_trace lives in the SDK.
#include "generated/default/crackdown_init.h"
#include "game_addresses.h"
#include <rex/hook.h>
#include <rex/system/xthread.h>
#include <rex/system/coop_network_context.h>
#include <atomic>
#include <chrono>
#include <fstream>
#include <map>
#include <mutex>
#include <string_view>
#include <thread>
REXCVAR_DEFINE_STRING(coop_trace_file,"","Crackdown","Optional network JSONL evidence path");
REXCVAR_DEFINE_BOOL(coop_trace_verbose,false,"Crackdown","Also trace enter lines, empty polls and ProfileGate");
namespace {
std::mutex trace_mutex;
const uint8_t* ReadGuest(uint32_t address,uint32_t length) {
  if (!address || uint64_t(address)+length>0x100000000ull) return nullptr;
  auto heap=REX_KERNEL_MEMORY()->LookupHeap(address);
  rex::memory::HeapAllocationInfo region{};
  if (!heap || !heap->QueryRegionInfo(address,&region) ||
      !(region.state&rex::memory::kMemoryAllocationCommit) ||
      !(region.protect&rex::memory::kMemoryProtectRead) ||
      uint64_t(address)+length>uint64_t(region.base_address)+region.region_size) return nullptr;
  return REX_KERNEL_MEMORY()->TranslateVirtual<const uint8_t*>(address);
}
std::string Decode(std::string_view operation,const uint32_t* args,uint32_t result) {
  std::string decoded;
  const bool returning=operation.ends_with(" return");
  const bool send=operation.starts_with("NetDll_send");
  const bool recv=operation.starts_with("NetDll_recv");
  if ((send && !returning) || (recv && returning && int32_t(result)>0)) {
    const uint32_t length=send?args[2]:result;
    const auto bounded=std::min(length,4096u);
    if (const auto bytes=ReadGuest(args[1],bounded)) {
      uint32_t hash=2166136261u;
      for (uint32_t i=0;i<bounded;i++) hash=(hash^bytes[i])*16777619u;
      decoded+=fmt::format(",\"payload_length\":{},\"hashed_bytes\":{},\"fnv1a\":{}",length,bounded,hash);
    }
  }
  uint32_t address=0;
  if (operation.starts_with("NetDll_bind") || operation.starts_with("NetDll_connect")) address=args[1];
  if (operation.starts_with("NetDll_sendto") || (operation.starts_with("NetDll_recvfrom") && returning && int32_t(result)>=0)) address=args[4];
  if (const auto bytes=ReadGuest(address,16)) {
    decoded+=fmt::format(",\"family\":{},\"port\":{},\"address\":{}",
        rex::memory::load_and_swap<uint16_t>(bytes),rex::memory::load_and_swap<uint16_t>(bytes+2),rex::memory::load_and_swap<uint32_t>(bytes+4));
  }
  if (operation.starts_with("NetDll_XNetRegisterKey") || operation.starts_with("NetDll_XNetUnregisterKey") ||
      (operation.starts_with("NetDll_XNetCreateKey") && returning && result==0)) {
    if (const auto bytes=ReadGuest(args[0],8))
      decoded+=fmt::format(",\"session_id\":\"{:016X}\"",rex::memory::load_and_swap<uint64_t>(bytes));
  }
  if (operation.starts_with("SessionCreate"))
    decoded+=fmt::format(",\"xgi_message\":{},\"flags\":{},\"user\":{},\"public_slots\":{},\"private_slots\":{},\"info_ptr\":{},\"overlapped_ptr\":{}",0xb0010,args[0],args[1],args[2],args[3],args[5],args[6]);
  if (operation.starts_with("SessionJoin"))
    decoded+=fmt::format(",\"xgi_message\":{},\"member_count\":{}",0xb0012,args[1]);
  if (operation.starts_with("PrivilegeWrapper")) {
    decoded+=fmt::format(",\"privilege\":{}",args[1]);
    if (returning) if (const auto bytes=ReadGuest(args[2],4))
      decoded+=fmt::format(",\"allowed\":{}",rex::memory::load_and_swap<uint32_t>(bytes));
  }
  if (operation.starts_with("ProfileGate") && returning) {
    if (const auto bytes=ReadGuest(args[0],200))
      decoded+=fmt::format(",\"user\":{},\"profile_flags\":{}",rex::memory::load_and_swap<uint32_t>(bytes),rex::memory::load_and_swap<uint32_t>(bytes+196));
  }
  return decoded;
}
void Trace(const char* operation,uint32_t pc,uint32_t caller,const uint32_t* args,
           uint32_t result,uint32_t last_error,
           uint32_t thread_id=rex::system::XThread::GetCurrentThreadId()) {
  const auto now=std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
  auto line=fmt::format("{{\"us\":{},\"op\":\"{}\",\"pc\":{},\"lr\":{},\"thread\":{},"
      "\"args\":[{},{},{},{},{},{},{},{}],\"result\":{},\"last_error\":{}{}}}",
      now,operation,pc,caller,thread_id,
      args[0],args[1],args[2],args[3],args[4],args[5],args[6],args[7],result,last_error,Decode(operation,args,result));
  std::lock_guard lock(trace_mutex);
  const auto path=REXCVAR_GET(coop_trace_file);
  if (path.empty()) { REXLOG_INFO("coop_trace {}",line); return; }
  static std::ofstream output(path,std::ios::app); output<<line<<'\n'; output.flush();
}
// The game polls nonblocking UDP and select() in a tight loop (2.8 GB of trace in
// minutes on 2026-10-13); keep only calls that did something.
bool Quiet(std::string_view operation,uint32_t result,uint32_t last_error) {
  if (operation.ends_with(" enter"))
    return !(operation.starts_with("NetDll_connect") || operation.starts_with("NetDll_accept") ||
             operation.starts_with("Session"));
  if (operation.starts_with("ProfileGate") || operation.starts_with("NetDll___WSAFDIsSet") ||
      operation.starts_with("NetDll_WSAGetLastError")) return true;
  if (operation.starts_with("NetDll_select")) return result==0;
  // Re-armed and waited on every poll (millions of calls per minute in game).
  if (operation.starts_with("NetDll_WSAEventSelect")) return result==0;
  if (operation.starts_with("NetDll_WSAWaitForMultipleEvents")) return result!=~0u;
  if (operation.starts_with("NetDll_recv")) return result==uint32_t(-1) && last_error==10035;
  return false;
}
void MaybeTrace(const char* operation,uint32_t pc,uint32_t caller,const uint32_t* args,
                uint32_t result,uint32_t last_error) {
  if (!REXCVAR_GET(coop_trace_verbose) && Quiet(operation,result,last_error)) return;
  Trace(operation,pc,caller,args,result,last_error);
}
// Calls still inside the SDK after 1.5 s are logged once as "<op> stuck":
// both instances fell silent together in the co-op load (2026-10-13) with no
// failing return, which looks like a blocked call rather than an error.
struct InFlight { const char* operation; uint32_t thread,pc,caller; uint32_t args[8]; int64_t start; bool reported; };
std::mutex inflight_mutex;
std::map<uint32_t,InFlight> inflight;
int64_t NowUs() {
  return std::chrono::duration_cast<std::chrono::microseconds>(
      std::chrono::steady_clock::now().time_since_epoch()).count();
}
void Watchdog() {
  for (;;) {
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    std::vector<InFlight> stuck;
    {
      std::lock_guard lock(inflight_mutex);
      for (auto& [thread,call]:inflight)
        if (!call.reported && NowUs()-call.start>1'500'000) { call.reported=true; stuck.push_back(call); }
    }
    for (const auto& call:stuck)
      Trace(fmt::format("{} stuck",call.operation).c_str(),call.pc,call.caller,call.args,0,0,call.thread);
  }
}
void Begin(const char* operation,uint32_t pc,uint32_t caller,const uint32_t* args) {
  static std::once_flag started;
  std::call_once(started,[] { std::thread(Watchdog).detach(); });
  const uint32_t thread=rex::system::XThread::GetCurrentThreadId();
  InFlight call{operation,thread,pc,caller,{},NowUs(),false};
  std::copy(args,args+8,call.args);
  std::lock_guard lock(inflight_mutex);
  inflight[thread]=call;
}
void End() {
  std::lock_guard lock(inflight_mutex);
  inflight.erase(rex::system::XThread::GetCurrentThreadId());
}
}
// disc/tu: the NetDll wrapper's address in each version (same import called; docs/tu-address-map.csv).
#define COOP_TRACE_WRAPPER(disc,tu,operation) \
CD_HOOK_RAW(CD_SUB(disc,tu)) { \
  constexpr uint32_t pc=CD_ADDR(0x##disc,0x##tu); \
  if (!REXCVAR_GET(net_trace)) { CD_IMP(disc,tu)(ctx,base); return; } \
  const uint32_t caller=uint32_t(ctx.lr); \
  const uint32_t args[]={ctx.r3.u32,ctx.r4.u32,ctx.r5.u32,ctx.r6.u32,ctx.r7.u32,ctx.r8.u32,ctx.r9.u32,ctx.r10.u32}; \
  MaybeTrace(operation " enter",pc,caller,args,0,0); \
  Begin(operation,pc,caller,args); \
  CD_IMP(disc,tu)(ctx,base); \
  End(); \
  auto thread=rex::system::XThread::GetCurrentThread(); \
  MaybeTrace(operation " return",pc,caller,args,ctx.r3.u32,thread?thread->last_error():0); \
}
COOP_TRACE_WRAPPER(82929670,829B2740,"NetDll_getsockname")
COOP_TRACE_WRAPPER(829297C0,829B2890,"NetDll_WSAWaitForMultipleEvents")
COOP_TRACE_WRAPPER(82929448,829B2518,"NetDll_XNetInAddrToString")
COOP_TRACE_WRAPPER(82929630,829B2700,"NetDll_setsockopt")
COOP_TRACE_WRAPPER(829293D0,829B24A0,"NetDll_XNetCreateKey")
COOP_TRACE_WRAPPER(82929508,829B25D8,"NetDll_XNetQosRelease")
COOP_TRACE_WRAPPER(829296A0,829B2770,"NetDll_bind")
COOP_TRACE_WRAPPER(82929470,829B2540,"NetDll_XNetGetConnectStatus")
COOP_TRACE_WRAPPER(82929650,829B2720,"NetDll_getsockopt")
COOP_TRACE_WRAPPER(829296D0,829B27A0,"NetDll_listen")
COOP_TRACE_WRAPPER(829297B8,829B2888,"NetDll_WSACloseEvent")
COOP_TRACE_WRAPPER(829292F8,829B23C8,"NetDll_XNetStartup")
COOP_TRACE_WRAPPER(829293E0,829B24B0,"NetDll_XNetRegisterKey")
COOP_TRACE_WRAPPER(82929528,829B25F8,"NetDll_WSAStartup")
COOP_TRACE_WRAPPER(82929418,829B24E8,"NetDll_XNetServerToInAddr")
COOP_TRACE_WRAPPER(82929608,829B26D8,"NetDll_shutdown")
COOP_TRACE_WRAPPER(82929788,829B2858,"NetDll_WSAEventSelect")
COOP_TRACE_WRAPPER(82929430,829B2500,"NetDll_XNetInAddrToXnAddr")
COOP_TRACE_WRAPPER(82929618,829B26E8,"NetDll_ioctlsocket")
COOP_TRACE_WRAPPER(829297B0,829B2880,"NetDll_WSACreateEvent")
COOP_TRACE_WRAPPER(82929400,829B24D0,"NetDll_XNetXnAddrToInAddr")
COOP_TRACE_WRAPPER(829295E0,829B26B0,"NetDll_socket")
COOP_TRACE_WRAPPER(829297A0,829B2870,"NetDll_inet_addr")
COOP_TRACE_WRAPPER(82929518,829B25E8,"NetDll_XNetGetTitleXnAddr")
COOP_TRACE_WRAPPER(829296B8,829B2788,"NetDll_connect")
COOP_TRACE_WRAPPER(829296E0,829B27B0,"NetDll_accept")
COOP_TRACE_WRAPPER(829293F0,829B24C0,"NetDll_XNetUnregisterKey")
COOP_TRACE_WRAPPER(82929480,829B2550,"NetDll_XNetQosListen")
COOP_TRACE_WRAPPER(82929750,829B2820,"NetDll_send")
COOP_TRACE_WRAPPER(82929688,829B2758,"NetDll_getpeername")
COOP_TRACE_WRAPPER(829297C8,829B2898,"NetDll___WSAFDIsSet")
COOP_TRACE_WRAPPER(829295F8,829B26C8,"NetDll_closesocket")
COOP_TRACE_WRAPPER(82929730,829B2800,"NetDll_recvfrom")
COOP_TRACE_WRAPPER(82929718,829B27E8,"NetDll_recv")
COOP_TRACE_WRAPPER(829296F8,829B27C8,"NetDll_select")
COOP_TRACE_WRAPPER(829293C0,829B2490,"NetDll_XNetRandom")
COOP_TRACE_WRAPPER(829294A0,829B2570,"NetDll_XNetQosLookup")
COOP_TRACE_WRAPPER(82929460,829B2530,"NetDll_XNetConnect")
COOP_TRACE_WRAPPER(82929768,829B2838,"NetDll_sendto")
COOP_TRACE_WRAPPER(829297A8,829B2878,"NetDll_WSAGetLastError")
COOP_TRACE_WRAPPER(8292A9B8,829B3A88,"SessionCreate")
COOP_TRACE_WRAPPER(8292ABF8,829B3CC8,"SessionJoinLocal")
COOP_TRACE_WRAPPER(8292ACA0,829B3D70,"SessionJoinRemote")
COOP_TRACE_WRAPPER(8292AF70,829B4040,"SessionStart")
COOP_TRACE_WRAPPER(8292B008,829B40D8,"SessionEnd")
COOP_TRACE_WRAPPER(8292B0B8,829B4188,"SessionSearchEx")
// .7.cpp:21652/21679 checks local sign-in before the privilege import.
COOP_TRACE_WRAPPER(82744760,827CDB28,"PrivilegeWrapper")
// .140.cpp:32988 sign-in; privilege 252/251 at 0x82A8F034/0x82A8F058.
COOP_TRACE_WRAPPER(82A8ED90,82B18540,"ProfileGate")
// --coop_trace_desync: sub_823C49A0(game r3, code r4) is where the System Link
// layer ends a session (it stores the code at game+762 and hands it to the
// error screen, sub_8266DDC0). Codes seen in the disassembly: 2 lockstep
// timeout (10 s real time, sub_8246A5C0 .78.cpp:11177-11188), 7 stall without
// a resend (.78.cpp:11137-11154), 8 state-hash mismatch (sub_8246A828
// .120.cpp:9666-9672), 9 frame-scheduled wait over 10 s (.78.cpp:11010-11036),
// 4/5 pool exhaustion, 0/3 session/sign-in. Logs each distinct code once with
// the lockstep frame [[0x82DE25C0]+16]. Disc only: the title update's address
// for this function isn't in docs/tu-address-map.csv yet.
REXCVAR_DEFINE_BOOL(coop_trace_desync,false,"Crackdown","Log the System Link error code (sub_823C49A0) and lockstep frame, once per code");
#if !CRACKDOWN_TU
REX_HOOK_RAW(sub_823C49A0) {
  if (REXCVAR_GET(coop_trace_desync)) {
    static std::atomic<uint64_t> seen{0};
    const uint32_t code=ctx.r4.u32;
    const uint64_t bit=code<64?(1ull<<code):(1ull<<63);
    if (!(seen.fetch_or(bit)&bit)) {
      const uint32_t clock=REX_LOAD_U32(0x82DE25C0);
      REXLOG_WARN("coop_trace_desync: session error code {} at frame {} (lr {:08X})",code,
                  clock?REX_LOAD_U32(clock+16):0u,uint32_t(ctx.lr));
    }
  }
  __imp__sub_823C49A0(ctx,base);
}
#endif
