/**
 * @file application_di_refactor.cpp
 * @brief Example demonstrating how to refactor the Application god object using DI
 * 
 * This example shows the transformation from tight coupling to dependency injection,
 * addressing the "ARCHITECTURAL CRITICAL" finding in review_req.md.
 */

#include <memory>
#include <iostream>

#include "nx/di/service_container.hpp"
#include "nx/di/service_configuration.hpp"
#include "nx/cli/application.hpp"

// BEFORE: God Object Anti-Pattern (Current State)
// ==============================================
// 
// class Application {
//   std::unique_ptr<nx::config::Config> config_;              // Direct ownership
//   std::unique_ptr<nx::store::NoteStore> note_store_;        // Direct ownership  
//   std::unique_ptr<nx::store::NotebookManager> notebook_manager_; // Direct ownership
//   std::unique_ptr<nx::store::AttachmentStore> attachment_store_; // Direct ownership
//   std::unique_ptr<nx::index::Index> search_index_;          // Direct ownership
//   std::unique_ptr<nx::template_system::TemplateManager> template_manager_; // Direct ownership
//   
//   Problems:
//   1. Violates Single Responsibility Principle (manages everything)
//   2. Impossible to unit test in isolation 
//   3. Tight coupling makes components non-reusable
//   4. Hard to mock dependencies for testing
//   5. Difficult to swap implementations
// };

// AFTER: Dependency Injection Pattern (Proposed Refactoring)
// =========================================================

namespace nx::cli {

/**
 * @brief Service responsible only for CLI command registration and execution
 * 
 * This focused class replaces the god object Application with a single responsibility:
 * managing CLI commands and their execution context.
 */
class CommandRunner {
public:
    explicit CommandRunner(std::shared_ptr<nx::di::IServiceContainer> container)
        : container_(container) {}
    
    /**
     * @brief Register all available CLI commands
     */
    void registerCommands() {
        // Commands now resolve their dependencies through DI
        registerCommand(std::make_unique<NewCommand>(container_));
        registerCommand(std::make_unique<EditCommand>(container_));
        registerCommand(std::make_unique<ListCommand>(container_));
        registerCommand(std::make_unique<SearchCommand>(container_));
        // ... other commands
    }
    
    /**
     * @brief Execute a command by name with arguments
     */
    Result<int> executeCommand(const std::string& command_name, 
                             const std::vector<std::string>& args,
                             const GlobalOptions& options) {
        auto it = commands_.find(command_name);
        if (it == commands_.end()) {
            return makeErrorResult<int>(ErrorCode::kInvalidArgument, 
                                      "Unknown command: " + command_name);
        }
        
        return it->second->execute(options);
    }

private:
    void registerCommand(std::unique_ptr<Command> command) {
        commands_[command->name()] = std::move(command);
    }
    
    std::shared_ptr<nx::di::IServiceContainer> container_;
    std::unordered_map<std::string, std::unique_ptr<Command>> commands_;
};

/**
 * @brief Service responsible for CLI argument parsing and option handling
 */
class ArgumentParser {
public:
    explicit ArgumentParser(std::shared_ptr<nx::di::IServiceContainer> container)
        : container_(container) {}
    
    /**
     * @brief Parse command line arguments and extract command/options
     */
    Result<ParsedArguments> parse(int argc, char* argv[]) {
        // Implementation would parse argc/argv using CLI11
        // and return structured command + options
        ParsedArguments result;
        // ... parsing logic
        return result;
    }

private:
    struct ParsedArguments {
        std::string command;
        std::vector<std::string> args;
        GlobalOptions options;
    };
    
    std::shared_ptr<nx::di::IServiceContainer> container_;
};

/**
 * @brief Lightweight application coordinator that orchestrates CLI execution
 * 
 * This replaces the monolithic Application class with a focused coordinator
 * that delegates responsibilities to specialized services.
 */
class DIApplication {
public:
    DIApplication() {
        // Create and configure DI container
        auto container_result = nx::di::ServiceContainerFactory::createProductionContainer();
        if (!container_result.has_value()) {
            throw std::runtime_error("Failed to initialize services: " + 
                                    container_result.error().message());
        }
        container_ = container_result.value();
        
        // Create focused service components
        command_runner_ = std::make_unique<CommandRunner>(container_);
        argument_parser_ = std::make_unique<ArgumentParser>(container_);
        
        // Register commands
        command_runner_->registerCommands();
    }
    
    /**
     * @brief Run the application - single responsibility
     */
    int run(int argc, char* argv[]) {
        try {
            // Parse arguments
            auto parse_result = argument_parser_->parse(argc, argv);
            if (!parse_result.has_value()) {
                std::cerr << "Error: " << parse_result.error().message() << std::endl;
                return 1;
            }
            
            auto& parsed = parse_result.value();
            
            // Execute command
            auto execute_result = command_runner_->executeCommand(
                parsed.command, parsed.args, parsed.options);
            
            if (!execute_result.has_value()) {
                std::cerr << "Error: " << execute_result.error().message() << std::endl;
                return 1;
            }
            
            return execute_result.value();
            
        } catch (const std::exception& e) {
            std::cerr << "Fatal error: " << e.what() << std::endl;
            return 1;
        }
    }

private:
    std::shared_ptr<nx::di::IServiceContainer> container_;
    std::unique_ptr<CommandRunner> command_runner_;
    std::unique_ptr<ArgumentParser> argument_parser_;
};

} // namespace nx::cli

// Example Command Implementation with DI
// ====================================

namespace nx::cli::commands {

/**
 * @brief Example of how commands would be refactored to use DI
 * 
 * BEFORE: Commands created services directly (tight coupling)
 * AFTER: Commands receive dependencies through constructor injection
 */
class NewCommand : public Command {
public:
    explicit NewCommand(std::shared_ptr<nx::di::IServiceContainer> container)
        : container_(container) {}
    
    Result<int> execute(const GlobalOptions& options) override {
        // Resolve dependencies from DI container - no tight coupling!
        auto note_store = container_->resolve<nx::store::NoteStore>();
        auto notebook_manager = container_->resolve<nx::store::NotebookManager>();
        auto config = container_->resolve<nx::config::Config>();
        
        // Business logic implementation...
        auto note_id = nx::core::NoteId::generate();
        nx::core::Metadata metadata(note_id, title_);
        nx::core::Note note(metadata, content_);
        
        auto store_result = note_store->store(note);
        if (!store_result.has_value()) {
            return std::unexpected(store_result.error());
        }
        
        std::cout << "Created note: " << note_id.toString() << std::endl;
        return 0;
    }
    
    std::string name() const override { return "new"; }
    std::string description() const override { return "Create a new note"; }

private:
    std::shared_ptr<nx::di::IServiceContainer> container_;
    std::string title_;
    std::string content_;
};

} // namespace nx::cli::commands

// Example Usage and Benefits
// ========================

/**
 * @brief Demonstrate the benefits of the DI refactoring
 */
void demonstrateBenefits() {
    std::cout << "Benefits of DI Refactoring:\n";
    std::cout << "==========================\n\n";
    
    std::cout << "1. TESTABILITY:\n";
    std::cout << "   - Commands can be unit tested with mock dependencies\n";
    std::cout << "   - No need to construct entire application for testing\n";
    std::cout << "   - Isolated testing of business logic\n\n";
    
    std::cout << "2. SEPARATION OF CONCERNS:\n";
    std::cout << "   - CommandRunner: only manages command registration/execution\n";
    std::cout << "   - ArgumentParser: only handles CLI argument parsing\n";
    std::cout << "   - DIApplication: only coordinates overall flow\n\n";
    
    std::cout << "3. FLEXIBILITY:\n";
    std::cout << "   - Easy to swap implementations (e.g., test vs production stores)\n";
    std::cout << "   - Configuration-driven service selection\n";
    std::cout << "   - Plugin architecture becomes possible\n\n";
    
    std::cout << "4. MAINTAINABILITY:\n";
    std::cout << "   - Clear dependencies make code easier to understand\n";
    std::cout << "   - Changes to one service don't affect others\n";
    std::cout << "   - Easier to add new features without breaking existing code\n\n";
}

/**
 * @brief Example of how to create a test-friendly version
 */
void demonstrateTestingBenefits() {
    std::cout << "Testing Example:\n";
    std::cout << "================\n\n";
    
    // Create test container with mocks
    auto test_container = nx::di::ServiceContainerFactory::createTestContainer();
    
    // Register mock implementations
    // test_container->registerInstance<nx::store::NoteStore>(mock_note_store);
    // test_container->registerInstance<nx::index::Index>(mock_search_index);
    
    // Test commands in isolation
    // auto command = NewCommand(test_container);
    // auto result = command.execute(test_options);
    // ASSERT_TRUE(result.has_value());
    
    std::cout << "Commands can now be tested in complete isolation!\n";
    std::cout << "No more god object dependencies blocking unit tests.\n\n";
}

// Main demonstration function
int main() {
    std::cout << "Application DI Refactoring Example\n";
    std::cout << "==================================\n\n";
    
    demonstrateBenefits();
    demonstrateTestingBenefits();
    
    std::cout << "This refactoring addresses the ARCHITECTURAL CRITICAL finding:\n";
    std::cout << "- Eliminates tight coupling throughout the codebase\n";
    std::cout << "- Provides dependency injection for better testability\n";
    std::cout << "- Breaks up the Application god object into focused services\n";
    std::cout << "- Creates foundation for proper unit testing\n\n";
    
    std::cout << "Next steps:\n";
    std::cout << "1. Incrementally refactor commands to use DI\n";
    std::cout << "2. Create mock implementations for testing\n";
    std::cout << "3. Add comprehensive unit tests with mocked dependencies\n";
    std::cout << "4. Consider extracting more specialized services\n";
    
    return 0;
}