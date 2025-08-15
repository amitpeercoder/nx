#include "nx/cli/commands/open_command.hpp"

#include <iostream>
#include <cstdlib>
#include <iomanip>
#include <nlohmann/json.hpp>
#include "nx/store/filesystem_store.hpp"

namespace nx::cli {

OpenCommand::OpenCommand(Application& app) : app_(app) {
}

Result<int> OpenCommand::execute(const GlobalOptions& options) {
  try {
    // Try fuzzy resolution
    auto fuzzy_matches = app_.noteStore().fuzzyResolve(partial_id_, 10);
    if (!fuzzy_matches.has_value()) {
      if (options.json) {
        std::cout << R"({"error": ")" << fuzzy_matches.error().message() << R"(", "partial_id": ")" << partial_id_ << R"("})" << std::endl;
      } else {
        std::cout << "Error: " << fuzzy_matches.error().message() << std::endl;
      }
      return 1;
    }

    const auto& matches = *fuzzy_matches;

    if (matches.empty()) {
      if (options.json) {
        std::cout << R"({"error": "No matching notes found", "partial_id": ")" << partial_id_ << R"("})" << std::endl;
      } else {
        std::cout << "No matching notes found for: " << partial_id_ << std::endl;
      }
      return 1;
    }

    // If exactly one match, open it directly
    if (matches.size() == 1) {
      const auto& match = matches[0];
      
      // Get editor from config or environment
      std::string editor = app_.config().editor;
      if (editor.empty()) {
        editor = std::getenv("VISUAL") ? std::getenv("VISUAL") :
                 (std::getenv("EDITOR") ? std::getenv("EDITOR") : "vi");
      }

      // Get the note file path
      auto filesystem_store = dynamic_cast<nx::store::FilesystemStore*>(&app_.noteStore());
      if (!filesystem_store) {
        if (options.json) {
          std::cout << R"({"error": "Open command requires filesystem storage", "note_id": ")" << match.id.toString() << R"("})" << std::endl;
        } else {
          std::cout << "Error: Open command requires filesystem storage" << std::endl;
        }
        return 1;
      }

      auto note_path = filesystem_store->getNotePath(match.id);
      
      // Ensure terminal is in a good state before launching editor
      std::system("stty sane 2>/dev/null");
      
      // Launch editor
      std::string command = editor + " \"" + note_path.string() + "\"";
      int result = std::system(command.c_str());
      
      // Restore terminal state after editor exits
      std::system("stty sane 2>/dev/null");
      
      if (result != 0) {
        if (options.json) {
          std::cout << R"({"error": "Editor exited with non-zero status", "editor": ")" << editor 
                    << R"(", "exit_code": )" << result << ", \"note_id\": \"" << match.id.toString() << "\"}" << std::endl;
        } else {
          std::cout << "Warning: Editor exited with status " << result << std::endl;
        }
      }

      // Reload and reindex the note after editing
      auto updated_note_result = app_.noteStore().load(match.id);
      if (updated_note_result.has_value()) {
        auto index_result = app_.searchIndex().updateNote(*updated_note_result);
        if (!index_result.has_value() && !options.quiet) {
          std::cerr << "Warning: Failed to update search index: " << index_result.error().message() << std::endl;
        }
      }

      // Output result
      if (options.json) {
        std::cout << R"({"success": true, "note_id": ")" << match.id.toString() 
                  << R"(", "title": ")" << match.display_text 
                  << R"(", "editor": ")" << editor << R"("})" << std::endl;
      } else if (!options.quiet) {
        std::cout << "Opened note: " << match.id.toString() << " - " << match.display_text << std::endl;
      }

      return result == 0 ? 0 : 1;
    }

    // Multiple matches - show list for user to choose
    if (options.json) {
      nlohmann::json json_matches = nlohmann::json::array();
      
      for (const auto& match : matches) {
        nlohmann::json json_match;
        json_match["id"] = match.id.toString();
        json_match["title"] = match.display_text;
        json_match["score"] = match.score;
        json_matches.push_back(json_match);
      }
      
      nlohmann::json output;
      output["partial_id"] = partial_id_;
      output["total_matches"] = matches.size();
      output["matches"] = json_matches;
      output["message"] = "Multiple matches found. Use a more specific ID or title.";
      
      std::cout << output.dump() << std::endl;
    } else {
      std::cout << "Multiple matches found for '" << partial_id_ << "':" << std::endl;
      std::cout << std::string(60, '-') << std::endl;
      
      for (size_t i = 0; i < matches.size(); ++i) {
        const auto& match = matches[i];
        std::cout << std::setw(2) << (i + 1) << ". " 
                  << match.id.toString() << " | " 
                  << match.display_text 
                  << " (score: " << std::fixed << std::setprecision(2) << match.score << ")" 
                  << std::endl;
      }
      
      std::cout << std::endl << "Use a more specific ID or title to open a single note." << std::endl;
    }

    return 1;  // Multiple matches is considered an error state for the open command

  } catch (const std::exception& e) {
    if (options.json) {
      std::cout << R"({"error": ")" << e.what() << R"(", "partial_id": ")" << partial_id_ << R"("})" << std::endl;
    } else {
      std::cout << "Error: " << e.what() << std::endl;
    }
    return 1;
  }
}

void OpenCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("partial_id", partial_id_, "Partial note ID or title to fuzzy match")->required();
}

} // namespace nx::cli