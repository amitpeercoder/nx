#include "nx/cli/commands/tags_command.hpp"

#include <iostream>
#include <unordered_map>
#include <algorithm>
#include <iomanip>
#include <nlohmann/json.hpp>
#include "nx/store/note_store.hpp"

namespace nx::cli {

TagsCommand::TagsCommand(Application& app) : app_(app) {
}

Result<int> TagsCommand::execute(const GlobalOptions& options) {
  try {
    if (show_count_) {
      // When showing counts, we need to iterate through all notes
      // to count tag occurrences since getAllTags() only returns unique tags
      std::unordered_map<std::string, size_t> tag_counts;
      
      // Get all notes to count tag usage
      nx::store::NoteQuery query;
      auto notes_result = app_.noteStore().search(query);
      if (!notes_result.has_value()) {
        if (options.json) {
          std::cout << R"({"error": ")" << notes_result.error().message() << R"("})" << std::endl;
        } else {
          std::cout << "Error: " << notes_result.error().message() << std::endl;
        }
        return 1;
      }

      const auto& notes = *notes_result;
      
      // Count tag occurrences
      for (const auto& note : notes) {
        for (const auto& tag : note.metadata().tags()) {
          tag_counts[tag]++;
        }
      }

      // Convert to vector for sorting
      std::vector<std::pair<std::string, size_t>> sorted_tags(tag_counts.begin(), tag_counts.end());
      
      // Sort by tag name alphabetically
      std::sort(sorted_tags.begin(), sorted_tags.end(), 
                [](const auto& a, const auto& b) {
                  return a.first < b.first;
                });

      if (options.json) {
        nlohmann::json json_tags = nlohmann::json::array();
        
        for (const auto& [tag, count] : sorted_tags) {
          nlohmann::json json_tag;
          json_tag["name"] = tag;
          json_tag["count"] = count;
          json_tags.push_back(json_tag);
        }
        
        nlohmann::json output;
        output["total_tags"] = sorted_tags.size();
        output["show_count"] = true;
        output["tags"] = json_tags;
        
        std::cout << output.dump() << std::endl;
      } else {
        if (sorted_tags.empty()) {
          if (!options.quiet) {
            std::cout << "No tags found." << std::endl;
          }
          return 0;
        }

        if (!options.quiet) {
          std::cout << "Found " << sorted_tags.size() << " tag(s):" << std::endl;
          std::cout << std::string(40, '-') << std::endl;
        }

        for (const auto& [tag, count] : sorted_tags) {
          std::cout << std::setw(20) << std::left << tag << " (" << count << " note" << (count == 1 ? "" : "s") << ")" << std::endl;
        }
      }
    } else {
      // Simple tag listing without counts
      auto tags_result = app_.noteStore().getAllTags();
      if (!tags_result.has_value()) {
        if (options.json) {
          std::cout << R"({"error": ")" << tags_result.error().message() << R"("})" << std::endl;
        } else {
          std::cout << "Error: " << tags_result.error().message() << std::endl;
        }
        return 1;
      }

      const auto& tags = *tags_result;
      
      // Sort tags alphabetically
      auto sorted_tags = tags;
      std::sort(sorted_tags.begin(), sorted_tags.end());

      if (options.json) {
        nlohmann::json output;
        output["total_tags"] = sorted_tags.size();
        output["show_count"] = false;
        output["tags"] = sorted_tags;
        
        std::cout << output.dump() << std::endl;
      } else {
        if (sorted_tags.empty()) {
          if (!options.quiet) {
            std::cout << "No tags found." << std::endl;
          }
          return 0;
        }

        if (!options.quiet) {
          std::cout << "Found " << sorted_tags.size() << " tag(s):" << std::endl;
          std::cout << std::string(30, '-') << std::endl;
        }

        for (const auto& tag : sorted_tags) {
          std::cout << tag << std::endl;
        }
      }
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

void TagsCommand::setupCommand(CLI::App* cmd) {
  cmd->add_flag("--count,-c", show_count_, "Show note count for each tag");
}

} // namespace nx::cli