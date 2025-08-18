#pragma once

#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include "nx/cli/application.hpp"

namespace nx::cli {

/**
 * @brief Extract action items from notes
 * Usage: nx tasks <note_id> [--priority high|medium|low] [--context]
 */
class TasksCommand : public Command {
public:
  explicit TasksCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "tasks"; }
  std::string description() const override { return "Extract action items and tasks from note content using AI"; }
  
  void setupCommand(CLI::App* cmd) override;

private:
  // Task structure for JSON output
  struct Task {
    std::string description;
    std::string priority;      // high, medium, low
    std::string context;       // surrounding context from note
    int line_number = 0;       // approximate line number in original content
  };
  
  Application& app_;
  
  // Command options
  std::string note_id_;
  std::string priority_filter_;  // Filter by priority
  bool include_context_ = false; // Include surrounding context
  
  // Helper methods
  Result<void> validateAiConfig();
  Result<nx::core::Note> loadNote();
  Result<std::vector<Task>> extractTasks(const nx::core::Note& note);
  void outputResult(const std::vector<Task>& tasks, const GlobalOptions& options);
};

} // namespace nx::cli