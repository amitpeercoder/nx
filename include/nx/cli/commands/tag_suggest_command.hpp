#pragma once

#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include "nx/cli/application.hpp"

namespace nx::cli {

/**
 * @brief Auto-suggest tags for notes
 * Usage: nx tag-suggest <note_id> [--apply] [--dry-run] [--limit N]
 */
class TagSuggestCommand : public Command {
public:
  explicit TagSuggestCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "tag-suggest"; }
  std::string description() const override { return "Suggest tags for a note using AI"; }
  
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  
  // Command options
  std::string note_id_;
  bool apply_ = false;
  bool dry_run_ = false;
  int limit_ = 5;  // Maximum number of tags to suggest
  
  // Helper methods
  Result<void> validateAiConfig();
  Result<nx::core::Note> loadNote();
  Result<std::vector<std::string>> suggestTags(const nx::core::Note& note);
  Result<std::vector<std::string>> getExistingTags();
  Result<void> applyTags(const std::vector<std::string>& suggested_tags);
  void outputResult(const std::vector<std::string>& suggested_tags, const std::vector<std::string>& existing_tags, const GlobalOptions& options);
};

} // namespace nx::cli