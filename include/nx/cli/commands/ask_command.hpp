#pragma once

#include <string>
#include <vector>
#include <optional>

#include <CLI/CLI.hpp>
#include "nx/cli/application.hpp"

namespace nx::cli {

/**
 * @brief RAG Q&A over notes
 * Usage: nx ask "query" [--tag tag] [--notebook name] [--since date] [--limit N]
 */
class AskCommand : public Command {
public:
  explicit AskCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "ask"; }
  std::string description() const override { return "Ask questions over your notes using AI"; }
  
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  
  // Command options
  std::string query_;
  std::vector<std::string> tags_;
  std::string notebook_;
  std::string since_;
  int limit_ = 6;  // Number of relevant notes to include as context
  
  // Helper methods
  Result<void> validateAiConfig();
  Result<std::vector<nx::core::Note>> findRelevantNotes();
  Result<std::string> buildContextFromNotes(const std::vector<nx::core::Note>& notes);
  Result<std::string> callAiApi(const std::string& context, const std::string& query);
  void outputResult(const std::string& answer, const std::vector<nx::core::Note>& sources, const GlobalOptions& options);
};

} // namespace nx::cli