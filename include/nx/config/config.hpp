#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include <unordered_map>

#include "nx/common.hpp"

namespace nx::config {

// Configuration for nx application
class Config {
 public:
  // Default constructor loads from default config file
  Config();
  
  // Load from specific file
  explicit Config(const std::filesystem::path& config_path);
  
  // Core paths
  std::filesystem::path root;
  std::filesystem::path data_dir;
  std::filesystem::path notes_dir;
  std::filesystem::path attachments_dir;
  std::filesystem::path trash_dir;
  std::filesystem::path index_file;
  
  // Editor configuration
  std::string editor;
  
  // Indexing configuration
  enum class IndexerType {
    kFts,      // SQLite FTS5
    kRipgrep   // Fallback to ripgrep
  };
  IndexerType indexer = IndexerType::kFts;
  
  // Encryption configuration
  enum class EncryptionType {
    kNone,
    kAge
  };
  EncryptionType encryption = EncryptionType::kNone;
  std::string age_recipient;
  
  // Sync configuration
  enum class SyncType {
    kNone,
    kGit
  };
  SyncType sync = SyncType::kNone;
  std::string git_remote;
  std::string git_user_name;
  std::string git_user_email;
  
  // Default values
  std::string default_notebook;
  std::vector<std::string> default_tags;
  
  // AI configuration (optional)
  struct AiConfig {
    std::string provider;          // "openai" or "anthropic"
    std::string model;
    std::string api_key;           // Can be "env:VARNAME" reference
    int max_tokens = 1200;
    double temperature = 0.2;
    int rate_limit_qpm = 20;
    double daily_usd_budget = 1.50;
    bool enable_embeddings = true;
    std::string embedding_model;
    int top_k = 6;
    
    // Redaction settings
    bool strip_emails = true;
    bool strip_urls = false;
    bool mask_numbers = true;
  };
  std::optional<AiConfig> ai;
  
  // Performance tuning
  struct PerformanceConfig {
    size_t cache_size_mb = 50;
    size_t max_file_size_mb = 10;
    int sqlite_cache_size = -20000;  // 20k pages
    std::string sqlite_journal_mode = "WAL";
    std::string sqlite_synchronous = "NORMAL";
    std::string sqlite_temp_store = "MEMORY";
  };
  PerformanceConfig performance;
  
  // Load configuration from file
  Result<void> load(const std::filesystem::path& config_path);
  
  // Save configuration to file
  Result<void> save(const std::filesystem::path& config_path = {}) const;
  
  // Get/set configuration values using dot notation
  Result<std::string> get(const std::string& key) const;
  Result<void> set(const std::string& key, const std::string& value);
  
  // Validate configuration
  Result<void> validate() const;
  
  // Get default configuration file path
  static std::filesystem::path defaultConfigPath();
  
  // Create default configuration
  static Config createDefault();
  
  // Environment variable resolution
  std::string resolveEnvVar(const std::string& value) const;

 private:
  std::filesystem::path config_path_;
  
  // Convert enum values to/from strings
  static std::string indexerTypeToString(IndexerType type);
  static IndexerType stringToIndexerType(const std::string& str);
  
  static std::string encryptionTypeToString(EncryptionType type);
  static EncryptionType stringToEncryptionType(const std::string& str);
  
  static std::string syncTypeToString(SyncType type);
  static SyncType stringToSyncType(const std::string& str);
  
  // Dot notation helpers
  Result<std::string> getValueByPath(const std::vector<std::string>& path) const;
  Result<void> setValueByPath(const std::vector<std::string>& path, const std::string& value);
  
  std::vector<std::string> splitPath(const std::string& path) const;
};

}  // namespace nx::config