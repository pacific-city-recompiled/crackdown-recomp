#include "generated/default/crackdown_init.h"

#include <rex/rex_app.h>

#include "host_tuning.h"
#include "input_replay.h"

// Mirrors crackdown/src/crackdown_app.h for the parts ported to the update so far.
class CrackdownTuApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;
  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<CrackdownTuApp>(new CrackdownTuApp(ctx, "crackdown_tu", PPCImageConfig));
  }
  void OnPostInitLogging() override { crackdown::ApplyGpuPluginDefault(); }
  void OnConfigurePaths(rex::PathConfig& paths) override { crackdown::ApplyStartupDefaults(paths); }
  void OnPreLaunchModule() override {
    crackdown::ApplyGameDefaults();
    crackdown::ApplySimRate(runtime()->memory());
    crackdown::ApplyFpsCap(runtime()->memory());
    crackdown::DumpGuestImage(runtime());
    crackdown::SetInputReplayQuit([this] {
      app_context().CallInUIThreadDeferred([this] { if (window()) window()->RequestClose(); });
    });
  }
};

REX_DEFINE_APP(crackdown_tu, CrackdownTuApp::Create)
