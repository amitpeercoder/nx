#pragma once

#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include "nx/cli/application.hpp"

namespace nx::cli {

/**
 * @brief Create a new note
 * Usage: nx new [title] [--tags tag1,tag2] [--notebook name] [--from template]
 */
class NewCommand : public Command {
public:
  explicit NewCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "new"; }
  std::string description() const override { return "Create a new note"; }
  
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  
  // Command options
  std::string title_;
  std::vector<std::string> tags_;
  std::string notebook_;
  std::string template_name_;
  bool edit_after_create_ = false;
  
  // Helper methods
  bool hasStdinInput() const;
  std::string readStdinContent() const;
  
  // AI helper methods
  Result<std::string> generateTitleFromContent(const std::string& content);
  Result<std::vector<std::string>> generateTagsFromContent(const std::string& content);
};

} // namespace nx::cli