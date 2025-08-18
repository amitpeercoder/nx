#pragma once

#include <string>
#include <CLI/CLI.hpp>
#include "nx/cli/application.hpp"

namespace nx::cli {

class BacklinksCommand : public Command {
public:
  explicit BacklinksCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "backlinks"; }
  std::string description() const override { return "Show backlinks to a note"; }
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  std::string note_id_;
};

} // namespace nx::cli