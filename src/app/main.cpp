#include <iostream>
#include <cstdlib>

#include "nx/cli/application.hpp"
#include "nx/cli/application_factory.hpp"
#include "nx/tui/tui_app.hpp"

int main(int argc, char* argv[]) {
  try {
    // Check if we should launch TUI instead of CLI
    if (nx::tui::TUIApp::shouldLaunchTUI(argc, argv)) {
      // Create CLI application using factory for proper DI setup
      auto cli_app_result = nx::cli::ApplicationFactory::createProductionApplication();
      if (!cli_app_result.has_value()) {
        std::cerr << "Failed to initialize: " << cli_app_result.error().message() << std::endl;
        return 1;
      }
      auto& cli_app = **cli_app_result;
      
      // Create TUI app with initialized services
      nx::tui::TUIApp tui_app(cli_app.config(), cli_app.noteStore(), cli_app.notebookManager(), cli_app.searchIndex(), cli_app.templateManager());
      
      return tui_app.run();
    }
    
    // Otherwise run normal CLI - create using factory for consistent DI setup
    auto app_result = nx::cli::ApplicationFactory::createProductionApplication();
    if (!app_result.has_value()) {
      std::cerr << "Failed to initialize: " << app_result.error().message() << std::endl;
      return 1;
    }
    
    return (*app_result)->run(argc, argv);
  } catch (const std::exception& e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "Fatal error: Unknown exception" << std::endl;
    return 1;
  }
}