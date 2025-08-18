#pragma once

#include <string>
#include <CLI/CLI.hpp>
#include "nx/cli/application.hpp"

namespace nx::cli {

class MoveCommand : public Command {
public:
  explicit MoveCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "mv"; }
  std::string description() const override { return "Move a note between notebooks"; }
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  std::string note_id_;
  std::string destination_;
};

} // namespace nx::cli