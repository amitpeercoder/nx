#pragma once

#include <string>
#include <chrono>
#include <CLI/CLI.hpp>
#include "nx/cli/application.hpp"

namespace nx::cli {

class ListCommand : public Command {
public:
  explicit ListCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "ls"; }
  std::string description() const override { return "List notes"; }
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  std::string tag_;
  std::string notebook_;
  std::string since_;
  std::string before_;
  bool long_format_ = false;
  
  std::chrono::system_clock::time_point parseISODate(const std::string& date_str);
};

} // namespace nx::cli