#pragma once

#include "nx/cli/application.hpp"
#include "nx/common.hpp"

namespace nx::cli {

/**
 * @brief Command for git synchronization operations
 */
class SyncCommand : public Command {
public:
  explicit SyncCommand(Application& app);
  
  void setupCommand(CLI::App* cmd) override;
  Result<int> execute(const GlobalOptions& options) override;
  
  std::string name() const override { return "sync"; }
  std::string description() const override { return "Git synchronization operations"; }

private:
  Application& app_;
  std::string operation_ = "status"; // status, init, clone, pull, push, sync, resolve
  std::string remote_url_;
  std::string branch_ = "main";
  std::string message_;
  bool force_ = false;
  bool auto_resolve_ = true;
  std::string strategy_ = "merge";
  std::string resolve_strategy_ = "manual"; // ours, theirs, manual
  std::vector<std::string> resolve_files_; // specific files to resolve
  std::string user_name_;
  std::string user_email_;
};

} // namespace nx::cli