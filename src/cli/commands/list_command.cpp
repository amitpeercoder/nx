#include "nx/cli/commands/list_command.hpp"

#include <iostream>
#include <iomanip>
#include <sstream>
#include <algorithm>
#include <nlohmann/json.hpp>
#include "nx/store/note_store.hpp"

namespace nx::cli {

ListCommand::ListCommand(Application& app) : app_(app) {
}

Result<int> ListCommand::execute(const GlobalOptions& options) {
  try {
    // Build query from command line options
    nx::store::NoteQuery query;
    
    if (!tag_.empty()) {
      query.tags = {tag_};
    }
    if (!notebook_.empty()) {
      query.notebook = notebook_;
    }
    if (!since_.empty()) {
      query.since = parseISODate(since_);
    }
    if (!before_.empty()) {
      query.until = parseISODate(before_);
    }
    
    // Get matching notes
    auto notes_result = app_.noteStore().search(query);
    if (!notes_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": ")" << notes_result.error().message() << R"("})" << std::endl;
      } else {
        std::cout << "Error: " << notes_result.error().message() << std::endl;
      }
      return 1;
    }
    
    auto notes = *notes_result;
    
    // Sort by modification date (newest first)
    std::sort(notes.begin(), notes.end(), [](const auto& a, const auto& b) {
      return a.metadata().updated() > b.metadata().updated();
    });

    // Output in JSON format
    if (options.json) {
      nlohmann::json result = nlohmann::json::array();
      for (const auto& note : notes) {
        nlohmann::json note_json;
        note_json["id"] = note.id().toString();
        note_json["title"] = note.title();
        note_json["created"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            note.metadata().created().time_since_epoch()).count();
        note_json["modified"] = std::chrono::duration_cast<std::chrono::milliseconds>(
            note.metadata().updated().time_since_epoch()).count();
        note_json["tags"] = note.metadata().tags();
        if (note.metadata().notebook().has_value()) {
          note_json["notebook"] = *note.metadata().notebook();
        }
        result.push_back(note_json);
      }
      std::cout << result.dump(2) << std::endl;
      return 0;
    }

    // Output in human-readable format
    if (notes.empty()) {
      if (!options.quiet) {
        std::cout << "No notes found matching criteria." << std::endl;
      }
      return 0;
    }

    if (long_format_) {
      // Long format: detailed information
      for (const auto& note : notes) {
        auto modified_time = std::chrono::system_clock::to_time_t(note.metadata().updated());
        
        std::cout << note.id().toString() << "  ";
        std::cout << std::put_time(std::localtime(&modified_time), "%Y-%m-%d %H:%M") << "  ";
        
        // Tags
        const auto& tags = note.metadata().tags();
        if (!tags.empty()) {
          std::cout << "[";
          for (size_t i = 0; i < tags.size(); ++i) {
            if (i > 0) std::cout << ",";
            std::cout << tags[i];
          }
          std::cout << "] ";
        }
        
        // Notebook
        if (note.metadata().notebook().has_value()) {
          std::cout << "(" << *note.metadata().notebook() << ") ";
        }
        
        std::cout << note.title() << std::endl;
      }
    } else {
      // Short format: ID and title only
      for (const auto& note : notes) {
        std::cout << note.id().toString() << "  " << note.title() << std::endl;
      }
    }

    if (options.verbose) {
      std::cout << "\nTotal: " << notes.size() << " notes" << std::endl;
    }

    return 0;

  } catch (const std::exception& e) {
    if (options.json) {
      std::cout << R"({"error": ")" << e.what() << R"("})" << std::endl;
    } else {
      std::cout << "Error: " << e.what() << std::endl;
    }
    return 1;
  }
}

void ListCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("--tag", tag_, "Filter by tag");
  cmd->add_option("--notebook", notebook_, "Filter by notebook");
  cmd->add_option("--since", since_, "Show notes created/modified since date (ISO-8601)");
  cmd->add_option("--before", before_, "Show notes created/modified before date (ISO-8601)");
  cmd->add_flag("-l,--long", long_format_, "Use long format output");
}

std::chrono::system_clock::time_point ListCommand::parseISODate(const std::string& date_str) {
  std::istringstream ss(date_str);
  std::tm tm = {};
  
  // Try to parse ISO format: YYYY-MM-DD or YYYY-MM-DDTHH:MM:SS
  if (date_str.find('T') != std::string::npos) {
    // Full ISO format with time
    ss >> std::get_time(&tm, "%Y-%m-%dT%H:%M:%S");
  } else {
    // Date only
    ss >> std::get_time(&tm, "%Y-%m-%d");
  }
  
  if (ss.fail()) {
    throw std::invalid_argument("Invalid date format: " + date_str + ". Expected ISO-8601 format (YYYY-MM-DD or YYYY-MM-DDTHH:MM:SS)");
  }
  
  return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

} // namespace nx::cli