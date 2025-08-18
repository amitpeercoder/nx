#pragma once

#include <string>
#include <CLI/CLI.hpp>
#include "nx/cli/application.hpp"

namespace nx::cli {

class AttachCommand : public Command {
public:
  explicit AttachCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "attach"; }
  std::string description() const override { return "Attach file to note\n\nEXAMPLES:\n  nx attach abc123 document.pdf\n  nx attach abc123 image.jpg --description \"Project diagram\"\n  nx ls --tag project | head -1 | nx attach {} file.txt"; }
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  std::string note_id_;
  std::string file_path_;
  std::string description_;
};

} // namespace nx::cli