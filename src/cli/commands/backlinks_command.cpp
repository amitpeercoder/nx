#include "nx/cli/commands/backlinks_command.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

namespace nx::cli {

BacklinksCommand::BacklinksCommand(Application& app) : app_(app) {
}

Result<int> BacklinksCommand::execute(const GlobalOptions& options) {
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

    // Check if the note exists
    auto exists = app_.noteStore().exists(*resolved_id);
    if (!exists.has_value() || !*exists) {
      if (options.json) {
        std::cout << R"({"error": "Note not found", "note_id": ")" << resolved_id->toString() << R"("})" << std::endl;
      } else {
        std::cout << "Error: Note not found: " << resolved_id->toString() << std::endl;
      }
      return 1;
    }

    // Get backlinks
    auto backlinks_result = app_.noteStore().getBacklinks(*resolved_id);
    if (!backlinks_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": ")" << backlinks_result.error().message() << R"(", "note_id": ")" << resolved_id->toString() << R"("})" << std::endl;
      } else {
        std::cout << "Error: " << backlinks_result.error().message() << std::endl;
      }
      return 1;
    }

    const auto& backlinks = *backlinks_result;

    if (options.json) {
      nlohmann::json json_backlinks = nlohmann::json::array();
      
      // Load each backlinked note to get its title and other metadata
      for (const auto& backlink_id : backlinks) {
        auto note_result = app_.noteStore().load(backlink_id);
        if (note_result.has_value()) {
          const auto& note = *note_result;
          
          nlohmann::json json_backlink;
          json_backlink["id"] = backlink_id.toString();
          json_backlink["title"] = note.title();
          json_backlink["created"] = std::chrono::duration_cast<std::chrono::seconds>(
            note.metadata().created().time_since_epoch()).count();
          json_backlink["modified"] = std::chrono::duration_cast<std::chrono::seconds>(
            note.metadata().updated().time_since_epoch()).count();
          json_backlink["tags"] = note.metadata().tags();
          if (note.metadata().notebook().has_value()) {
            json_backlink["notebook"] = *note.metadata().notebook();
          } else {
            json_backlink["notebook"] = nullptr;
          }
          
          json_backlinks.push_back(json_backlink);
        } else {
          // If we can't load the note, just include the ID
          nlohmann::json json_backlink;
          json_backlink["id"] = backlink_id.toString();
          json_backlink["title"] = "(unable to load)";
          json_backlink["error"] = note_result.error().message();
          json_backlinks.push_back(json_backlink);
        }
      }
      
      nlohmann::json output;
      output["note_id"] = resolved_id->toString();
      output["total_backlinks"] = backlinks.size();
      output["backlinks"] = json_backlinks;
      
      std::cout << output.dump() << std::endl;
    } else {
      if (backlinks.empty()) {
        if (!options.quiet) {
          std::cout << "No backlinks found for note: " << resolved_id->toString() << std::endl;
        }
        return 0;
      }

      if (!options.quiet) {
        std::cout << "Found " << backlinks.size() << " backlink(s) to note: " << resolved_id->toString() << std::endl;
        std::cout << std::string(50, '-') << std::endl;
      }

      // Load and display each backlinked note
      for (const auto& backlink_id : backlinks) {
        auto note_result = app_.noteStore().load(backlink_id);
        if (note_result.has_value()) {
          const auto& note = *note_result;
          std::cout << backlink_id.toString() << " | " << note.title();
          
          if (note.metadata().notebook().has_value()) {
            std::cout << " [" << *note.metadata().notebook() << "]";
          }
          
          std::cout << std::endl;
          
          if (!note.metadata().tags().empty()) {
            std::cout << "  Tags: ";
            for (size_t i = 0; i < note.metadata().tags().size(); ++i) {
              if (i > 0) std::cout << ", ";
              std::cout << note.metadata().tags()[i];
            }
            std::cout << std::endl;
          }
          
          std::cout << std::endl;
        } else {
          std::cout << backlink_id.toString() << " | (unable to load: " << note_result.error().message() << ")" << std::endl;
        }
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

void BacklinksCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("note_id", note_id_, "Note ID to find backlinks for (can be partial)")->required();
}

} // namespace nx::cli