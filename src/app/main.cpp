#include <iostream>
#include <cstdlib>

#include "nx/cli/application.hpp"
#include "nx/tui/tui_app.hpp"

int main(int argc, char* argv[]) {
  try {
    // Check if we should launch TUI instead of CLI
    if (nx::tui::TUIApp::shouldLaunchTUI(argc, argv)) {
      // Initialize CLI application to set up services
      nx::cli::Application cli_app;
      
      // Initialize services
      auto init_result = cli_app.initialize();
      if (!init_result.has_value()) {
        std::cerr << "Failed to initialize: " << init_result.error().message() << std::endl;
        return 1;
      }
      
      // Create TUI app with initialized services
      nx::tui::TUIApp tui_app(cli_app.config(), cli_app.noteStore(), cli_app.notebookManager(), cli_app.searchIndex());
      
      return tui_app.run();
    }
    
    // Otherwise run normal CLI
    nx::cli::Application app;
    return app.run(argc, argv);
  } catch (const std::exception& e) {
    std::cerr << "Fatal error: " << e.what() << std::endl;
    return 1;
  } catch (...) {
    std::cerr << "Fatal error: Unknown exception" << std::endl;
    return 1;
  }
}