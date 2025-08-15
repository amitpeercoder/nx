#include "nx/config/config.hpp"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <iostream>

#include <toml++/toml.hpp>

#include "nx/util/xdg.hpp"

namespace nx::config {

Config::Config() {
  // Initialize defaults directly
  root = nx::util::Xdg::dataHome();
  notes_dir = nx::util::Xdg::notesDir();
  attachments_dir = nx::util::Xdg::attachmentsDir();
  trash_dir = nx::util::Xdg::trashDir();
  index_file = nx::util::Xdg::indexFile();
  
  // Set default editor
  editor = std::getenv("VISUAL") ? std::getenv("VISUAL") :
           (std::getenv("EDITOR") ? std::getenv("EDITOR") : "vi");
  
  // Set default values
  indexer = IndexerType::kFts;
  encryption = EncryptionType::kNone;
  sync = SyncType::kNone;
  
  // Try to load from default location
  auto default_path = defaultConfigPath();
  if (std::filesystem::exists(default_path)) {
    auto result = load(default_path);
    // If loading fails, silently continue with defaults
    (void)result;  // Suppress unused variable warning
  }
}

Config::Config(const std::filesystem::path& config_path) {
  // Initialize defaults directly instead of calling createDefault()
  root = nx::util::Xdg::dataHome();
  notes_dir = nx::util::Xdg::notesDir();
  attachments_dir = nx::util::Xdg::attachmentsDir();
  trash_dir = nx::util::Xdg::trashDir();
  index_file = nx::util::Xdg::indexFile();
  
  // Set default editor
  editor = std::getenv("VISUAL") ? std::getenv("VISUAL") :
           (std::getenv("EDITOR") ? std::getenv("EDITOR") : "vi");
  
  // Set default values
  indexer = IndexerType::kFts;
  encryption = EncryptionType::kNone;
  sync = SyncType::kNone;
  
  auto result = load(config_path);
  if (!result.has_value()) {
    // Config loading failed, continue with defaults
    // This allows the application to function even with missing/invalid config
  }
}

Result<void> Config::load(const std::filesystem::path& config_path) {
  config_path_ = config_path;
  
  if (!std::filesystem::exists(config_path)) {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                     "Config file not found: " + config_path.string()));
  }
  
  try {
    auto config_data = toml::parse_file(config_path.string());
    
    // Core paths
    if (auto value = config_data["root"].value<std::string>()) {
      root = *value;
    }
    if (auto value = config_data["notes_dir"].value<std::string>()) {
      notes_dir = *value;
    }
    if (auto value = config_data["attachments_dir"].value<std::string>()) {
      attachments_dir = *value;
    }
    if (auto value = config_data["trash_dir"].value<std::string>()) {
      trash_dir = *value;
    }
    if (auto value = config_data["index_file"].value<std::string>()) {
      index_file = *value;
    }
    
    // Editor
    if (auto value = config_data["editor"].value<std::string>()) {
      editor = *value;
    }
    
    // Indexer
    if (auto value = config_data["indexer"].value<std::string>()) {
      indexer = stringToIndexerType(*value);
    }
    
    // Encryption
    if (auto value = config_data["encryption"].value<std::string>()) {
      encryption = stringToEncryptionType(*value);
    }
    if (auto value = config_data["age_recipient"].value<std::string>()) {
      age_recipient = resolveEnvVar(*value);
    }
    
    // Sync
    if (auto value = config_data["sync"].value<std::string>()) {
      sync = stringToSyncType(*value);
    }
    if (auto value = config_data["git_remote"].value<std::string>()) {
      git_remote = *value;
    }
    
    // Defaults
    if (auto value = config_data["defaults"]["notebook"].value<std::string>()) {
      default_notebook = *value;
    }
    if (auto tags_array = config_data["defaults"]["tags"].as_array()) {
      default_tags.clear();
      for (const auto& tag : *tags_array) {
        if (auto tag_str = tag.value<std::string>()) {
          default_tags.push_back(*tag_str);
        }
      }
    }
    
    // AI configuration
    if (auto ai_table = config_data["ai"].as_table()) {
      AiConfig ai_config;
      
      if (auto value = (*ai_table)["provider"].value<std::string>()) {
        ai_config.provider = *value;
      }
      if (auto value = (*ai_table)["model"].value<std::string>()) {
        ai_config.model = *value;
      }
      if (auto value = (*ai_table)["api_key"].value<std::string>()) {
        ai_config.api_key = resolveEnvVar(*value);
      }
      if (auto value = (*ai_table)["max_tokens"].value<int>()) {
        ai_config.max_tokens = *value;
      }
      if (auto value = (*ai_table)["temperature"].value<double>()) {
        ai_config.temperature = *value;
      }
      if (auto value = (*ai_table)["rate_limit_qpm"].value<int>()) {
        ai_config.rate_limit_qpm = *value;
      }
      if (auto value = (*ai_table)["daily_usd_budget"].value<double>()) {
        ai_config.daily_usd_budget = *value;
      }
      if (auto value = (*ai_table)["enable_embeddings"].value<bool>()) {
        ai_config.enable_embeddings = *value;
      }
      if (auto value = (*ai_table)["embedding_model"].value<std::string>()) {
        ai_config.embedding_model = *value;
      }
      if (auto value = (*ai_table)["top_k"].value<int>()) {
        ai_config.top_k = *value;
      }
      
      // Redaction settings
      if (auto redaction_table = (*ai_table)["redaction"].as_table()) {
        if (auto value = (*redaction_table)["strip_emails"].value<bool>()) {
          ai_config.strip_emails = *value;
        }
        if (auto value = (*redaction_table)["strip_urls"].value<bool>()) {
          ai_config.strip_urls = *value;
        }
        if (auto value = (*redaction_table)["mask_numbers"].value<bool>()) {
          ai_config.mask_numbers = *value;
        }
      }
      
      ai = ai_config;
    }
    
    // Performance configuration
    if (auto perf_table = config_data["performance"].as_table()) {
      if (auto value = (*perf_table)["cache_size_mb"].value<int>()) {
        performance.cache_size_mb = static_cast<size_t>(*value);
      }
      if (auto value = (*perf_table)["max_file_size_mb"].value<int>()) {
        performance.max_file_size_mb = static_cast<size_t>(*value);
      }
      if (auto value = (*perf_table)["sqlite_cache_size"].value<int>()) {
        performance.sqlite_cache_size = *value;
      }
      if (auto value = (*perf_table)["sqlite_journal_mode"].value<std::string>()) {
        performance.sqlite_journal_mode = *value;
      }
      if (auto value = (*perf_table)["sqlite_synchronous"].value<std::string>()) {
        performance.sqlite_synchronous = *value;
      }
      if (auto value = (*perf_table)["sqlite_temp_store"].value<std::string>()) {
        performance.sqlite_temp_store = *value;
      }
    }
    
    return {};
    
  } catch (const toml::parse_error& e) {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                     "TOML parse error: " + std::string(e.what())));
  }
}

Result<void> Config::save(const std::filesystem::path& config_path) const {
  std::filesystem::path save_path = config_path.empty() ? config_path_ : config_path;
  
  if (save_path.empty()) {
    save_path = defaultConfigPath();
  }
  
  try {
    toml::table config_data;
    
    // Core paths
    if (!root.empty()) config_data.insert_or_assign("root", root.string());
    if (!notes_dir.empty()) config_data.insert_or_assign("notes_dir", notes_dir.string());
    if (!attachments_dir.empty()) config_data.insert_or_assign("attachments_dir", attachments_dir.string());
    if (!trash_dir.empty()) config_data.insert_or_assign("trash_dir", trash_dir.string());
    if (!index_file.empty()) config_data.insert_or_assign("index_file", index_file.string());
    
    // Editor
    if (!editor.empty()) config_data.insert_or_assign("editor", editor);
    
    // Indexer
    config_data.insert_or_assign("indexer", indexerTypeToString(indexer));
    
    // Encryption
    config_data.insert_or_assign("encryption", encryptionTypeToString(encryption));
    if (!age_recipient.empty()) config_data.insert_or_assign("age_recipient", age_recipient);
    
    // Sync
    config_data.insert_or_assign("sync", syncTypeToString(sync));
    if (!git_remote.empty()) config_data.insert_or_assign("git_remote", git_remote);
    
    // Defaults
    auto defaults_table = toml::table{};
    if (!default_notebook.empty()) defaults_table.insert_or_assign("notebook", default_notebook);
    if (!default_tags.empty()) {
      auto tags_array = toml::array{};
      for (const auto& tag : default_tags) {
        tags_array.push_back(tag);
      }
      defaults_table.insert_or_assign("tags", tags_array);
    }
    config_data.insert_or_assign("defaults", defaults_table);
    
    // AI configuration
    if (ai.has_value()) {
      auto ai_table = toml::table{};
      ai_table.insert_or_assign("provider", ai->provider);
      ai_table.insert_or_assign("model", ai->model);
      ai_table.insert_or_assign("api_key", ai->api_key);
      ai_table.insert_or_assign("max_tokens", ai->max_tokens);
      ai_table.insert_or_assign("temperature", ai->temperature);
      ai_table.insert_or_assign("rate_limit_qpm", ai->rate_limit_qpm);
      ai_table.insert_or_assign("daily_usd_budget", ai->daily_usd_budget);
      ai_table.insert_or_assign("enable_embeddings", ai->enable_embeddings);
      ai_table.insert_or_assign("embedding_model", ai->embedding_model);
      ai_table.insert_or_assign("top_k", ai->top_k);
      
      auto redaction_table = toml::table{};
      redaction_table.insert_or_assign("strip_emails", ai->strip_emails);
      redaction_table.insert_or_assign("strip_urls", ai->strip_urls);
      redaction_table.insert_or_assign("mask_numbers", ai->mask_numbers);
      ai_table.insert_or_assign("redaction", redaction_table);
      
      config_data.insert_or_assign("ai", ai_table);
    }
    
    // Performance configuration
    auto perf_table = toml::table{};
    perf_table.insert_or_assign("cache_size_mb", static_cast<int>(performance.cache_size_mb));
    perf_table.insert_or_assign("max_file_size_mb", static_cast<int>(performance.max_file_size_mb));
    perf_table.insert_or_assign("sqlite_cache_size", performance.sqlite_cache_size);
    perf_table.insert_or_assign("sqlite_journal_mode", performance.sqlite_journal_mode);
    perf_table.insert_or_assign("sqlite_synchronous", performance.sqlite_synchronous);
    perf_table.insert_or_assign("sqlite_temp_store", performance.sqlite_temp_store);
    config_data.insert_or_assign("performance", perf_table);
    
    // Ensure parent directory exists
    auto parent = save_path.parent_path();
    if (!parent.empty() && !std::filesystem::exists(parent)) {
      std::filesystem::create_directories(parent);
    }
    
    // Write to file
    std::ofstream file(save_path);
    if (!file) {
      return std::unexpected(makeError(ErrorCode::kConfigError, 
                                       "Cannot write config file: " + save_path.string()));
    }
    
    file << config_data;
    
    return {};
    
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                     "Config save error: " + std::string(e.what())));
  }
}

Result<std::string> Config::get(const std::string& key) const {
  auto path = splitPath(key);
  return getValueByPath(path);
}

Result<void> Config::set(const std::string& key, const std::string& value) {
  auto path = splitPath(key);
  return setValueByPath(path, value);
}

Result<void> Config::validate() const {
  // Validate paths exist if specified
  if (!notes_dir.empty() && !std::filesystem::exists(notes_dir)) {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                     "Notes directory does not exist: " + notes_dir.string()));
  }
  
  // Validate AI configuration
  if (ai.has_value()) {
    if (ai->provider != "openai" && ai->provider != "anthropic") {
      return std::unexpected(makeError(ErrorCode::kConfigError, 
                                       "Invalid AI provider: " + ai->provider));
    }
    
    if (ai->api_key.empty()) {
      return std::unexpected(makeError(ErrorCode::kConfigError, 
                                       "AI API key not configured"));
    }
    
    if (ai->max_tokens <= 0 || ai->max_tokens > 32000) {
      return std::unexpected(makeError(ErrorCode::kConfigError, 
                                       "Invalid max_tokens value"));
    }
  }
  
  return {};
}

std::filesystem::path Config::defaultConfigPath() {
  return nx::util::Xdg::configFile();
}

Config Config::createDefault() {
  Config config{};  // Create with default initialization
  
  // Set default paths
  config.root = nx::util::Xdg::dataHome();
  config.notes_dir = nx::util::Xdg::notesDir();
  config.attachments_dir = nx::util::Xdg::attachmentsDir();
  config.trash_dir = nx::util::Xdg::trashDir();
  config.index_file = nx::util::Xdg::indexFile();
  
  // Set default editor
  config.editor = std::getenv("VISUAL") ? std::getenv("VISUAL") :
                  (std::getenv("EDITOR") ? std::getenv("EDITOR") : "vi");
  
  // Set default values
  config.indexer = IndexerType::kFts;
  config.encryption = EncryptionType::kNone;
  config.sync = SyncType::kNone;
  
  return config;
}

std::string Config::resolveEnvVar(const std::string& value) const {
  if (value.substr(0, 4) == "env:") {
    std::string var_name = value.substr(4);
    const char* env_value = std::getenv(var_name.c_str());
    return env_value ? std::string(env_value) : "";
  }
  return value;
}

// Enum conversion methods
std::string Config::indexerTypeToString(IndexerType type) {
  switch (type) {
    case IndexerType::kFts: return "fts";
    case IndexerType::kRipgrep: return "ripgrep";
  }
  return "fts";
}

Config::IndexerType Config::stringToIndexerType(const std::string& str) {
  if (str == "ripgrep") return IndexerType::kRipgrep;
  return IndexerType::kFts;
}

std::string Config::encryptionTypeToString(EncryptionType type) {
  switch (type) {
    case EncryptionType::kNone: return "none";
    case EncryptionType::kAge: return "age";
  }
  return "none";
}

Config::EncryptionType Config::stringToEncryptionType(const std::string& str) {
  if (str == "age") return EncryptionType::kAge;
  return EncryptionType::kNone;
}

std::string Config::syncTypeToString(SyncType type) {
  switch (type) {
    case SyncType::kNone: return "none";
    case SyncType::kGit: return "git";
  }
  return "none";
}

Config::SyncType Config::stringToSyncType(const std::string& str) {
  if (str == "git") return SyncType::kGit;
  return SyncType::kNone;
}

Result<std::string> Config::getValueByPath(const std::vector<std::string>& path) const {
  if (path.empty()) {
    return std::unexpected(makeError(ErrorCode::kConfigError, "Empty config path"));
  }
  
  // Handle specific paths
  if (path.size() == 1) {
    const std::string& key = path[0];
    
    if (key == "root") return root.string();
    if (key == "notes_dir") return notes_dir.string();
    if (key == "attachments_dir") return attachments_dir.string();
    if (key == "trash_dir") return trash_dir.string();
    if (key == "index_file") return index_file.string();
    if (key == "editor") return editor;
    if (key == "indexer") return indexerTypeToString(indexer);
    if (key == "encryption") return encryptionTypeToString(encryption);
    if (key == "age_recipient") return age_recipient;
    if (key == "sync") return syncTypeToString(sync);
    if (key == "git_remote") return git_remote;
  } else if (path.size() == 2) {
    if (path[0] == "defaults") {
      if (path[1] == "notebook") return default_notebook;
    }
  }
  
  return std::unexpected(makeError(ErrorCode::kConfigError, 
                                   "Unknown config key: " + path[0]));
}

Result<void> Config::setValueByPath(const std::vector<std::string>& path, const std::string& value) {
  if (path.empty()) {
    return std::unexpected(makeError(ErrorCode::kConfigError, "Empty config path"));
  }
  
  // Handle specific paths
  if (path.size() == 1) {
    const std::string& key = path[0];
    
    if (key == "root") { root = value; return {}; }
    if (key == "notes_dir") { notes_dir = value; return {}; }
    if (key == "attachments_dir") { attachments_dir = value; return {}; }
    if (key == "trash_dir") { trash_dir = value; return {}; }
    if (key == "index_file") { index_file = value; return {}; }
    if (key == "editor") { editor = value; return {}; }
    if (key == "indexer") { indexer = stringToIndexerType(value); return {}; }
    if (key == "encryption") { encryption = stringToEncryptionType(value); return {}; }
    if (key == "age_recipient") { age_recipient = value; return {}; }
    if (key == "sync") { sync = stringToSyncType(value); return {}; }
    if (key == "git_remote") { git_remote = value; return {}; }
  } else if (path.size() == 2) {
    if (path[0] == "defaults") {
      if (path[1] == "notebook") { default_notebook = value; return {}; }
    }
  }
  
  return std::unexpected(makeError(ErrorCode::kConfigError, 
                                   "Unknown config key: " + path[0]));
}

std::vector<std::string> Config::splitPath(const std::string& path) const {
  std::vector<std::string> parts;
  std::istringstream stream(path);
  std::string part;
  
  while (std::getline(stream, part, '.')) {
    if (!part.empty()) {
      parts.push_back(part);
    }
  }
  
  return parts;
}

}  // namespace nx::config