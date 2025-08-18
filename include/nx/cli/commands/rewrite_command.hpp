#pragma once

#include <string>

#include <CLI/CLI.hpp>
#include "nx/cli/application.hpp"

namespace nx::cli {

/**
 * @brief Rewrite note content with different tones
 * Usage: nx rewrite <note_id> [--tone crisp|neutral|professional|casual] [--apply] [--dry-run]
 */
class RewriteCommand : public Command {
public:
  explicit RewriteCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "rewrite"; }
  std::string description() const override { return "Rewrite note content with different tone using AI"; }
  
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  
  // Command options
  std::string note_id_;
  std::string tone_ = "neutral";  // crisp, neutral, professional, casual
  bool apply_ = false;
  bool dry_run_ = false;
  
  // Helper methods
  Result<void> validateAiConfig();
  Result<nx::core::Note> loadNote();
  Result<std::string> rewriteContent(const nx::core::Note& note);
  Result<void> applyRewrite(const std::string& rewritten_content);
  void outputResult(const std::string& original_content, const std::string& rewritten_content, const GlobalOptions& options);
};

} // namespace nx::cli