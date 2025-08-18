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
  std::string description() const override { return "Attach file to note"; }
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  std::string note_id_;
  std::string file_path_;
  std::string description_;
};

} // namespace nx::cli