#pragma once

#include <string>
#include <vector>
#include <map>
#include <CLI/CLI.hpp>
#include "nx/cli/application.hpp"

namespace nx::cli {

class TplCommand : public Command {
public:
  explicit TplCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "tpl"; }
  std::string description() const override { return "Template management"; }
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  
  // Subcommand selection
  std::string subcommand_;
  
  // Common options
  std::string template_name_;
  std::string template_content_;
  std::string template_file_;
  std::string description_;
  std::string category_;
  std::vector<std::string> variables_;
  bool force_ = false;
  bool install_builtins_ = false;
  
  // Subcommand implementations
  Result<int> executeList();
  Result<int> executeShow();
  Result<int> executeCreate();
  Result<int> executeEdit();
  Result<int> executeDelete();
  Result<int> executeUse();
  Result<int> executeSearch();
  Result<int> executeInstall();
  
  // Helper methods
  void outputTemplateList(const std::vector<nx::template_system::TemplateInfo>& templates, 
                         const GlobalOptions& options);
  void outputTemplateInfo(const nx::template_system::TemplateInfo& info, 
                         const GlobalOptions& options);
  std::map<std::string, std::string> parseVariables(const std::vector<std::string>& var_strings);
  std::string promptForTemplate(const std::vector<nx::template_system::TemplateInfo>& templates);
};

} // namespace nx::cli