#include "generated/default/crackdown_init.h"
#include <rex/rex_app.h>
#include "input_replay.h"
class CrackdownTuApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;
  static std::unique_ptr<rex::ui::WindowedApp> Create(rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<CrackdownTuApp>(new CrackdownTuApp(ctx, "crackdown_tu", PPCImageConfig));
  }
  void OnPreLaunchModule() override {
    crackdown::SetInputReplayQuit([this] {
      app_context().CallInUIThreadDeferred([this] { if (window()) window()->RequestClose(); });
    });
  }
};
REX_DEFINE_APP(crackdown_tu, CrackdownTuApp::Create)
