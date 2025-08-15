#include "nx/cli/commands/view_command.hpp"

#include <iostream>
#include <iomanip>
#include <nlohmann/json.hpp>

namespace nx::cli {

ViewCommand::ViewCommand(Application& app) : app_(app) {
}

Result<int> ViewCommand::execute(const GlobalOptions& options) {
  try {
    // Resolve the note ID
    auto resolved_id = app_.noteStore().resolveSingle(note_id_);
    if (!resolved_id.has_value()) {
      if (options.json) {
        std::cout << R"({"error": ")" << resolved_id.error().message() << R"(", "note_id": ")" << note_id_ << R"("})" << std::endl;
      } else {
        std::cout << "Error: " << resolved_id.error().message() << std::endl;
      }
      return 1;
    }

    // Load the note
    auto note_result = app_.noteStore().load(*resolved_id);
    if (!note_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": ")" << note_result.error().message() << R"(", "note_id": ")" << resolved_id->toString() << R"("})" << std::endl;
      } else {
        std::cout << "Error: " << note_result.error().message() << std::endl;
      }
      return 1;
    }

    auto note = *note_result;

    // Output in JSON format
    if (options.json) {
      nlohmann::json result;
      result["id"] = note.id().toString();
      result["title"] = note.title();
      result["content"] = note.content();
      result["created"] = std::chrono::duration_cast<std::chrono::milliseconds>(
          note.metadata().created().time_since_epoch()).count();
      result["modified"] = std::chrono::duration_cast<std::chrono::milliseconds>(
          note.metadata().updated().time_since_epoch()).count();
      result["tags"] = note.metadata().tags();
      if (note.notebook().has_value()) {
        result["notebook"] = *note.notebook();
      }
      
      std::cout << result.dump(2) << std::endl;
      return 0;
    }

    // Output in human-readable format
    if (!options.quiet) {
      // Header with metadata
      std::cout << "Note: " << resolved_id->toString() << std::endl;
      std::cout << "Title: " << note.title() << std::endl;
      
      // Format timestamps
      auto created_time = std::chrono::system_clock::to_time_t(note.metadata().created());
      auto modified_time = std::chrono::system_clock::to_time_t(note.metadata().updated());
      
      std::cout << "Created: " << std::put_time(std::localtime(&created_time), "%Y-%m-%d %H:%M:%S") << std::endl;
      std::cout << "Modified: " << std::put_time(std::localtime(&modified_time), "%Y-%m-%d %H:%M:%S") << std::endl;
      
      // Tags
      const auto& tags = note.metadata().tags();
      if (!tags.empty()) {
        std::cout << "Tags: ";
        for (size_t i = 0; i < tags.size(); ++i) {
          if (i > 0) std::cout << ", ";
          std::cout << tags[i];
        }
        std::cout << std::endl;
      }
      
      // Notebook
      if (note.notebook().has_value()) {
        std::cout << "Notebook: " << *note.notebook() << std::endl;
      }
      
      std::cout << std::string(50, '-') << std::endl;
    }
    
    // Content
    std::cout << note.content();
    if (!note.content().empty() && note.content().back() != '\n') {
      std::cout << std::endl;
    }

    return 0;

  } catch (const std::exception& e) {
    if (options.json) {
      std::cout << R"({"error": ")" << e.what() << R"(", "note_id": ")" << note_id_ << R"("})" << std::endl;
    } else {
      std::cout << "Error: " << e.what() << std::endl;
    }
    return 1;
  }
}

void ViewCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("note_id", note_id_, "Note ID to view (can be partial)")->required();
}

} // namespace nx::cli