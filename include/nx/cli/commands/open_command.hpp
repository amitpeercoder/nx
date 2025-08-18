#pragma once

#include <string>
#include <CLI/CLI.hpp>
#include "nx/cli/application.hpp"

namespace nx::cli {

class OpenCommand : public Command {
public:
  explicit OpenCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "open"; }
  std::string description() const override { return "Fuzzy find and open a note"; }
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  std::string partial_id_;
};

} // namespace nx::cli