#pragma once

#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include "nx/cli/application.hpp"

namespace nx::cli {

/**
 * @brief Suggest links to other notes
 * Usage: nx suggest-links <note_id> [--apply] [--dry-run] [--limit N] [--min-score 0.7]
 */
class SuggestLinksCommand : public Command {
public:
  explicit SuggestLinksCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "suggest-links"; }
  std::string description() const override { return "Suggest links to related notes using AI"; }
  
  void setupCommand(CLI::App* cmd) override;

private:
  // Link suggestion structure
  struct LinkSuggestion {
    std::string target_note_id;
    std::string target_title;
    std::string reason;          // Why this link is suggested
    double relevance_score;      // 0.0 to 1.0
    std::string suggested_text;  // Text to use for the wikilink
    int insertion_point;         // Suggested line number to insert
  };
  
  Application& app_;
  
  // Command options
  std::string note_id_;
  bool apply_ = false;
  bool dry_run_ = false;
  int limit_ = 5;              // Maximum number of links to suggest
  double min_score_ = 0.7;     // Minimum relevance score
  
  // Helper methods
  Result<void> validateAiConfig();
  Result<nx::core::Note> loadNote();
  Result<std::vector<nx::core::Note>> getAllOtherNotes();
  Result<std::vector<LinkSuggestion>> suggestLinks(const nx::core::Note& note, const std::vector<nx::core::Note>& other_notes);
  Result<void> applyLinks(const std::vector<LinkSuggestion>& suggestions);
  void outputResult(const std::vector<LinkSuggestion>& suggestions, const GlobalOptions& options);
};

} // namespace nx::cli