#include "nx/cli/commands/edit_command.hpp"

#include <iostream>
#include <cstdlib>
#include <vector>
#include <cstring>
#include <nlohmann/json.hpp>
#include "nx/store/filesystem_store.hpp"

namespace nx::cli {

EditCommand::EditCommand(Application& app) : app_(app) {
}

Result<int> EditCommand::execute(const GlobalOptions& options) {
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
    
    // Get editor from config or environment
    std::string editor = app_.config().editor;
    std::cerr << "DEBUG: Config editor: '" << editor << "'" << std::endl;
    
    if (editor.empty()) {
      const char* visual_env = std::getenv("VISUAL");
      const char* editor_env = std::getenv("EDITOR");
      
      std::cerr << "DEBUG: VISUAL env: '" << (visual_env ? visual_env : "null") << "'" << std::endl;
      std::cerr << "DEBUG: EDITOR env: '" << (editor_env ? editor_env : "null") << "'" << std::endl;
      
      if (visual_env && strlen(visual_env) > 0) {
        editor = visual_env;
        std::cerr << "DEBUG: Using VISUAL: " << editor << std::endl;
      } else if (editor_env && strlen(editor_env) > 0) {
        editor = editor_env;
        std::cerr << "DEBUG: Using EDITOR: " << editor << std::endl;
      } else {
        // Fallback editors in order of preference (nano first for better terminal compatibility)
        const std::vector<std::string> fallback_editors = {"nano", "micro", "nvim", "vim", "vi"};
        std::cerr << "DEBUG: Testing fallback editors..." << std::endl;
        for (const auto& candidate : fallback_editors) {
          std::cerr << "DEBUG: Testing " << candidate << std::endl;
          if (system(("which " + candidate + " > /dev/null 2>&1").c_str()) == 0) {
            editor = candidate;
            std::cerr << "DEBUG: Found and selected: " << candidate << std::endl;
            break;
          }
        }
        
        // Final fallback
        if (editor.empty()) {
          editor = "vi";
          std::cerr << "DEBUG: Using final fallback: vi" << std::endl;
        }
      }
    }
    
    std::cerr << "DEBUG: Final editor selection: '" << editor << "'" << std::endl;

    // Get the note file path
    auto filesystem_store = dynamic_cast<nx::store::FilesystemStore*>(&app_.noteStore());
    if (!filesystem_store) {
      if (options.json) {
        std::cout << R"({"error": "Edit command requires filesystem storage", "note_id": ")" << resolved_id->toString() << R"("})" << std::endl;
      } else {
        std::cout << "Error: Edit command requires filesystem storage" << std::endl;
      }
      return 1;
    }

    auto note_path = filesystem_store->getNotePath(*resolved_id);
    
    // Ensure terminal is in a good state before launching editor
    std::system("stty sane 2>/dev/null");
    
    // Launch editor
    std::string command = editor + " \"" + note_path.string() + "\"";
    int result = std::system(command.c_str());
    
    // Restore terminal state after editor exits
    std::system("stty sane 2>/dev/null");
    
    if (result != 0) {
      if (options.json) {
        std::cout << R"({"error": "Editor exited with non-zero status", "editor": ")" << editor << R"(", "exit_code": )" << result << "}" << std::endl;
      } else {
        std::cout << "Warning: Editor exited with status " << result << std::endl;
      }
    }

    // Reload and reindex the note after editing
    auto updated_note_result = app_.noteStore().load(*resolved_id);
    if (updated_note_result.has_value()) {
      auto index_result = app_.searchIndex().updateNote(*updated_note_result);
      if (!index_result.has_value() && !options.quiet) {
        std::cerr << "Warning: Failed to update search index: " << index_result.error().message() << std::endl;
      }
    }

    // Output result
    if (options.json) {
      std::cout << R"({"success": true, "note_id": ")" << resolved_id->toString() << R"(", "editor": ")" << editor << R"("})" << std::endl;
    } else if (!options.quiet) {
      std::cout << "Edited note: " << resolved_id->toString() << std::endl;
    }

    return result == 0 ? 0 : 1;

  } catch (const std::exception& e) {
    if (options.json) {
      std::cout << R"({"error": ")" << e.what() << R"(", "note_id": ")" << note_id_ << R"("})" << std::endl;
    } else {
      std::cout << "Error: " << e.what() << std::endl;
    }
    return 1;
  }
}

void EditCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("note_id", note_id_, "Note ID to edit (can be partial)")->required();
}

} // namespace nx::cli