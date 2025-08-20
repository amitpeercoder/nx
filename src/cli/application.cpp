#include "nx/cli/application.hpp"

#include <iostream>
#include <filesystem>

#include "nx/store/filesystem_store.hpp"
#include "nx/store/filesystem_attachment_store.hpp"
#include "nx/index/index.hpp"
#include "nx/util/xdg.hpp"
#include "nx/di/service_configuration.hpp"

// Command includes
#include "nx/cli/commands/new_command.hpp"
#include "nx/cli/commands/edit_command.hpp"
#include "nx/cli/commands/view_command.hpp"
#include "nx/cli/commands/list_command.hpp"
#include "nx/cli/commands/remove_command.hpp"
#include "nx/cli/commands/move_command.hpp"
#include "nx/cli/commands/grep_command.hpp"
#include "nx/cli/commands/open_command.hpp"
#include "nx/cli/commands/backlinks_command.hpp"
#include "nx/cli/commands/tags_command.hpp"
#include "nx/cli/commands/export_command.hpp"

// AI Command includes
#include "nx/cli/commands/ask_command.hpp"
#include "nx/cli/commands/summarize_command.hpp"
#include "nx/cli/commands/tag_suggest_command.hpp"
#include "nx/cli/commands/rewrite_command.hpp"
#include "nx/cli/commands/tasks_command.hpp"
#include "nx/cli/commands/suggest_links_command.hpp"
#include "nx/cli/commands/outline_command.hpp"

// TUI Command includes
#include "nx/cli/commands/ui_command.hpp"

// Notebook management
#include "nx/cli/commands/notebook_command.hpp"

// Attachment management
#include "nx/cli/commands/attach_command.hpp"

// Import/Export management
#include "nx/cli/commands/import_command.hpp"

// Template management
#include "nx/cli/commands/tpl_command.hpp"

// Metadata management
#include "nx/cli/commands/meta_command.hpp"

// System maintenance commands
#include "nx/cli/commands/reindex_command.hpp"
#include "nx/cli/commands/backup_command.hpp"
#include "nx/cli/commands/gc_command.hpp"
#include "nx/cli/commands/doctor_command.hpp"

// Configuration management
#include "nx/cli/commands/config_command.hpp"

// Sync management
#include "nx/cli/commands/sync_command.hpp"

namespace nx::cli {

Application::Application() 
    : app_("nx", "High-performance CLI notes application")
    , services_initialized_(false) {
  
  // Set up the application
  app_.set_version_flag("--version", nx::getVersion().toString());
  app_.set_help_all_flag("--help-all", "Expand all help");
  app_.require_subcommand(1);
  
  setupGlobalOptions();
  setupCommands();
  setupHelp();
}

Application::Application(std::shared_ptr<nx::di::IServiceContainer> container)
    : app_("nx", "High-performance CLI notes application")
    , service_container_(std::move(container))
    , services_initialized_(true) {
  
  // Set up the application
  app_.set_version_flag("--version", nx::getVersion().toString());
  app_.set_help_all_flag("--help-all", "Expand all help");
  app_.require_subcommand(1);
  
  setupGlobalOptions();
  setupCommands();
  setupHelp();
}

int Application::run(int argc, char* argv[]) {
  try {
    app_.parse(argc, argv);
  } catch (const CLI::ParseError& e) {
    return app_.exit(e);
  } catch (const std::exception& e) {
    std::cerr << "Error: " << e.what() << std::endl;
    return 1;
  }
  
  // The command has already been executed by CLI11's callback system
  return 0;
}

Result<void> Application::initialize() {
  if (!services_initialized_) {
    return initializeServices();
  }
  return {};
}

void Application::setupGlobalOptions() {
  app_.add_flag("--json", global_options_.json, "Output in JSON format");
  app_.add_flag("-v,--verbose", global_options_.verbose, "Verbose output");
  app_.add_flag("-q,--quiet", global_options_.quiet, "Suppress normal output");
  app_.add_option("--config", global_options_.config_file, "Path to config file");
  app_.add_option("--notes-dir", global_options_.notes_dir, "Override notes directory");
  app_.add_flag("--no-color", global_options_.no_color, "Disable colored output");
}

void Application::setupCommands() {
  // Core CRUD commands
  registerCommand(std::make_unique<NewCommand>(*this));
  registerCommand(std::make_unique<EditCommand>(*this));
  registerCommand(std::make_unique<ViewCommand>(*this));
  registerCommand(std::make_unique<ListCommand>(*this));
  registerCommand(std::make_unique<RemoveCommand>(*this));
  registerCommand(std::make_unique<MoveCommand>(*this));
  
  // Search & Discovery commands
  registerCommand(std::make_unique<GrepCommand>(*this));
  registerCommand(std::make_unique<OpenCommand>(*this));
  registerCommand(std::make_unique<BacklinksCommand>(*this));
  registerCommand(std::make_unique<TagsCommand>(*this));
  
  // Import/Export commands
  registerCommand(std::make_unique<ExportCommand>(*this));
  
  // AI-powered commands
  registerCommand(std::make_unique<AskCommand>(*this));
  registerCommand(std::make_unique<SummarizeCommand>(*this));
  registerCommand(std::make_unique<TagSuggestCommand>(*this));
  registerCommand(std::make_unique<RewriteCommand>(*this));
  registerCommand(std::make_unique<TasksCommand>(*this));
  registerCommand(std::make_unique<SuggestLinksCommand>(*this));
  registerCommand(std::make_unique<OutlineCommand>(*this));
  
  // TUI command
  registerCommand(std::make_unique<UICommand>(*this));
  
  // Notebook management commands
  registerCommand(std::make_unique<NotebookCommand>(*this));
  
  // Attachment management commands
  registerCommand(std::make_unique<AttachCommand>(*this));
  
  // Import/Export commands
  registerCommand(std::make_unique<ImportCommand>(*this));
  
  // Template management commands
  registerCommand(std::make_unique<TplCommand>(*this));
  
  // Metadata management commands
  registerCommand(std::make_unique<MetaCommand>(*this));
  
  // System maintenance commands
  registerCommand(std::make_unique<ReindexCommand>(*this));
  registerCommand(std::make_unique<BackupCommand>(*this));
  registerCommand(std::make_unique<GcCommand>(*this));
  registerCommand(std::make_unique<DoctorCommand>(*this));
  
  // Configuration management commands
  registerCommand(std::make_unique<ConfigCommand>(*this));
  
  // Synchronization commands
  registerCommand(std::make_unique<SyncCommand>(*this));
}

void Application::setupHelp() {
  app_.get_formatter()->column_width(40);
  
  // Add footer with examples
  app_.footer(R"(Examples:
  nx new --tags work,important  # Title automatically derived from first line
  nx ls --tag work --since 2024-01-01
  nx grep "algorithm" --regex
  nx edit abc123
  nx view 01234567 --json
  
AI Commands:
  nx ask "What did I learn about machine learning?"
  nx summarize abc123 --style bullets --apply
  nx tag-suggest abc123 --apply
  nx rewrite abc123 --tone professional --apply
  nx tasks abc123 --priority high
  nx suggest-links abc123 --apply
  nx outline "Project Management" --create
  
For more information on a specific command, run:
  nx <command> --help)");
}

void Application::registerCommand(std::unique_ptr<Command> command) {
  auto* cmd_ptr = command.get();
  
  // Create CLI11 subcommand
  auto* sub = app_.add_subcommand(cmd_ptr->name(), cmd_ptr->description());
  
  // Let the command setup its specific options
  cmd_ptr->setupCommand(sub);
  
  // Set callback to execute the command
  sub->callback([this, cmd_ptr]() {
    // Initialize services before running command
    auto init_result = initializeServices();
    if (!init_result.has_value()) {
      if (global_options_.json) {
        std::cout << R"({"error": ")" << init_result.error().message() << R"(", "code": )" 
                  << static_cast<int>(init_result.error().code()) << "}\n";
      } else {
        std::cout << "Error: " << init_result.error().message() << "\n";
      }
      throw CLI::RuntimeError(1);
    }
    
    auto result = cmd_ptr->execute(global_options_);
    if (!result.has_value()) {
      if (global_options_.json) {
        std::cout << R"({"error": ")" << result.error().message() << R"(", "code": )" 
                  << static_cast<int>(result.error().code()) << "}\n";
      } else {
        std::cout << "Error: " << result.error().message() << "\n";
      }
      throw CLI::RuntimeError(1);
    }
    if (*result != 0) {
      throw CLI::RuntimeError(*result);
    }
  });
  
  // Store the command
  commands_.push_back(std::move(command));
}

Result<void> Application::initializeServices() {
  if (services_initialized_) {
    return {};
  }
  
  // Create service container if not provided
  if (!service_container_) {
    std::optional<std::filesystem::path> config_path;
    if (!global_options_.config_file.empty()) {
      config_path = global_options_.config_file;
    }
    
    auto container_result = nx::di::ServiceContainerFactory::createProductionContainer(config_path);
    if (!container_result.has_value()) {
      return std::unexpected(container_result.error());
    }
    service_container_ = *container_result;
  }
  
  // Override config with command line options if needed
  if (!global_options_.notes_dir.empty()) {
    auto config = service_container_->resolve<nx::config::Config>();
    config->notes_dir = global_options_.notes_dir;
  }
  
  services_initialized_ = true;
  return {};
}


// Getters for services (to be used by commands)
const GlobalOptions& Application::globalOptions() const {
  return global_options_;
}

nx::config::Config& Application::config() {
  if (!services_initialized_) {
    throw std::runtime_error("Services not initialized");
  }
  return *service_container_->resolve<nx::config::Config>();
}

nx::store::NoteStore& Application::noteStore() {
  if (!services_initialized_) {
    throw std::runtime_error("Services not initialized");
  }
  return *service_container_->resolve<nx::store::NoteStore>();
}

nx::store::NotebookManager& Application::notebookManager() {
  if (!services_initialized_) {
    throw std::runtime_error("Services not initialized");
  }
  return *service_container_->resolve<nx::store::NotebookManager>();
}

nx::store::AttachmentStore& Application::attachmentStore() {
  if (!services_initialized_) {
    throw std::runtime_error("Services not initialized");
  }
  return *service_container_->resolve<nx::store::AttachmentStore>();
}

nx::index::Index& Application::searchIndex() {
  if (!services_initialized_) {
    throw std::runtime_error("Services not initialized");
  }
  return *service_container_->resolve<nx::index::Index>();
}

nx::template_system::TemplateManager& Application::templateManager() {
  if (!services_initialized_) {
    throw std::runtime_error("Services not initialized");
  }
  return *service_container_->resolve<nx::template_system::TemplateManager>();
}

std::shared_ptr<nx::di::IServiceContainer> Application::serviceContainer() const {
  return service_container_;
}

} // namespace nx::cli