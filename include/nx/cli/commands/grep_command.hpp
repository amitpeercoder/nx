#pragma once

#include <string>
#include <CLI/CLI.hpp>
#include "nx/cli/application.hpp"

namespace nx::cli {

class GrepCommand : public Command {
public:
  explicit GrepCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "grep"; }
  std::string description() const override { 
    return "Search notes with full-text search\n\n"
           "SEARCH MODES:\n"
           "  Simple:   nx grep \"project meeting\"      # Phrase search\n"
           "  Regex:    nx grep \"todo.*urgent\" --regex # Regular expressions\n"
           "  Case:     nx grep \"API\" --case-sensitive  # Exact case match\n\n"
           "EXAMPLES:\n"
           "  nx grep \"machine learning\"               # Find phrase in any note\n"
           "  nx grep \"TODO|FIXME\" --regex             # Find todos or fixmes\n"
           "  nx grep \"^# \" --regex                    # Find all headers\n"
           "  nx grep error --ignore-case               # Case-insensitive search\n\n"
           "SEARCH TIPS:\n"
           "  Boolean:     Use regex for AND/OR: \"(term1|term2)\"\n"
           "  Fuzzy:       Use partial words: \"machin learn\"\n"
           "  Structure:   Search headers: \"^## .*project\"";
  }
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  std::string query_;
  bool use_regex_ = false;
  bool ignore_case_ = false;
};

} // namespace nx::cli