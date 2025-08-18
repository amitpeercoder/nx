#pragma once

#include "nx/cli/application.hpp"
#include "nx/common.hpp"

namespace nx::cli {

/**
 * @brief Command for encrypting/decrypting notes
 */
class EncryptCommand {
public:
  explicit EncryptCommand(Application& app);
  
  void setupCommand(CLI::App* cmd);
  Result<int> execute(const GlobalOptions& options);

private:
  Application& app_;
  std::string note_id_;
  bool decrypt_ = false;
  bool all_notes_ = false;
  std::string key_file_;
  bool generate_key_ = false;
  std::string output_key_file_;
};

} // namespace nx::cli