#pragma once

#include <memory>
#include <string>
#include <vector>

#include <CLI/CLI.hpp>

#include "nx/common.hpp"
#include "nx/config/config.hpp"
#include "nx/store/note_store.hpp"
#include "nx/store/notebook_manager.hpp"
#include "nx/store/attachment_store.hpp"
#include "nx/index/index.hpp"
#include "nx/template/template_manager.hpp"
#include "nx/di/service_container.hpp"

namespace nx::cli {

/**
 * @brief Global CLI options that are available to all commands
 */
struct GlobalOptions {
  bool json = false;           // --json: Output in JSON format
  int verbose = 0;             // --verbose: Verbose output level (can be repeated: -v, -vv)
  bool quiet = false;          // --quiet: Suppress normal output
  std::string config_file;     // --config: Path to config file
  std::string notes_dir;       // --notes-dir: Override notes directory
  bool no_color = false;       // --no-color: Disable colored output
  bool force = false;          // --force: Force dangerous operations
};

/**
 * @brief Base class for all CLI commands
 */
class Command {
public:
  virtual ~Command() = default;
  
  /**
   * @brief Execute the command with the given arguments
   * @param options Global CLI options
   * @return Result with exit code (0 = success)
   */
  virtual Result<int> execute(const GlobalOptions& options) = 0;
  
  /**
   * @brief Get the command name
   */
  virtual std::string name() const = 0;
  
  /**
   * @brief Get the command description
   */
  virtual std::string description() const = 0;
  
  /**
   * @brief Setup command-specific CLI options (optional override)
   */
  virtual void setupCommand(CLI::App* cmd) { (void)cmd; }
};

/**
 * @brief Main CLI application
 */
class Application {
public:
  Application();
  explicit Application(std::shared_ptr<nx::di::IServiceContainer> container);
  ~Application() = default;

  /**
   * @brief Run the application with command line arguments
   * @param argc Argument count
   * @param argv Argument vector
   * @return Exit code (0 = success)
   */
  int run(int argc, char* argv[]);
  
  /**
   * @brief Initialize services without running CLI
   * @return Result indicating success or failure
   */
  Result<void> initialize();
  
  // Service accessors for commands
  const GlobalOptions& globalOptions() const;
  nx::config::Config& config();
  nx::store::NoteStore& noteStore();
  nx::store::NotebookManager& notebookManager();
  nx::store::AttachmentStore& attachmentStore();
  nx::index::Index& searchIndex();
  nx::template_system::TemplateManager& templateManager();
  
  // Service container access for advanced usage
  std::shared_ptr<nx::di::IServiceContainer> serviceContainer() const;

private:
  // Setup methods
  void setupGlobalOptions();
  void setupCommands();
  void setupHelp();
  
  // Command registration
  void registerCommand(std::unique_ptr<Command> command);
  
  // Initialization
  Result<void> initializeServices();
  
  // CLI framework
  CLI::App app_;
  GlobalOptions global_options_;
  
  // Dependency injection container
  std::shared_ptr<nx::di::IServiceContainer> service_container_;
  bool services_initialized_;
  
  // Registered commands
  std::vector<std::unique_ptr<Command>> commands_;
};

} // namespace nx::cli