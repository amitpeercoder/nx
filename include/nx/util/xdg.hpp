#pragma once

#include <filesystem>
#include <string>

namespace nx::util {

// XDG Base Directory Specification utilities
class Xdg {
 public:
  // Get XDG data home directory (~/.local/share/nx)
  static std::filesystem::path dataHome();

  // Get XDG config home directory (~/.config/nx)
  static std::filesystem::path configHome();

  // Get XDG cache home directory (~/.cache/nx)
  static std::filesystem::path cacheHome();

  // Get XDG runtime directory (for temporary files)
  static std::filesystem::path runtimeDir();

  // Ensure directory exists with proper permissions
  static bool ensureDirectory(const std::filesystem::path& path, std::filesystem::perms perms);

  // Get notes directory
  static std::filesystem::path notesDir();

  // Get attachments directory
  static std::filesystem::path attachmentsDir();

  // Get nx internal directory (.nx)
  static std::filesystem::path nxDir();

  // Get config file path
  static std::filesystem::path configFile();

  // Get index database path
  static std::filesystem::path indexFile();

  // Get trash directory
  static std::filesystem::path trashDir();

 private:
  // Get environment variable with default
  static std::string getEnvVar(const std::string& name, const std::string& default_value);
};

}  // namespace nx::util