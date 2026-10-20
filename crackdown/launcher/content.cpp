// Xbox 360 STFS packages: read-only reader (volume descriptor at 0x379, hash tables every 170 blocks) and the
// two installs the launcher does with it (title update, DLC). Same reader as the Crackdown 1 DLC installer on the
// 1.1 branch, with the title id as a parameter.
#include "content.h"
#include "TinySHA1.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <cstring>
#include <fstream>
#include <stdexcept>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace launcher {
namespace {
constexpr uint32_t kMarketplace = 2, kTitleUpdate = 0xB0000, kEnd = 0xFFFFFF, kBlock = 0x1000;
constexpr uint32_t kPerLevel[3] = {170, 170 * 170, 170 * 170 * 170};
constexpr size_t kHeaderSize = 0x148;  // XCONTENT_AGGREGATE_DATA
uint32_t be32(const unsigned char* p) { return uint32_t(p[0]) << 24 | uint32_t(p[1]) << 16 | uint32_t(p[2]) << 8 | p[3]; }
uint32_t be24(const unsigned char* p) { return uint32_t(p[0]) << 16 | uint32_t(p[1]) << 8 | p[2]; }
uint32_t le24(const unsigned char* p) { return uint32_t(p[2]) << 16 | uint32_t(p[1]) << 8 | p[0]; }
void put_be(std::string& s, size_t at, uint64_t v, int bytes) { for (int i = 0; i < bytes; ++i) s[at + i] = char(v >> (8 * (bytes - 1 - i))); }
void remove_quietly(const fs::path& path) { std::error_code ec; fs::remove_all(path, ec); }
void no_links(const fs::path& path) {
  if (fs::is_symlink(fs::symlink_status(path))) throw std::runtime_error("Symlink is not permitted: " + utf8(path));
#ifdef _WIN32
  auto attr = GetFileAttributesW(path.c_str());
  if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT))
    throw std::runtime_error("Reparse point is not permitted: " + utf8(path));
#endif
}
std::string hex_title(uint32_t title) { char s[9]; std::snprintf(s, sizeof(s), "%08X", title); return s; }
std::string lower(std::string s) { std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return char(std::tolower(c)); }); return s; }
std::string pid() {
#ifdef _WIN32
  return std::to_string(GetCurrentProcessId());
#else
  return std::to_string(getpid());
#endif
}
fs::path dlc_folder(const fs::path& user, uint32_t title) {
  auto root = fs::absolute(user).lexically_normal();
  fs::path p = root; no_links(p);
  for (const std::string& part : {std::string("0000000000000000"), hex_title(title)}) { p /= part; no_links(p); }
  return p;
}
std::string display_name(const unsigned char* utf16be, size_t chars) {
  std::string out;
  for (size_t i = 0; i < chars; ++i) {
    uint16_t c = uint16_t(utf16be[2 * i] << 8 | utf16be[2 * i + 1]);
    if (!c) break;
    out += c < 0x80 ? char(c) : '?';
  }
  return out;
}

class Stfs {
 public:
  Stfs(const fs::path& path, uint32_t title_id, uint32_t content_type)
      : in_(path, std::ios::binary), length_(fs::file_size(path)) {
    if (!in_) throw std::runtime_error("Cannot open " + utf8(path));
    if (length_ < 0x1000) throw std::runtime_error("Not an Xbox 360 content package");
    read(0, header_.data(), header_.size());
    std::string magic(reinterpret_cast<const char*>(header_.data()), 4);
    if (magic != "LIVE" && magic != "PIRS" && magic != "CON ") throw std::runtime_error("Not an Xbox 360 content package");
    if (be32(&header_[0x360]) != title_id) throw std::runtime_error("This package is for another game");
    if (be32(&header_[0x344]) != content_type)
      throw std::runtime_error(content_type == kMarketplace
                                   ? "This package is not downloadable content (it may be a save or a title update)"
                                   : "This package is not a title update");
    if (be32(&header_[0x3A9]) != 0) throw std::runtime_error("Unsupported package format (SVOD)");
    const unsigned char* vd = &header_[0x379];
    read_only_ = vd[2] & 1; root_active_ = vd[2] & 2;
    table_blocks_ = uint16_t(vd[3] | vd[4] << 8); table_start_ = le24(vd + 5); total_blocks_ = be32(vd + 0x1C);
    tables_ = read_only_ ? 1 : 2;
    base_ = (uint64_t(be32(&header_[0x340])) + 0xFFF) & ~uint64_t(0xFFF);
    title = display_name(&header_[0x411], 128);
    for (int i = 0; i < 16; ++i) {
      const unsigned char* l = &header_[0x22C + 16 * i];
      if (be32(l + 12)) license |= be32(l + 8);
    }
  }
  struct File { std::string path; bool directory; uint32_t start, length; };
  std::vector<File> files() {
    std::vector<File> result;
    uint32_t block = table_start_;
    for (uint32_t n = 0; n < table_blocks_ && block != kEnd; ++n) {
      std::array<unsigned char, kBlock> table; read_block(block, table.data());
      for (size_t m = 0; m < kBlock / 0x40; ++m) {
        const unsigned char* e = &table[m * 0x40];
        if (!e[0]) break;
        std::string name(reinterpret_cast<const char*>(e), e[0x28] & 0x3F);
        if (!safe_name(name)) throw std::runtime_error("Package has an unsafe file name");
        uint16_t parent = uint16_t(e[0x32] << 8 | e[0x33]);
        std::string path = name;
        if (parent != 0xFFFF) {
          if (parent >= result.size() || !result[parent].directory) throw std::runtime_error("Package directory table is damaged");
          path = result[parent].path + "/" + name;
        }
        result.push_back({path, (e[0x28] & 0x80) != 0, le24(e + 0x2F), be32(e + 0x34)});
      }
      block = next(block);
    }
    return result;
  }
  // Follows the block chain and checks every block's SHA-1 against the package's hash tables. A mismatch throws,
  // or with `damaged` set is counted there and the block kept as it is.
  uint32_t* damaged = nullptr;
  void copy(const File& file, std::ofstream& out, const std::atomic_bool& cancel, const std::function<void(uint64_t)>& done) {
    uint32_t block = file.start, remaining = file.length;
    std::array<unsigned char, kBlock> data;
    for (uint32_t guard = 0; remaining; ++guard) {
      if (cancel) throw std::runtime_error("Cancelled");
      if (block == kEnd || guard > total_blocks_) throw std::runtime_error("Package file " + file.path + " is truncated");
      read_block(block, data.data());
      auto entry = hash_entry(block);
      sha1::SHA1 sha; sha.processBytes(data.data(), data.size());
      sha1::SHA1::digest8_t digest; sha.finalize(digest);
      if (std::memcmp(entry.data(), digest, sizeof(digest)) != 0 && damaged) ++*damaged;
      else if (std::memcmp(entry.data(), digest, sizeof(digest)) != 0) throw std::runtime_error("Package is damaged (hash mismatch in " + file.path + ")");
      uint32_t n = (std::min)(remaining, kBlock);
      out.write(reinterpret_cast<const char*>(data.data()), n);
      remaining -= n; done(n);
      block = be24(&entry[0x15]);
    }
  }
  // Writes every file under `folder` (which must not exist yet).
  void extract(const fs::path& folder, const std::atomic_bool& cancel, const Progress& progress) {
    auto list = files(); uint64_t total = 0, written = 0;
    for (const auto& f : list) if (!f.directory) total += f.length;
    fs::create_directory(folder);
    for (const auto& f : list) {
      auto target = folder / fs::u8path(f.path);
      if (f.directory) { fs::create_directories(target); continue; }
      fs::create_directories(target.parent_path());
      std::ofstream out(target, std::ios::binary);
      if (!out) throw std::runtime_error("Cannot write " + utf8(target));
      copy(f, out, cancel, [&](uint64_t n) { written += n; if (progress) progress(written, total); });
      out.close(); if (!out) throw std::runtime_error("Cannot write " + utf8(target));
    }
  }
  std::string title; uint32_t license = 0;

 private:
  void read(uint64_t off, void* out, size_t size) {
    if (off > length_ || size > length_ - off) throw std::runtime_error("Package is truncated");
    in_.clear(); in_.seekg(static_cast<std::streamoff>(off));
    if (!in_.read(static_cast<char*>(out), static_cast<std::streamsize>(size))) throw std::runtime_error("Cannot read package");
  }
  uint64_t block_offset(uint64_t index) const {
    uint64_t block = index, base = kPerLevel[0];
    for (int level = 0; level < 3; ++level) {
      block += ((index + base) / base) * tables_;
      if (index < base) break;
      base *= kPerLevel[0];
    }
    return base_ + (block << 12);
  }
  uint64_t hash_table_offset(uint32_t index, int level) const {
    uint64_t step0 = kPerLevel[0] + tables_, step1 = kPerLevel[1] + uint64_t(kPerLevel[0] + 1) * tables_, block;
    if (level == 0) {
      if (index < kPerLevel[0]) block = 0;
      else {
        block = (index / kPerLevel[0]) * step0 + ((index / kPerLevel[1]) + 1) * tables_;
        if (index >= kPerLevel[1]) block += tables_;
      }
    } else block = index < kPerLevel[1] ? step0 : (index / kPerLevel[1]) * step1 + tables_;
    return base_ + (block << 12);
  }
  std::array<unsigned char, 0x18> hash_entry(uint32_t index) {
    uint64_t secondary = 0;
    if (!read_only_) {
      // Writable packages keep two copies of each table; the level above says which one is live.
      secondary = root_active_ ? kBlock : 0;
      if (total_blocks_ > kPerLevel[0]) {
        std::array<unsigned char, 0x18> upper;
        read(hash_table_offset(index, 1) + secondary + ((index / kPerLevel[0]) % kPerLevel[0]) * 0x18, upper.data(), upper.size());
        secondary = (upper[0x14] & 0x40) ? kBlock : 0;
      }
    }
    std::array<unsigned char, 0x18> entry;
    read(hash_table_offset(index, 0) + secondary + (index % kPerLevel[0]) * 0x18, entry.data(), entry.size());
    return entry;
  }
  uint32_t next(uint32_t block) { return be24(&hash_entry(block)[0x15]); }
  void read_block(uint32_t index, unsigned char* out) {
    if (index >= total_blocks_) throw std::runtime_error("Package block out of range");
    read(block_offset(index), out, kBlock);
  }
  std::ifstream in_; uint64_t length_, base_ = 0;
  std::array<unsigned char, 0x1000> header_{};
  bool read_only_ = false, root_active_ = false; uint32_t tables_ = 1, table_blocks_ = 0, table_start_ = 0, total_blocks_ = 0;
};
}  // namespace

std::vector<ContentPack> list_content_dlc(const fs::path& user, uint32_t title_id) {
  std::vector<ContentPack> result;
  try {
    auto root = dlc_folder(user, title_id); std::error_code ec;
    for (const auto& entry : fs::directory_iterator(root / "00000002", ec)) {
      if (!entry.is_directory(ec) || fs::is_symlink(entry.symlink_status(ec))) continue;
      auto name = utf8(entry.path().filename());
      if (name.starts_with(".install-")) continue;
      ContentPack pack{name, {}};
      std::ifstream in(root / "Headers" / "00000002" / fs::u8path(name + ".header"), std::ios::binary);
      std::array<unsigned char, kHeaderSize> h{};
      if (in.read(reinterpret_cast<char*>(h.data()), h.size())) pack.title = display_name(&h[8], 128);
      if (pack.title.empty()) pack.title = pack.file_name;
      result.push_back(std::move(pack));
    }
  } catch (const std::exception&) {}
  std::sort(result.begin(), result.end(), [](const ContentPack& a, const ContentPack& b) { return a.title < b.title; });
  return result;
}

ContentPack install_content_dlc(const fs::path& package, const fs::path& user, uint32_t title_id,
                                const std::atomic_bool& cancel, Progress progress) {
  // The SDK finds the pack by its original package name (at most 42 characters, the XCONTENT file name field).
  auto name = utf8(package.filename());
  if (!safe_name(name) || name.size() > 42) throw std::runtime_error("Keep the package's original file name (up to 42 characters)");
  Stfs stfs(package, title_id, kMarketplace);
  // Some console dumps have a damaged block (a known Crackdown 2 Toy Box dump has one, inside a texture pack, and
  // the game runs fine with it), so DLC installs anyway and reports it, as the Crackdown 2 setup script does.
  uint32_t damaged = 0; stfs.damaged = &damaged;
  auto root = dlc_folder(user, title_id);
  fs::create_directories(root / "00000002"); no_links(root / "00000002");
  fs::create_directories(root / "Headers" / "00000002"); no_links(root / "Headers"); no_links(root / "Headers" / "00000002");
  auto destination = root / "00000002" / fs::u8path(name);
  auto staging = root / "00000002" / fs::u8path(".install-" + pid() + "-" + name);
  remove_quietly(staging);
  try {
    stfs.extract(staging, cancel, progress);
    no_links(destination);
    remove_quietly(destination);
    fs::rename(staging, destination);
  } catch (...) { remove_quietly(staging); throw; }

  // XCONTENT_AGGREGATE_DATA (big-endian, as the SDK writes it) + the package's license bits.
  std::string header(kHeaderSize, '\0');
  put_be(header, 0, 1, 4);  // device: HDD
  put_be(header, 4, kMarketplace, 4);
  for (size_t i = 0; i < stfs.title.size() && i < 127; ++i) put_be(header, 8 + 2 * i, uint8_t(stfs.title[i]), 2);
  std::memcpy(&header[0x108], name.data(), name.size());
  put_be(header, 0x140, title_id, 4);
  for (int i = 0; i < 4; ++i) header += char(stfs.license >> (8 * i));
  auto header_path = root / "Headers" / "00000002" / fs::u8path(name + ".header");
  no_links(header_path);
  std::ofstream out(header_path, std::ios::binary | std::ios::trunc);
  out.write(header.data(), static_cast<std::streamsize>(header.size())); out.close();
  if (!out) throw std::runtime_error("Cannot write " + utf8(header_path));
  ContentPack result{name, stfs.title.empty() ? name : stfs.title, damaged};
  return result;
}

std::string install_content_update(const fs::path& package, const fs::path& home, const fs::path& game,
                                   uint32_t title_id, const std::string& xexp_sha256,
                                   const std::atomic_bool& cancel, Progress progress) {
  Stfs stfs(package, title_id, kTitleUpdate);
  auto root = fs::absolute(home).lexically_normal(); no_links(root);
  if (!fs::is_regular_file(game / "default.xex")) throw std::runtime_error("Install the game files first, then the title update");
  auto destination = root / "tu", staging = root / ("tu.installing-" + pid());
  remove_quietly(staging);
  try {
    stfs.extract(staging, cancel, progress);
    fs::path xexp;
    for (const auto& item : fs::directory_iterator(staging))
      if (item.is_regular_file() && lower(utf8(item.path().filename())) == "default.xexp") xexp = item.path();
    if (xexp.empty()) throw std::runtime_error("The title update has no default.xexp. Nothing was installed.");
    if (hash_file(xexp, &cancel) != xexp_sha256)
      throw std::runtime_error("This is not the title update this build is made for. Nothing was installed.");
    // default.xexp beside default.xex (the SDK applies it at load), through a temporary name so a failed copy
    // never leaves a partial patch.
    auto patch = game / "default.xexp", partial = game / ("default.xexp.installing-" + pid());
    no_links(game); no_links(patch);
    fs::copy_file(xexp, partial, fs::copy_options::overwrite_existing);
    fs::rename(partial, patch);
    no_links(destination);
    remove_quietly(destination);
    fs::rename(staging, destination);
  } catch (...) { remove_quietly(staging); throw; }
  return stfs.title.empty() ? "Title update" : stfs.title;
}
}  // namespace launcher
