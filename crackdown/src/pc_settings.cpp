#include "pc_settings.h"
#include "../common/toml_edit.h"
#include <optional>
#include <stdexcept>
#include <vector>

namespace crackdown {
namespace {
struct Row {
  const char16_t* id;
  const char* key;
  // TOML values per button; nullptr = remove the key (the default). Advanced overrides
  // (fps_cap, per-axis scales, video_mode_*) are left alone, as in the launcher's docs.
  std::vector<const char*> values;
};
const std::vector<Row>& Rows() {
  static const std::vector<Row> rows = {
      {u"framerate", "vsync", {nullptr, "true"}},
      {u"resolution", "resolution_scale", {nullptr, "2", "3"}},
      {u"aspect", "aspect_ratio", {nullptr, "\"21:9\"", "\"32:9\""}},
      {u"antialiasing", "swap_post_effect", {nullptr, "\"fxaa\""}},
      {u"display", "fullscreen", {nullptr, "false"}},
  };
  return rows;
}
const Row& RowOf(PCSetting setting) {
  const auto index = static_cast<size_t>(setting);
  if (index >= Rows().size()) throw std::runtime_error("Invalid PC setting");
  return Rows()[index];
}
}  // namespace

const char16_t* PCSettingId(PCSetting setting) { return RowOf(setting).id; }
int PCSettingChoices(PCSetting setting) { return static_cast<int>(RowOf(setting).values.size()); }

PCSettingsStore& PCSettings() {
  static PCSettingsStore settings;
  return settings;
}

void PCSettingsStore::Initialize(const std::filesystem::path& path, const PCSettingsSnapshot& s) {
  std::lock_guard lock(mutex_);
  path_ = path;
  const int scale = s.resolution_scale;
  selections_ = {
      s.vsync ? 1 : 0,
      scale >= 1 && scale <= 3 ? scale - 1 : -1,
      s.aspect_ratio == "16:9" ? 0 : s.aspect_ratio == "21:9" ? 1 : s.aspect_ratio == "32:9" ? 2 : -1,
      s.swap_post_effect == "none" ? 0 : s.swap_post_effect == "fxaa" ? 1 : -1,
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
  if (selection < 0 || selection >= static_cast<int>(row.values.size()))
    throw std::runtime_error("Invalid PC selection");
  const auto index = static_cast<size_t>(setting);
  if (path_.empty()) throw std::runtime_error("PC settings not initialized");
  if (selections_[index] == selection) return false;
  const char* value = row.values[static_cast<size_t>(selection)];
  config::update_config(path_, {{row.key, value ? std::optional<std::string>(value) : std::nullopt}});
  selections_[index] = selection;
  return true;
}

std::string PCSettingsStore::Describe(PCSetting setting, int selection) {
  const Row& row = RowOf(setting);
  if (selection < 0 || selection >= static_cast<int>(row.values.size())) return "?";
  const char* value = row.values[static_cast<size_t>(selection)];
  return std::string(row.key) + "=" + (value ? value : "default (key removed)");
}
}
