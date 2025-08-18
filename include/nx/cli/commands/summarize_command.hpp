#pragma once

#include <string>

#include <CLI/CLI.hpp>
#include "nx/cli/application.hpp"

namespace nx::cli {

/**
 * @brief Auto-summarize notes
 * Usage: nx summarize <note_id> [--style bullets|exec] [--apply] [--dry-run]
 */
class SummarizeCommand : public Command {
public:
  explicit SummarizeCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "summarize"; }
  std::string description() const override { return "Generate AI summary of a note"; }
  
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  
  // Command options
  std::string note_id_;
  std::string style_ = "bullets";  // bullets, exec
  bool apply_ = false;
  bool dry_run_ = false;
  
  // Helper methods
  Result<void> validateAiConfig();
  Result<nx::core::Note> loadNote();
  Result<std::string> generateSummary(const nx::core::Note& note);
  Result<void> applySummary(const std::string& summary);
  void outputResult(const std::string& summary, const GlobalOptions& options);
};

} // namespace nx::cli