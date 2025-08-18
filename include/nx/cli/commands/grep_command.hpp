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
  std::string description() const override { return "Search notes with grep"; }
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  std::string query_;
  bool use_regex_ = false;
  bool ignore_case_ = false;
};

} // namespace nx::cli