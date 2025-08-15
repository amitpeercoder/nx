#include "nx/cli/commands/config_command.hpp"

#include <iostream>
#include <iomanip>
#include <filesystem>

#include <nlohmann/json.hpp>

namespace nx::cli {

ConfigCommand::ConfigCommand(Application& app) : app_(app) {}

void ConfigCommand::setupCommand(CLI::App* cmd) {
  cmd->description("Manage configuration settings");
  
  // Add subcommands
  auto get_cmd = cmd->add_subcommand("get", "Get configuration value");
  get_cmd->add_option("key", key_, "Configuration key (dot notation)")->required();
  get_cmd->callback([this]() { get_mode_ = true; });
  
  auto set_cmd = cmd->add_subcommand("set", "Set configuration value");
  set_cmd->add_option("key", key_, "Configuration key (dot notation)")->required();
  set_cmd->add_option("value", value_, "Configuration value")->required();
  set_cmd->callback([this]() { set_mode_ = true; });
  
  auto list_cmd = cmd->add_subcommand("list", "List all configuration settings");
  list_cmd->callback([this]() { list_mode_ = true; });
  
  auto path_cmd = cmd->add_subcommand("path", "Show configuration file path");
  path_cmd->callback([this]() { path_mode_ = true; });
  
  auto validate_cmd = cmd->add_subcommand("validate", "Validate current configuration");
  validate_cmd->callback([this]() { validate_mode_ = true; });
  
  auto reset_cmd = cmd->add_subcommand("reset", "Reset configuration key to default value");
  reset_cmd->add_option("key", key_, "Configuration key to reset")->required();
  reset_cmd->callback([this]() { reset_mode_ = true; });
  
  // Global options for some subcommands
  get_cmd->add_flag("--global", global_, "Get from global config (default)");
  get_cmd->add_flag("--local", local_, "Get from local config");
  
  set_cmd->add_flag("--global", global_, "Set in global config (default)");
  set_cmd->add_flag("--local", local_, "Set in local config");
  
  // Require exactly one subcommand
  cmd->require_subcommand(1);
}

Result<int> ConfigCommand::execute(const GlobalOptions& options) {
  if (get_mode_) {
    return executeGet();
  } else if (set_mode_) {
    return executeSet();
  } else if (list_mode_) {
    return executeList();
  } else if (path_mode_) {
    return executePath();
  } else if (validate_mode_) {
    return executeValidate();
  } else if (reset_mode_) {
    return executeReset();
  }
  
  return std::unexpected(makeError(ErrorCode::kInvalidArgument, "No subcommand specified"));
}

Result<int> ConfigCommand::executeGet() {
  if (!isValidConfigKey(key_)) {
    return std::unexpected(makeError(ErrorCode::kInvalidArgument, "Invalid configuration key: " + key_));
  }
  
  auto& config = app_.config();
  auto result = config.get(key_);
  
  if (!result.has_value()) {
    if (app_.globalOptions().json) {
      nlohmann::json output;
      output["error"] = result.error().message();
      output["key"] = key_;
      std::cout << output.dump(2) << "\n";
    } else {
      std::cout << "Error: " << result.error().message() << "\n";
    }
    return 1;
  }
  
  printConfigValue(key_, *result, app_.globalOptions().json);
  return 0;
}

Result<int> ConfigCommand::executeSet() {
  if (!isValidConfigKey(key_)) {
    return std::unexpected(makeError(ErrorCode::kInvalidArgument, "Invalid configuration key: " + key_));
  }
  
  auto& config = app_.config();
  auto result = config.set(key_, value_);
  
  if (!result.has_value()) {
    if (app_.globalOptions().json) {
      nlohmann::json output;
      output["error"] = result.error().message();
      output["key"] = key_;
      output["value"] = value_;
      std::cout << output.dump(2) << "\n";
    } else {
      std::cout << "Error: " << result.error().message() << "\n";
    }
    return 1;
  }
  
  // Save configuration
  auto save_result = config.save();
  if (!save_result.has_value()) {
    if (app_.globalOptions().json) {
      nlohmann::json output;
      output["error"] = "Failed to save configuration: " + save_result.error().message();
      output["key"] = key_;
      output["value"] = value_;
      std::cout << output.dump(2) << "\n";
    } else {
      std::cout << "Error: Failed to save configuration: " << save_result.error().message() << "\n";
    }
    return 1;
  }
  
  if (app_.globalOptions().json) {
    nlohmann::json output;
    output["success"] = true;
    output["key"] = key_;
    output["value"] = value_;
    output["message"] = "Configuration updated successfully";
    std::cout << output.dump(2) << "\n";
  } else {
    std::cout << "✅ Configuration updated: " << key_ << " = " << value_ << "\n";
  }
  
  return 0;
}

Result<int> ConfigCommand::executeList() {
  printAllConfig(app_.globalOptions().json);
  return 0;
}

Result<int> ConfigCommand::executePath() {
  auto config_path = nx::config::Config::defaultConfigPath();
  
  if (app_.globalOptions().json) {
    nlohmann::json output;
    output["config_path"] = config_path.string();
    output["exists"] = std::filesystem::exists(config_path);
    std::cout << output.dump(2) << "\n";
  } else {
    std::cout << "Configuration file: " << config_path << "\n";
    if (std::filesystem::exists(config_path)) {
      std::cout << "Status: ✅ File exists\n";
    } else {
      std::cout << "Status: ⚠️  File not found (using defaults)\n";
    }
  }
  
  return 0;
}

Result<int> ConfigCommand::executeValidate() {
  auto& config = app_.config();
  auto result = config.validate();
  
  if (result.has_value()) {
    if (app_.globalOptions().json) {
      nlohmann::json output;
      output["valid"] = true;
      output["message"] = "Configuration is valid";
      std::cout << output.dump(2) << "\n";
    } else {
      std::cout << "✅ Configuration is valid\n";
    }
    return 0;
  } else {
    if (app_.globalOptions().json) {
      nlohmann::json output;
      output["valid"] = false;
      output["error"] = result.error().message();
      std::cout << output.dump(2) << "\n";
    } else {
      std::cout << "❌ Configuration validation failed: " << result.error().message() << "\n";
    }
    return 1;
  }
}

Result<int> ConfigCommand::executeReset() {
  if (!isValidConfigKey(key_)) {
    return std::unexpected(makeError(ErrorCode::kInvalidArgument, "Invalid configuration key: " + key_));
  }
  
  std::string default_value = getDefaultValue(key_);
  if (default_value.empty()) {
    if (app_.globalOptions().json) {
      nlohmann::json output;
      output["error"] = "Cannot determine default value for key: " + key_;
      output["key"] = key_;
      std::cout << output.dump(2) << "\n";
    } else {
      std::cout << "Error: Cannot determine default value for key: " << key_ << "\n";
    }
    return 1;
  }
  
  auto& config = app_.config();
  auto result = config.set(key_, default_value);
  
  if (!result.has_value()) {
    if (app_.globalOptions().json) {
      nlohmann::json output;
      output["error"] = result.error().message();
      output["key"] = key_;
      std::cout << output.dump(2) << "\n";
    } else {
      std::cout << "Error: " << result.error().message() << "\n";
    }
    return 1;
  }
  
  // Save configuration
  auto save_result = config.save();
  if (!save_result.has_value()) {
    if (app_.globalOptions().json) {
      nlohmann::json output;
      output["error"] = "Failed to save configuration: " + save_result.error().message();
      output["key"] = key_;
      std::cout << output.dump(2) << "\n";
    } else {
      std::cout << "Error: Failed to save configuration: " << save_result.error().message() << "\n";
    }
    return 1;
  }
  
  if (app_.globalOptions().json) {
    nlohmann::json output;
    output["success"] = true;
    output["key"] = key_;
    output["default_value"] = default_value;
    output["message"] = "Configuration key reset to default";
    std::cout << output.dump(2) << "\n";
  } else {
    std::cout << "✅ Reset " << key_ << " to default: " << default_value << "\n";
  }
  
  return 0;
}

void ConfigCommand::printConfigValue(const std::string& key, const std::string& value, bool json_output) {
  if (json_output) {
    nlohmann::json output;
    output["key"] = key;
    output["value"] = value;
    std::cout << output.dump(2) << "\n";
  } else {
    std::cout << key << " = " << value << "\n";
  }
}

void ConfigCommand::printAllConfig(bool json_output) {
  auto& config = app_.config();
  auto keys = getAllConfigKeys();
  
  if (json_output) {
    nlohmann::json output;
    for (const auto& key : keys) {
      auto result = config.get(key);
      if (result.has_value()) {
        output[key] = *result;
      }
    }
    std::cout << output.dump(2) << "\n";
  } else {
    std::cout << "Configuration settings:\n\n";
    
    // Group by categories
    std::cout << "📁 Paths:\n";
    for (const auto& key : {"root", "notes_dir", "attachments_dir", "trash_dir", "index_file"}) {
      auto result = config.get(key);
      if (result.has_value()) {
        std::cout << "  " << std::setw(16) << std::left << key << " = " << *result << "\n";
      }
    }
    
    std::cout << "\n📝 Editor:\n";
    for (const auto& key : {"editor"}) {
      auto result = config.get(key);
      if (result.has_value()) {
        std::cout << "  " << std::setw(16) << std::left << key << " = " << *result << "\n";
      }
    }
    
    std::cout << "\n🔍 Search:\n";
    for (const auto& key : {"indexer"}) {
      auto result = config.get(key);
      if (result.has_value()) {
        std::cout << "  " << std::setw(16) << std::left << key << " = " << *result << "\n";
      }
    }
    
    std::cout << "\n🔐 Security:\n";
    for (const auto& key : {"encryption", "age_recipient"}) {
      auto result = config.get(key);
      if (result.has_value()) {
        std::cout << "  " << std::setw(16) << std::left << key << " = " << *result << "\n";
      }
    }
    
    std::cout << "\n🔄 Sync:\n";
    for (const auto& key : {"sync", "git_remote", "git_user_name", "git_user_email"}) {
      auto result = config.get(key);
      if (result.has_value()) {
        std::cout << "  " << std::setw(16) << std::left << key << " = " << *result << "\n";
      }
    }
    
    std::cout << "\n📝 Defaults:\n";
    for (const auto& key : {"default_notebook"}) {
      auto result = config.get(key);
      if (result.has_value()) {
        std::cout << "  " << std::setw(16) << std::left << key << " = " << *result << "\n";
      }
    }
    
    std::cout << "\n🤖 AI (if configured):\n";
    for (const auto& key : {"ai.provider", "ai.model", "ai.max_tokens", "ai.temperature"}) {
      auto result = config.get(key);
      if (result.has_value()) {
        std::cout << "  " << std::setw(16) << std::left << key << " = " << *result << "\n";
      }
    }
  }
}

std::string ConfigCommand::getDefaultValue(const std::string& key) {
  // Create a default config and get the value
  auto default_config = nx::config::Config::createDefault();
  auto result = default_config.get(key);
  return result.has_value() ? *result : "";
}

std::vector<std::string> ConfigCommand::getAllConfigKeys() {
  return {
    // Core paths
    "root", "notes_dir", "attachments_dir", "trash_dir", "index_file",
    // Editor
    "editor",
    // Search
    "indexer",
    // Security
    "encryption", "age_recipient",
    // Sync
    "sync", "git_remote", "git_user_name", "git_user_email",
    // Defaults
    "default_notebook",
    // AI configuration
    "ai.provider", "ai.model", "ai.api_key", "ai.max_tokens", "ai.temperature",
    "ai.rate_limit_qpm", "ai.daily_usd_budget", "ai.enable_embeddings",
    "ai.embedding_model", "ai.top_k", "ai.strip_emails", "ai.strip_urls", "ai.mask_numbers",
    // Performance
    "performance.cache_size_mb", "performance.max_file_size_mb", "performance.sqlite_cache_size",
    "performance.sqlite_journal_mode", "performance.sqlite_synchronous", "performance.sqlite_temp_store"
  };
}

bool ConfigCommand::isValidConfigKey(const std::string& key) {
  auto valid_keys = getAllConfigKeys();
  return std::find(valid_keys.begin(), valid_keys.end(), key) != valid_keys.end();
}

} // namespace nx::cli