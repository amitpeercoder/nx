#pragma once

#include "nx/cli/application.hpp"
#include "nx/common.hpp"

namespace nx::cli {

/**
 * Command for managing configuration
 * 
 * Subcommands:
 * - get <key>: Get configuration value
 * - set <key> <value>: Set configuration value
 * - list: List all configuration
 * - path: Show configuration file path
 * - validate: Validate current configuration
 * - reset <key>: Reset key to default value
 */
class ConfigCommand : public Command {
public:
  explicit ConfigCommand(Application& app);
  
  void setupCommand(CLI::App* cmd) override;
  Result<int> execute(const GlobalOptions& options) override;
  
  std::string name() const override { return name_; }
  std::string description() const override { return description_; }

private:
  Application& app_;
  std::string name_ = "config";
  std::string description_ = "Manage configuration settings";
  
  // Subcommand flags
  bool get_mode_ = false;
  bool set_mode_ = false;
  bool list_mode_ = false;
  bool path_mode_ = false;
  bool validate_mode_ = false;
  bool reset_mode_ = false;
  
  // Command arguments
  std::string key_;
  std::string value_;
  
  // Options
  bool global_ = false;
  bool local_ = false;
  
  // Command execution methods
  Result<int> executeGet();
  Result<int> executeSet();
  Result<int> executeList();
  Result<int> executePath();
  Result<int> executeValidate();
  Result<int> executeReset();
  
  // Helper methods
  void printConfigValue(const std::string& key, const std::string& value, bool json_output);
  void printAllConfig(bool json_output);
  std::string getDefaultValue(const std::string& key);
  std::vector<std::string> getAllConfigKeys();
  bool isValidConfigKey(const std::string& key);
};

}  // namespace nx::cli