#include "pc_settings.h"
#include "../common/toml_edit.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <random>
#include <stdexcept>

namespace fs = std::filesystem;
using crackdown::PCSetting;
using crackdown::config::read_config;
using crackdown::config::setting;
namespace {
void Require(bool value, const char* why) { if (!value) throw std::runtime_error(why); }
void Write(const fs::path& path, const std::string& text) {
  std::ofstream out(path, std::ios::binary); out << text;
  if (!out) throw std::runtime_error("fixture write failed");
}
std::string Read(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(in), {}};
}
template <class F> void Reject(F action) {
  bool rejected = false;
  try { action(); } catch (const std::exception&) { rejected = true; }
  Require(rejected, "expected rejected save");
}
struct Temporary {
  fs::path path;
  Temporary() {
    std::random_device random;
    for (int i = 0; i < 16; ++i) {
      path = fs::temp_directory_path() / ("crackdown-pc-settings-test-" + std::to_string(random()));
      if (fs::create_directory(path)) return;
    }
    throw std::runtime_error("cannot create test directory");
  }
  ~Temporary() { std::error_code ec; fs::remove_all(path, ec); }
};
}
int main() {
  try {
    Temporary tmp;
    const auto path = tmp.path / "crackdown.toml";
    const std::string other = "fps_cap = 75 # custom cap\r\ndraw_resolution_scale_x = 3\r\n"
                              "name = '\xC3\xA9' # Unicode\r\n[unknown]\r\nvalue = 'literal'\r\n";
    const std::string original = "# retain\r\nvsync = false # frame comment\r\nfullscreen = true\r\n"
                                 "resolution_scale = 1\r\n" + other;
    Write(path, original);
    crackdown::PCSettingsStore store;
    store.Initialize(path, {false, 1, "16:9", "none", true});
    Require(!store.Save(PCSetting::framerate, 0) && Read(path) == original, "unchanged notify writes nothing");
    Require(store.Save(PCSetting::framerate, 1), "30 fps save");
    Require(Read(path) == std::string("# retain\r\nvsync = true # frame comment\r\nfullscreen = true\r\nresolution_scale = 1\r\n")+other,
            "changed key only; CRLF/comments/unknown overrides preserved");
    Require(store.Selection(PCSetting::framerate) == 1, "reopen keeps pending selection");
    Require(store.Save(PCSetting::resolution, 1) && store.Save(PCSetting::display, 1), "other saves");
    auto table = read_config(path);
    Require(setting(table, "vsync")->value<bool>() == true &&
            setting(table, "resolution_scale")->value<int64_t>() == 2 &&
            setting(table, "fullscreen")->value<bool>() == false, "correct override mapping");
    Require(Read(path).ends_with(other), "other settings never touched");
    for (auto key : {PCSetting::framerate, PCSetting::resolution, PCSetting::display})
      Require(store.Save(key, 0), "restore default");
    table = read_config(path);
    Require(!setting(table, "vsync") && !setting(table, "resolution_scale") && !setting(table, "fullscreen"), "default keys removed");
    Require(Read(path).find("# frame comment") != std::string::npos && Read(path).ends_with(other), "removal keeps comments");
    Write(path, "bad = [");
    Reject([&] { store.Save(PCSetting::display, 1); });
    Require(store.Selection(PCSetting::display) == 0 && Read(path) == "bad = [", "failed save retains state and source");
    Reject([&] { store.Save(PCSetting::display, 2); });
    Reject([&] { store.Save(PCSetting::resolution, 3); });
    Reject([&] { store.Save(static_cast<PCSetting>(99), 1); });
    fs::remove(path);
    Require(store.Save(PCSetting::display, 1) && Read(path) == "fullscreen = false\n", "create adjacent config if absent");
    store.Initialize(path, {true, 4, "4:3", "fxaa_extreme", false});
    Require(store.Selection(PCSetting::resolution) == -1 && store.Selection(PCSetting::aspect) == -1 &&
            store.Selection(PCSetting::antialiasing) == -1, "unsupported values not silently presented as presets");
    Require(store.Save(PCSetting::resolution, 0), "deliberate preset from unsupported scale");
    { // 1.0 rows: 4K, aspect ratio and FXAA.
      Write(path, "");
      store.Initialize(path, {false, 1, "16:9", "none", true});
      Require(store.Save(PCSetting::resolution, 2) && store.Save(PCSetting::aspect, 1) &&
              store.Save(PCSetting::antialiasing, 1), "1.0 row saves");
      auto t = read_config(path);
      Require(setting(t, "resolution_scale")->value<int64_t>() == 3 &&
              setting(t, "aspect_ratio")->value<std::string>() == "21:9" &&
              setting(t, "swap_post_effect")->value<std::string>() == "fxaa", "1.0 row values");
      Require(store.Save(PCSetting::aspect, 2) && setting(read_config(path), "aspect_ratio")->value<std::string>() == "32:9", "32:9");
      for (auto key : {PCSetting::resolution, PCSetting::aspect, PCSetting::antialiasing}) Require(store.Save(key, 0), "1.0 defaults");
      Require(Read(path).empty(), "1.0 defaults remove their lines");
      store.Initialize(path, {false, 3, "21:9", "fxaa", true});
      Require(store.Selection(PCSetting::resolution) == 2 && store.Selection(PCSetting::aspect) == 1 &&
              store.Selection(PCSetting::antialiasing) == 1, "1.0 startup values selected");
    }
    Write(path, "[gpu]\nresolution_scale = 2 # retain\n[other]\nvalue = 9\n");
    store.Initialize(path, {false, 2, "16:9", "none", true});
    Require(store.Save(PCSetting::resolution, 0) && !setting(read_config(path), "resolution_scale"), "default leaves unrelated gpu table setting");
    // SDK/launcher flatten table names with underscores; unrelated gpu_* is retained.
    Require(Read(path).find("resolution_scale = 2 # retain") != std::string::npos, "unrelated table key preserved");
    Write(path, "[resolution]\nscale = 2 # retain\n[other]\nvalue = 9\n");
    store.Initialize(path, {false, 2, "16:9", "none", true});
    Require(store.Save(PCSetting::resolution, 0) && !setting(read_config(path), "resolution_scale"), "flattened table default removed");
    Require(Read(path).find("# retain") != std::string::npos && Read(path).ends_with("[other]\nvalue = 9\n"), "table removal preserves surrounding source");
    Write(path, "resolution_scale = 2\n[resolution]\nscale = 2\n");
    const auto ambiguous = Read(path);
    store.Initialize(path, {false, 2, "16:9", "none", true});
    Reject([&] { store.Save(PCSetting::resolution, 0); });
    Require(Read(path) == ambiguous && store.Selection(PCSetting::resolution) == 1, "ambiguous config retains source/state");
    std::cout << "PC settings persistence tests passed\n";
  } catch (const std::exception& error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
