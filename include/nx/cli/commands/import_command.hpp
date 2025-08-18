#pragma once

#include <string>
#include <vector>
#include <CLI/CLI.hpp>
#include "nx/cli/application.hpp"
#include "nx/import_export/import_manager.hpp"

namespace nx::cli {

class ImportCommand : public Command {
public:
  explicit ImportCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "import"; }
  std::string description() const override { 
    return "Import notes from files or directories\n\n"
           "SUPPORTED FORMATS:\n"
           "  dir       Generic directory import (Markdown, text files)\n"
           "  obsidian  Obsidian vault with wiki-links and attachments\n"
           "  notion    Notion export (HTML/Markdown)\n\n"
           "EXAMPLES:\n"
           "  nx import dir ~/Documents/notes --nb imported-docs\n"
           "  nx import obsidian ~/MyVault --recursive --preserve-structure\n"
           "  nx import notion ~/notion-export --nb notion-notes\n"
           "  nx import dir . --extensions md,txt --nb current-folder\n\n"
           "MIGRATION WORKFLOWS:\n"
           "  From Obsidian:  nx import obsidian /path/to/vault\n"
           "  From Notion:    nx import notion /path/to/export\n"
           "  From folders:   nx import dir /path/to/docs --recursive";
  }
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  
  // Subcommand selection
  std::string subcommand_;
  
  // Common options
  std::string target_notebook_ = "imported";
  bool recursive_ = true;
  std::vector<std::string> extensions_ = {"md", "txt", "markdown"};
  bool preserve_structure_ = true;
  bool overwrite_ = false;
  bool skip_hidden_ = true;
  
  // Subcommand-specific arguments
  std::string source_path_;
  
  // Subcommand implementations
  Result<int> executeDir();
  Result<int> executeFile();
  Result<int> executeObsidian();
  Result<int> executeNotion();
  
  // Helper methods
  void outputResult(const nx::import_export::ImportManager::ImportResult& result, 
                   const GlobalOptions& options);
};

} // namespace nx::cli