#pragma once
// Xbox 360 content packages (STFS) the launcher installs for a game: its title update and its DLC packs.
#include "core.h"
#include <cstdint>
#include <string>
#include <vector>

namespace launcher {
struct ContentPack { std::string file_name, title; uint32_t damaged_blocks = 0; };

// DLC packs installed under <user>/0000000000000000/<title id>/00000002 (the layout the SDK's content manager
// mounts; the launcher passes --dlc_root=<user> so every save profile sees them).
std::vector<ContentPack> list_content_dlc(const fs::path& user, uint32_t title_id);
// Unpacks one Marketplace package (content type 00000002) of that title, checking every block's SHA-1, plus the
// .header the SDK reads (XCONTENT_AGGREGATE_DATA + license bits). Keeps the package's original file name. Blocks
// whose SHA-1 doesn't match are kept and counted in damaged_blocks (a title update must have none).
ContentPack install_content_dlc(const fs::path& package, const fs::path& user, uint32_t title_id,
                                const std::atomic_bool& cancel, Progress progress = {});

// Title update package (content type 000B0000) of that title: its files go to <home>/tu (passed as
// --update_data_root) and its default.xexp, which must have the given SHA256, next to the disc's default.xex.
// Unpacked through staging folders; returns the package title.
// beside_xex false (a game whose update has its own exe): default.xexp stays in tu/ only.
std::string install_content_update(const fs::path& package, const fs::path& home, const fs::path& game,
                                   uint32_t title_id, const std::string& xexp_sha256,
                                   const std::atomic_bool& cancel, Progress progress = {},
                                   bool beside_xex = true);
}  // namespace launcher
