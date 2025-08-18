#include "nx/cli/commands/ui_command.hpp"
#include "nx/tui/tui_app.hpp"

namespace nx::cli {

UICommand::UICommand(Application& app) : app_(app) {}

Result<int> UICommand::execute(const GlobalOptions& options) {
  // Create and run TUI app
  nx::tui::TUIApp tui_app(app_.config(), app_.noteStore(), app_.notebookManager(), app_.searchIndex(), app_.templateManager());
  
  return tui_app.run();
}

std::string UICommand::name() const {
  return "ui";
}

std::string UICommand::description() const {
  return "Launch interactive TUI (Terminal User Interface)";
}

void UICommand::setupCommand(CLI::App* cmd) {
  // No additional options for the UI command
  (void)cmd;
}

} // namespace nx::cli