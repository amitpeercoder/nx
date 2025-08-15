#include "temp_directory.hpp"

#include <random>

namespace nx::test {

TempDirectory::TempDirectory() {
  createTempDir();
}

TempDirectory::~TempDirectory() {
  cleanup();
}

void TempDirectory::createTempDir() {
  auto base = std::filesystem::temp_directory_path() / "nx_test";
  
  // Generate random suffix
  std::random_device rd;
  std::mt19937 gen(rd());
  std::uniform_int_distribution<> dis(0, 999999);
  
  path_ = base / ("tmp_" + std::to_string(dis(gen)));
  
  // Ensure it doesn't exist already
  while (std::filesystem::exists(path_)) {
    path_ = base / ("tmp_" + std::to_string(dis(gen)));
  }
  
  std::filesystem::create_directories(path_);
}

void TempDirectory::cleanup() {
  if (!path_.empty() && std::filesystem::exists(path_)) {
    std::filesystem::remove_all(path_);
  }
}

std::filesystem::path TempDirectory::createSubdir(const std::string& name) {
  auto subdir = path_ / name;
  std::filesystem::create_directories(subdir);
  return subdir;
}

std::filesystem::path TempDirectory::createFile(const std::string& name, const std::string& content) {
  auto file_path = path_ / name;
  std::ofstream file(file_path);
  if (file) {
    file << content;
  }
  return file_path;
}

}  // namespace nx::test