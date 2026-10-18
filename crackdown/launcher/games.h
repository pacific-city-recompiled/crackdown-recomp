#pragma once
// The games this launcher manages. Crackdown 1 keeps the package root as its folder (existing installs need no
// migration); Crackdown 2 lives in cd2/ with its own exe, SDK DLLs, settings, manifest and game files, because its
// runtime DLLs differ from Crackdown 1's (separate SDK patch set) and share their names.
#include "core.h"
#include <string>

namespace launcher {
struct GameProfile {
  const char* id;           // launcher.toml selected_game value
  const char* title;        // header button, logo fallback, About
  const char* folder;       // package subfolder ("" = package root)
  const char* exe;          // in that folder
  const char* config;       // the game's settings file, next to its exe
  const char* manifest;     // disc file list (sizes + SHA256), next to its exe
  const char* user_folder;  // Documents\<name>: the SDK's default user data folder for that exe
  const char* log_prefix;   // <folder>/logs/<prefix>NNN.log
  const char* xex_hash;     // SHA256 of the disc's default.xex
  const char* profile_key;  // launcher.toml key for the selected save profile
  const char* disc_hint;
  bool coop, archipelago, title_update, steam;
};

// Crackdown 2: World multi-language disc (XGD2), default.xex sha256 below. It runs with title update 0.0.5.1
// applied: default.xexp next to default.xex, the update's other files in game/TU and tu/ (passed as
// --update_data_root). See github.com/Unowa001/Crackdown-2-Recomp README.
inline constexpr GameProfile kGames[] = {
    {"cd1", "CRACKDOWN", "", "crackdown.exe", "crackdown.toml", "game-manifest.txt", "crackdown", "crackdown_",
     supported_xex, "profile", "Select your supported World multi-language disc image or an extracted game folder.",
     true, true, false, true},
    {"cd2", "CRACKDOWN 2", "cd2", "crackdown2.exe", "crackdown2.toml", "game-manifest.txt", "crackdown2",
     "crackdown2_", "8959c449e382661d29b8ea479c685f506cd5bd3eca40b3d73a51cc10f281069f", "profile_cd2",
     "Select your Crackdown 2 World multi-language disc image or an extracted game folder. The title update "
     "(0.0.5.1) is required: put default.xexp next to default.xex and the update's files in game\\TU.",
     false, false, true, false},
};
inline constexpr int kGameCount = int(sizeof(kGames) / sizeof(kGames[0]));
inline int game_index(const std::string& id) {
  for (int i = 0; i < kGameCount; ++i)
    if (id == kGames[i].id) return i;
  return 0;
}
inline fs::path game_home(const fs::path& package, const GameProfile& g) {
  return *g.folder ? package / g.folder : package;
}
}  // namespace launcher
