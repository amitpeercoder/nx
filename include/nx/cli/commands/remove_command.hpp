#pragma once

#include <string>
#include <CLI/CLI.hpp>
#include "nx/cli/application.hpp"

namespace nx::cli {

class RemoveCommand : public Command {
public:
  explicit RemoveCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "rm"; }
  std::string description() const override { return "Remove a note"; }
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  std::string note_id_;
  bool permanent_ = false;
};

} // namespace nx::cli