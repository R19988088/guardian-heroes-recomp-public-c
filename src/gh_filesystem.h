#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace gh::filesystem {

// Project-owned access to immutable game resources and writable user data.
// Paths passed to the operations are relative to their respective roots.
class Storage {
 public:
  Storage(std::filesystem::path game_data_root,
          std::filesystem::path user_data_root);

  const std::filesystem::path& game_data_root() const noexcept;
  const std::filesystem::path& user_data_root() const noexcept;
  bool ReadResource(std::string_view relative_path,
                    std::vector<uint8_t>& contents) const;
  bool ReadUserText(std::string_view relative_path, std::string& contents) const;
  bool WriteUserText(std::string_view relative_path, std::string_view contents) const;

 private:
  std::filesystem::path game_data_root_;
  std::filesystem::path user_data_root_;
};

}  // namespace gh::filesystem
