#include "generated/default/crackdown_init.h"

#include <rex/rex_app.h>

#include "host_tuning.h"
#include "input_replay.h"
#include "pc_options.h"
#include "pc_settings.h"
#include "xui_patch.h"
#include <rex/filesystem.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/overlay/debug_overlay.h>
#include <rex/ui/window.h>

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
    SetGuestFrameStats(&crackdown::GuestFrameStats);
    crackdown::PCSettings().Initialize(rex::filesystem::GetExecutableFolder() / "crackdown.toml",
        rex::cvar::Query<bool>("vsync"), rex::cvar::Query<int32_t>("resolution_scale"),
        rex::cvar::Query<bool>("fullscreen"));
    crackdown::SetPCMenuQuitAction([this] {
      return app_context().CallInUIThreadDeferred([this] {
        if (window()) window()->RequestClose();
      });
    });
    crackdown::PreparePCMenu(runtime(), game_data_root(), cache_root());
    crackdown::SetInputReplayQuit([this] {
      app_context().CallInUIThreadDeferred([this] { if (window()) window()->RequestClose(); });
    });
  }
  // F11 switches between fullscreen (borderless) and windowed while playing.
  void OnCreateDialogs(rex::ui::ImGuiDrawer*) override {
    rex::ui::RegisterBind("bind_fullscreen", "F11", "Toggle fullscreen", [this] {
      if (window()) window()->SetFullscreen(!window()->IsFullscreen());
    });
  }
};

REX_DEFINE_APP(crackdown_tu, CrackdownTuApp::Create)
