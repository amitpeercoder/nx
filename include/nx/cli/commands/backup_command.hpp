#pragma once

#include "nx/cli/application.hpp"
#include <nlohmann/json.hpp>

namespace nx::cli {

class BackupCommand : public Command {
public:
  explicit BackupCommand(Application& app);

  std::string name() const override { return "backup"; }
  std::string description() const override { return "Create and restore note backups"; }

  Result<int> execute(const GlobalOptions& options) override;
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  
  // Command options
  std::string subcommand_;
  std::string backup_path_;
  std::string restore_path_;
  bool include_attachments_ = true;
  bool include_index_ = false;
  bool include_config_ = false;
  bool force_ = false;
  std::string compression_ = "gzip";
  std::vector<std::string> exclude_patterns_;
  
  // Subcommand implementations
  Result<int> executeCreate();
  Result<int> executeList();
  Result<int> executeRestore();
  Result<int> executeVerify();
  Result<int> executeCleanup();
  
  // Helper methods
  struct BackupInfo {
    std::filesystem::path path;
    std::chrono::system_clock::time_point created;
    size_t size_bytes;
    size_t note_count;
    bool has_attachments;
    bool has_index;
    bool has_config;
    std::string compression;
  };
  
  Result<BackupInfo> createBackup(const std::filesystem::path& target_path);
  Result<BackupInfo> getBackupInfo(const std::filesystem::path& backup_path);
  Result<std::vector<BackupInfo>> listBackups(const std::filesystem::path& backup_dir);
  Result<void> restoreFromBackup(const std::filesystem::path& backup_path, 
                                 const std::filesystem::path& target_dir);
  Result<bool> verifyBackup(const std::filesystem::path& backup_path);
  
  // Backup format utilities
  Result<void> createTarGz(const std::filesystem::path& source_dir, 
                          const std::filesystem::path& target_file);
  Result<void> extractTarGz(const std::filesystem::path& tar_file, 
                           const std::filesystem::path& target_dir);
  Result<std::string> generateBackupName();
  
  // Output formatters
  void outputBackupInfo(const BackupInfo& info, const GlobalOptions& options);
  void outputBackupList(const std::vector<BackupInfo>& backups, const GlobalOptions& options);
  void outputProgress(const std::string& message, const GlobalOptions& options);
  
  // Metadata management
  Result<void> writeBackupMetadata(const std::filesystem::path& metadata_path, const BackupInfo& info);
  Result<nlohmann::json> loadBackupMetadata(const std::filesystem::path& metadata_path);
  
  // Utility methods
  std::string formatFileSize(std::uintmax_t bytes);
  std::string formatTime(const std::chrono::system_clock::time_point& tp);
};

} // namespace nx::cli