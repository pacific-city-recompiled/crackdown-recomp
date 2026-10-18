// Pending selections: never mutate live SDK cvars from a guest thread.
#pragma once
#include <array>
#include <cstddef>
#include <filesystem>
#include <mutex>
#include <string>

namespace crackdown {
// Rows of the in-game PC Settings page (screen 26), in page order. The radio group Ids in
// options.xur (tools/xui/mod_menus.py) match PCSettingId().
enum class PCSetting : size_t { framerate, resolution, aspect, antialiasing, display, count };
inline constexpr size_t kPCSettingCount = static_cast<size_t>(PCSetting::count);
const char16_t* PCSettingId(PCSetting setting);
// Number of radio buttons in the row.
int PCSettingChoices(PCSetting setting);

// Effective startup values, read after ApplyGameDefaults.
struct PCSettingsSnapshot {
  bool vsync = false;
  int resolution_scale = 1;
  std::string aspect_ratio = "16:9";
  std::string swap_post_effect = "none";
  bool fullscreen = true;
};

class PCSettingsStore {
 public:
  void Initialize(const std::filesystem::path& path, const PCSettingsSnapshot& snapshot);
  // -1 when the startup value matches no button (an advanced override, kept until a
  // button is deliberately chosen).
  int Selection(PCSetting setting) const;
  // Saves the row's key (choice 0 removes it, restoring the default) plus any key it
  // supersedes. Returns false for no change. Throws on failure; pending state advances
  // only after a successful file edit.
  bool Save(PCSetting setting, int selection);
  // "key=value" of a choice, for logs.
  static std::string Describe(PCSetting setting, int selection);
 private:
  mutable std::mutex mutex_;
  std::filesystem::path path_;
  std::array<int, kPCSettingCount> selections_{};
};
PCSettingsStore& PCSettings();
}
