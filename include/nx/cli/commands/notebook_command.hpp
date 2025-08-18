#pragma once

#include "nx/cli/application.hpp"
#include "nx/common.hpp"
#include "nx/store/notebook_manager.hpp"

namespace nx::cli {

/**
 * @brief Notebook management command
 * 
 * Supports subcommands:
 * - list: List all notebooks
 * - create: Create a new notebook
 * - rename: Rename an existing notebook
 * - delete: Delete a notebook
 * - info: Show detailed information about a notebook
 */
class NotebookCommand : public Command {
public:
  explicit NotebookCommand(Application& app);

  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override;
  std::string description() const override;
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  
  // Subcommands
  enum class SubCommand {
    List,
    Create,
    Rename,
    Delete,
    Info
  };
  
  SubCommand sub_command_ = SubCommand::List;
  
  // Command-specific options
  std::string notebook_name_;
  std::string new_name_;          // For rename command
  bool force_ = false;           // For delete command
  bool with_stats_ = false;      // For list and info commands
  bool json_output_ = false;     // JSON format output
  
  // Subcommand implementations
  Result<int> executeList(const GlobalOptions& options);
  Result<int> executeCreate(const GlobalOptions& options);
  Result<int> executeRename(const GlobalOptions& options);
  Result<int> executeDelete(const GlobalOptions& options);
  Result<int> executeInfo(const GlobalOptions& options);
  
  // Helper methods
  void printNotebookInfo(const nx::store::NotebookInfo& info, bool json_format = false) const;
  void printNotebookList(const std::vector<nx::store::NotebookInfo>& notebooks, bool json_format = false) const;
  
  // Error handling
  void displayError(const Error& error, const std::string& operation, const GlobalOptions& options) const;
};

} // namespace nx::cli