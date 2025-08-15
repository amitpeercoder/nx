#include "nx/cli/commands/remove_command.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

namespace nx::cli {

RemoveCommand::RemoveCommand(Application& app) : app_(app) {
}

Result<int> RemoveCommand::execute(const GlobalOptions& options) {
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

    // Check if note exists
    auto exists = app_.noteStore().exists(*resolved_id);
    if (!exists.has_value() || !*exists) {
      if (options.json) {
        std::cout << R"({"error": "Note not found", "note_id": ")" << resolved_id->toString() << R"("})" << std::endl;
      } else {
        std::cout << "Error: Note not found: " << resolved_id->toString() << std::endl;
      }
      return 1;
    }

    // Remove the note (soft delete by default, permanent if --permanent flag is set)
    auto remove_result = app_.noteStore().remove(*resolved_id, !permanent_);
    if (!remove_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": ")" << remove_result.error().message() << R"(", "note_id": ")" << resolved_id->toString() << R"("})" << std::endl;
      } else {
        std::cout << "Error: " << remove_result.error().message() << std::endl;
      }
      return 1;
    }

    // Update search index
    auto index_result = app_.searchIndex().removeNote(*resolved_id);
    if (!index_result.has_value() && !options.quiet) {
      std::cerr << "Warning: Failed to update search index: " << index_result.error().message() << std::endl;
    }

    // Output result
    if (options.json) {
      std::cout << R"({"success": true, "note_id": ")" << resolved_id->toString() 
                << R"(", "permanent": )" << (permanent_ ? "true" : "false") << "}" << std::endl;
    } else if (!options.quiet) {
      if (permanent_) {
        std::cout << "Permanently deleted note: " << resolved_id->toString() << std::endl;
      } else {
        std::cout << "Moved note to trash: " << resolved_id->toString() << std::endl;
      }
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

void RemoveCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("note_id", note_id_, "Note ID to remove (can be partial)")->required();
  cmd->add_flag("--permanent", permanent_, "Permanently delete (skip trash)");
}

} // namespace nx::cli