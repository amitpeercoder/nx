#pragma once

#include <filesystem>
#include <fstream>
#include <string>

namespace nx::test {

// RAII temporary directory for testing
class TempDirectory {
 public:
  TempDirectory();
  ~TempDirectory();

  // Non-copyable, movable
  TempDirectory(const TempDirectory&) = delete;
  TempDirectory& operator=(const TempDirectory&) = delete;
  TempDirectory(TempDirectory&&) = default;
  TempDirectory& operator=(TempDirectory&&) = default;

  // Get the temporary directory path
  const std::filesystem::path& path() const { return path_; }

  // Create subdirectory
  std::filesystem::path createSubdir(const std::string& name);

  // Create file with content
  std::filesystem::path createFile(const std::string& name, const std::string& content = "");

  // Manual cleanup (called automatically in destructor)
  void cleanup();

 private:
  std::filesystem::path path_;
  
  void createTempDir();
};

}  // namespace nx::test