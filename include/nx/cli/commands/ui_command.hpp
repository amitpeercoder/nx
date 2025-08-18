#pragma once

#include "nx/cli/application.hpp"

namespace nx::cli {

/**
 * @brief UI command that launches the TUI interface
 */
class UICommand : public Command {
public:
  explicit UICommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override;
  std::string description() const override;
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
};

} // namespace nx::cli