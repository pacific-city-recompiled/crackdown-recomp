#include "pc_settings.h"
#include "../common/toml_edit.h"
#include <initializer_list>
#include <map>
#include <optional>
#include <stdexcept>
#include <vector>

namespace crackdown {
namespace {
// One key edit: value nullptr removes the key (its default).
struct Edit {
  const char* key;
  const char* value;
};
// Each button writes the same keys the launcher writes for that choice (crackdown/launcher
// main.cpp), so both edit crackdown.toml alike. Advanced overrides the launcher leaves alone
// (custom fps_cap, video_mode_*) stay untouched unless a choice owns that key.
struct Row {
  const char16_t* id;
  std::vector<std::vector<Edit>> choices;
};
const std::vector<Row>& Rows() {
  static const std::vector<Row> rows = {
      {u"framerate",
       {{{"vsync", nullptr}, {"fps_cap", nullptr}},
        {{"vsync", "true"}, {"fps_cap", nullptr}},
        {{"vsync", nullptr}, {"fps_cap", "120"}},
        {{"vsync", nullptr}, {"fps_cap", "144"}}}},
      {u"resolution",
       {{{"resolution_scale", nullptr}}, {{"resolution_scale", "2"}}, {{"resolution_scale", "3"}}}},
      {u"aspect",
       {{{"aspect_ratio", nullptr}},
        {{"aspect_ratio", "\"16:10\""}},
        {{"aspect_ratio", "\"21:9\""}},
        {{"aspect_ratio", "\"32:9\""}},
        {{"aspect_ratio", "\"4:3\""}}}},
      {u"antialiasing", {{{"swap_post_effect", nullptr}}, {{"swap_post_effect", "\"fxaa\""}}}},
      // anisotropic_override: -1 the game's own, 2 = 2x, 3 = 4x (SDK default), 4 = 8x, 5 = 16x.
      {u"filtering",
       {{{"anisotropic_override", "-1"}},
        {{"anisotropic_override", "2"}},
        {{"anisotropic_override", nullptr}},
        {{"anisotropic_override", "4"}},
        {{"anisotropic_override", "5"}}}},
      {u"display", {{{"fullscreen", nullptr}}, {{"fullscreen", "false"}}}},
  };
  return rows;
}
const Row& RowOf(PCSetting setting) {
  const auto index = static_cast<size_t>(setting);
  if (index >= Rows().size()) throw std::runtime_error("Invalid PC setting");
  return Rows()[index];
}
int IndexOf(std::initializer_list<bool> matches) {
  int i = 0;
  for (bool match : matches) {
    if (match) return i;
    ++i;
  }
  return -1;
}
}  // namespace

const char16_t* PCSettingId(PCSetting setting) { return RowOf(setting).id; }
int PCSettingChoices(PCSetting setting) { return static_cast<int>(RowOf(setting).choices.size()); }

PCSettingsStore& PCSettings() {
  static PCSettingsStore settings;
  return settings;
}

void PCSettingsStore::Initialize(const std::filesystem::path& path, const PCSettingsSnapshot& s) {
  std::lock_guard lock(mutex_);
  path_ = path;
  const int scale = s.resolution_scale;
  const int cap = s.fps_cap;
  const std::string& ratio = s.aspect_ratio;
  const int aniso = s.anisotropic_override;
  selections_ = {
      s.vsync ? 1 : IndexOf({cap == 60, false, cap == 120, cap == 144}),
      scale >= 1 && scale <= 3 ? scale - 1 : -1,
      IndexOf({ratio == "16:9", ratio == "16:10", ratio == "21:9", ratio == "32:9", ratio == "4:3"}),
      s.swap_post_effect == "none" ? 0 : s.swap_post_effect == "fxaa" ? 1 : -1,
      IndexOf({aniso == -1, aniso == 2, aniso == 3, aniso == 4, aniso == 5}),
      s.fullscreen ? 0 : 1,
  };
}

int PCSettingsStore::Selection(PCSetting setting) const {
  std::lock_guard lock(mutex_);
  return selections_.at(static_cast<size_t>(setting));
}

bool PCSettingsStore::Save(PCSetting setting, int selection) {
  std::lock_guard lock(mutex_);
  const Row& row = RowOf(setting);
  if (selection < 0 || selection >= static_cast<int>(row.choices.size()))
    throw std::runtime_error("Invalid PC selection");
  const auto index = static_cast<size_t>(setting);
  if (path_.empty()) throw std::runtime_error("PC settings not initialized");
  if (selections_[index] == selection) return false;
  std::map<std::string, std::optional<std::string>> changes;
  for (const Edit& edit : row.choices[static_cast<size_t>(selection)])
    changes[edit.key] = edit.value ? std::optional<std::string>(edit.value) : std::nullopt;
  config::update_config(path_, changes);
  selections_[index] = selection;
  return true;
}

std::string PCSettingsStore::Describe(PCSetting setting, int selection) {
  const Row& row = RowOf(setting);
  if (selection < 0 || selection >= static_cast<int>(row.choices.size())) return "?";
  std::string text;
  for (const Edit& edit : row.choices[static_cast<size_t>(selection)]) {
    if (!text.empty()) text += ' ';
    text += std::string(edit.key) + "=" + (edit.value ? edit.value : "default (key removed)");
  }
  return text;
}
}
