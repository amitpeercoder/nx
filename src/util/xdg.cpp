#include "nx/util/xdg.hpp"

#include <cstdlib>
#include <filesystem>

namespace nx::util {

std::filesystem::path Xdg::dataHome() {
  std::string xdg_data_home = getEnvVar("XDG_DATA_HOME", "");
  if (!xdg_data_home.empty()) {
    return std::filesystem::path(xdg_data_home) / "nx";
  }
  
  std::string home = getEnvVar("HOME", "");
  if (home.empty()) {
    return std::filesystem::current_path() / ".nx_data";
  }
  
  return std::filesystem::path(home) / ".local" / "share" / "nx";
}

std::filesystem::path Xdg::configHome() {
  std::string xdg_config_home = getEnvVar("XDG_CONFIG_HOME", "");
  if (!xdg_config_home.empty()) {
    return std::filesystem::path(xdg_config_home) / "nx";
  }
  
  std::string home = getEnvVar("HOME", "");
  if (home.empty()) {
    return std::filesystem::current_path() / ".nx_config";
  }
  
  return std::filesystem::path(home) / ".config" / "nx";
}

std::filesystem::path Xdg::cacheHome() {
  std::string xdg_cache_home = getEnvVar("XDG_CACHE_HOME", "");
  if (!xdg_cache_home.empty()) {
    return std::filesystem::path(xdg_cache_home) / "nx";
  }
  
  std::string home = getEnvVar("HOME", "");
  if (home.empty()) {
    return std::filesystem::current_path() / ".nx_cache";
  }
  
  return std::filesystem::path(home) / ".cache" / "nx";
}

std::filesystem::path Xdg::runtimeDir() {
  std::string xdg_runtime_dir = getEnvVar("XDG_RUNTIME_DIR", "");
  if (!xdg_runtime_dir.empty()) {
    return std::filesystem::path(xdg_runtime_dir) / "nx";
  }
  
  // Fallback to temp directory
  return std::filesystem::temp_directory_path() / "nx";
}

bool Xdg::ensureDirectory(const std::filesystem::path& path, std::filesystem::perms perms) {
  std::error_code ec;
  
  if (std::filesystem::exists(path, ec)) {
    return !ec;
  }
  
  if (!std::filesystem::create_directories(path, ec)) {
    return false;
  }
  
  std::filesystem::permissions(path, perms, ec);
  return !ec;
}

std::filesystem::path Xdg::notesDir() {
  return dataHome() / "notes";
}

std::filesystem::path Xdg::attachmentsDir() {
  return dataHome() / "attachments";
}

std::filesystem::path Xdg::nxDir() {
  return dataHome() / ".nx";
}

std::filesystem::path Xdg::configFile() {
  return configHome() / "config.toml";
}

std::filesystem::path Xdg::indexFile() {
  return nxDir() / "index.sqlite";
}

std::filesystem::path Xdg::trashDir() {
  return nxDir() / "trash";
}

std::string Xdg::getEnvVar(const std::string& name, const std::string& default_value) {
  const char* value = std::getenv(name.c_str());
  return value ? std::string(value) : default_value;
}

}  // namespace nx::util