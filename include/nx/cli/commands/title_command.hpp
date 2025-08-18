#pragma once

#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include "nx/cli/application.hpp"

namespace nx::cli {

/**
 * @brief Auto-suggest titles for notes
 * Usage: nx title <note_id> [--apply] [--dry-run] [--count N]
 */
class TitleCommand : public Command {
public:
  explicit TitleCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "title"; }
  std::string description() const override { return "Suggest better titles for a note using AI"; }
  
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  
  // Command options
  std::string note_id_;
  bool apply_ = false;
  bool dry_run_ = false;
  int count_ = 3;  // Number of title suggestions to generate
  
  // Helper methods
  Result<void> validateAiConfig();
  Result<nx::core::Note> loadNote();
  Result<std::vector<std::string>> suggestTitles(const nx::core::Note& note);
  Result<void> applyTitle(const std::string& new_title);
  void outputResult(const std::vector<std::string>& suggested_titles, const std::string& current_title, const GlobalOptions& options);
};

} // namespace nx::cli