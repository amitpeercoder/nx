#include "nx/cli/commands/move_command.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

namespace nx::cli {

MoveCommand::MoveCommand(Application& app) : app_(app) {
}

Result<int> MoveCommand::execute(const GlobalOptions& options) {
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
    std::string old_notebook = note.metadata().notebook().value_or("");
    
    // Update the note's notebook
    if (destination_.empty()) {
      note.setNotebook("");  // Set to empty notebook
    } else {
      note.setNotebook(destination_);
    }

    // Save the updated note
    auto store_result = app_.noteStore().store(note);
    if (!store_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": ")" << store_result.error().message() << R"(", "note_id": ")" << resolved_id->toString() << R"("})" << std::endl;
      } else {
        std::cout << "Error: " << store_result.error().message() << std::endl;
      }
      return 1;
    }

    // Update search index
    auto index_result = app_.searchIndex().updateNote(note);
    if (!index_result.has_value() && !options.quiet) {
      std::cerr << "Warning: Failed to update search index: " << index_result.error().message() << std::endl;
    }

    // Output result
    if (options.json) {
      nlohmann::json result;
      result["success"] = true;
      result["note_id"] = resolved_id->toString();
      result["old_notebook"] = old_notebook;
      if (destination_.empty()) {
        result["new_notebook"] = nullptr;
      } else {
        result["new_notebook"] = destination_;
      }
      std::cout << result.dump() << std::endl;
    } else if (!options.quiet) {
      std::string from = old_notebook.empty() ? "(no notebook)" : old_notebook;
      std::string to = destination_.empty() ? "(no notebook)" : destination_;
      std::cout << "Moved note " << resolved_id->toString() << " from '" << from << "' to '" << to << "'" << std::endl;
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

void MoveCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("note_id", note_id_, "Note ID to move (can be partial)")->required();
  cmd->add_option("destination", destination_, "Destination notebook (empty string for no notebook)")->required();
}

} // namespace nx::cli