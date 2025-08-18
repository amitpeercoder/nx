#pragma once

#include <string>
#include <vector>
#include <CLI/CLI.hpp>
#include "nx/cli/application.hpp"
#include "nx/core/note_id.hpp"

namespace nx::cli {

class TagsCommand : public Command {
public:
  explicit TagsCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "tags"; }
  std::string description() const override { return "Manage note tags"; }
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  
  // List subcommand options
  bool show_count_ = false;
  
  // Add subcommand options
  std::string note_id_str_;
  std::vector<std::string> tags_to_add_;
  
  // Remove subcommand options
  std::string remove_note_id_str_;
  std::vector<std::string> tags_to_remove_;
  
  // Set subcommand options
  std::string set_note_id_str_;
  std::vector<std::string> tags_to_set_;
  
  // Command execution methods
  Result<int> executeList(const GlobalOptions& options);
  Result<int> executeAdd(const GlobalOptions& options);
  Result<int> executeRemove(const GlobalOptions& options);
  Result<int> executeSet(const GlobalOptions& options);
};

} // namespace nx::cli