#include "gh_filesystem.h"

#include <fstream>
#include <iterator>
#include <utility>

namespace gh::filesystem {

Storage::Storage(std::filesystem::path game_data_root,
                 std::filesystem::path user_data_root)
    : game_data_root_(std::move(game_data_root)),
      user_data_root_(user_data_root.empty() ? game_data_root_ : std::move(user_data_root)) {}

const std::filesystem::path& Storage::game_data_root() const noexcept {
  return game_data_root_;
}

const std::filesystem::path& Storage::user_data_root() const noexcept {
  return user_data_root_;
}

bool Storage::ReadResource(std::string_view relative_path,
                           std::vector<uint8_t>& contents) const {
  contents.clear();
  std::ifstream input(game_data_root_ / relative_path, std::ios::binary);
  if (!input) return false;
  contents.assign(std::istreambuf_iterator<char>(input), {});
  return !input.bad();
}

bool Storage::ReadUserText(std::string_view relative_path,
                           std::string& contents) const {
  contents.clear();
  std::ifstream input(user_data_root_ / relative_path, std::ios::binary);
  if (!input) return false;
  contents.assign(std::istreambuf_iterator<char>(input), {});
  return !input.bad();
}

bool Storage::WriteUserText(std::string_view relative_path,
                            std::string_view contents) const {
  const auto destination = user_data_root_ / relative_path;
  std::error_code error;
  std::filesystem::create_directories(destination.parent_path(), error);
  if (error) return false;

  auto temporary = destination;
  temporary += ".tmp";
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output) return false;
    output.write(contents.data(), static_cast<std::streamsize>(contents.size()));
    output.close();
    if (!output) {
      std::filesystem::remove(temporary, error);
      return false;
    }
  }
  std::filesystem::rename(temporary, destination, error);
  if (error) {
    std::filesystem::remove(temporary, error);
    return false;
  }
  return true;
}

}  // namespace gh::filesystem
