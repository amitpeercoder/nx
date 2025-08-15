#include "nx/cli/commands/tags_command.hpp"

#include <iostream>
#include <unordered_map>
#include <algorithm>
#include <iomanip>
#include <nlohmann/json.hpp>
#include "nx/store/note_store.hpp"
#include "nx/core/note_id.hpp"

namespace nx::cli {

TagsCommand::TagsCommand(Application& app) : app_(app) {
}

Result<int> TagsCommand::execute(const GlobalOptions& options) {
  // Determine which subcommand to execute based on which parameters are set
  if (!note_id_str_.empty() || !tags_to_add_.empty()) {
    return executeAdd(options);
  } else if (!remove_note_id_str_.empty() || !tags_to_remove_.empty()) {
    return executeRemove(options);
  } else if (!set_note_id_str_.empty() || !tags_to_set_.empty()) {
    return executeSet(options);
  } else {
    return executeList(options);
  }
}

Result<int> TagsCommand::executeList(const GlobalOptions& options) {
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

Result<int> TagsCommand::executeAdd(const GlobalOptions& options) {
  try {
    if (note_id_str_.empty() || tags_to_add_.empty()) {
      if (options.json) {
        std::cout << R"({"error": "Note ID and tags are required for add operation"})" << std::endl;
      } else {
        std::cout << "Error: Note ID and tags are required for add operation" << std::endl;
      }
      return 1;
    }

    auto note_id_result = nx::core::NoteId::fromString(note_id_str_);
    if (!note_id_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": "Invalid note ID: )" << note_id_str_ << R"("})" << std::endl;
      } else {
        std::cout << "Error: Invalid note ID: " << note_id_str_ << std::endl;
      }
      return 1;
    }

    auto note_result = app_.noteStore().load(*note_id_result);
    if (!note_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": "Note not found"})" << std::endl;
      } else {
        std::cout << "Error: Note not found" << std::endl;
      }
      return 1;
    }

    auto note = *note_result;
    auto metadata = note.metadata();
    
    // Add new tags
    int added_count = 0;
    for (const auto& tag : tags_to_add_) {
      if (!metadata.hasTag(tag)) {
        metadata.addTag(tag);
        added_count++;
      }
    }

    if (added_count > 0) {
      // Update note with new metadata
      nx::core::Note updated_note(std::move(metadata), note.content());
      auto store_result = app_.noteStore().store(updated_note);
      if (!store_result.has_value()) {
        if (options.json) {
          std::cout << R"({"error": "Failed to save note: )" << store_result.error().message() << R"("})" << std::endl;
        } else {
          std::cout << "Error: Failed to save note: " << store_result.error().message() << std::endl;
        }
        return 1;
      }

      // Update search index
      auto index_result = app_.searchIndex().updateNote(updated_note);
      if (!index_result.has_value() && !options.quiet) {
        std::cerr << "Warning: Failed to update search index: " << index_result.error().message() << std::endl;
      }
    }

    if (options.json) {
      nlohmann::json output;
      output["note_id"] = note_id_str_;
      output["added_tags"] = tags_to_add_;
      output["added_count"] = added_count;
      output["current_tags"] = metadata.tags();
      std::cout << output.dump() << std::endl;
    } else {
      if (added_count > 0) {
        std::cout << "Added " << added_count << " tag(s) to note " << note_id_str_ << std::endl;
      } else {
        std::cout << "No new tags added (all tags already present)" << std::endl;
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

Result<int> TagsCommand::executeRemove(const GlobalOptions& options) {
  try {
    if (remove_note_id_str_.empty() || tags_to_remove_.empty()) {
      if (options.json) {
        std::cout << R"({"error": "Note ID and tags are required for remove operation"})" << std::endl;
      } else {
        std::cout << "Error: Note ID and tags are required for remove operation" << std::endl;
      }
      return 1;
    }

    auto note_id_result = nx::core::NoteId::fromString(remove_note_id_str_);
    if (!note_id_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": "Invalid note ID: )" << remove_note_id_str_ << R"("})" << std::endl;
      } else {
        std::cout << "Error: Invalid note ID: " << remove_note_id_str_ << std::endl;
      }
      return 1;
    }

    auto note_result = app_.noteStore().load(*note_id_result);
    if (!note_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": "Note not found"})" << std::endl;
      } else {
        std::cout << "Error: Note not found" << std::endl;
      }
      return 1;
    }

    auto note = *note_result;
    auto metadata = note.metadata();
    
    // Remove tags
    int removed_count = 0;
    for (const auto& tag : tags_to_remove_) {
      if (metadata.hasTag(tag)) {
        metadata.removeTag(tag);
        removed_count++;
      }
    }

    if (removed_count > 0) {
      // Update note with new metadata
      nx::core::Note updated_note(std::move(metadata), note.content());
      auto store_result = app_.noteStore().store(updated_note);
      if (!store_result.has_value()) {
        if (options.json) {
          std::cout << R"({"error": "Failed to save note: )" << store_result.error().message() << R"("})" << std::endl;
        } else {
          std::cout << "Error: Failed to save note: " << store_result.error().message() << std::endl;
        }
        return 1;
      }

      // Update search index
      auto index_result = app_.searchIndex().updateNote(updated_note);
      if (!index_result.has_value() && !options.quiet) {
        std::cerr << "Warning: Failed to update search index: " << index_result.error().message() << std::endl;
      }
    }

    if (options.json) {
      nlohmann::json output;
      output["note_id"] = remove_note_id_str_;
      output["removed_tags"] = tags_to_remove_;
      output["removed_count"] = removed_count;
      output["current_tags"] = metadata.tags();
      std::cout << output.dump() << std::endl;
    } else {
      if (removed_count > 0) {
        std::cout << "Removed " << removed_count << " tag(s) from note " << remove_note_id_str_ << std::endl;
      } else {
        std::cout << "No tags removed (tags not found on note)" << std::endl;
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

Result<int> TagsCommand::executeSet(const GlobalOptions& options) {
  try {
    if (set_note_id_str_.empty()) {
      if (options.json) {
        std::cout << R"({"error": "Note ID is required for set operation"})" << std::endl;
      } else {
        std::cout << "Error: Note ID is required for set operation" << std::endl;
      }
      return 1;
    }

    auto note_id_result = nx::core::NoteId::fromString(set_note_id_str_);
    if (!note_id_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": "Invalid note ID: )" << set_note_id_str_ << R"("})" << std::endl;
      } else {
        std::cout << "Error: Invalid note ID: " << set_note_id_str_ << std::endl;
      }
      return 1;
    }

    auto note_result = app_.noteStore().load(*note_id_result);
    if (!note_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": "Note not found"})" << std::endl;
      } else {
        std::cout << "Error: Note not found" << std::endl;
      }
      return 1;
    }

    auto note = *note_result;
    auto metadata = note.metadata();
    
    // Set tags (replace all existing tags)
    metadata.setTags(tags_to_set_);

    // Update note with new metadata
    nx::core::Note updated_note(std::move(metadata), note.content());
    auto store_result = app_.noteStore().store(updated_note);
    if (!store_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": "Failed to save note: )" << store_result.error().message() << R"("})" << std::endl;
      } else {
        std::cout << "Error: Failed to save note: " << store_result.error().message() << std::endl;
      }
      return 1;
    }

    // Update search index
    auto index_result = app_.searchIndex().updateNote(updated_note);
    if (!index_result.has_value() && !options.quiet) {
      std::cerr << "Warning: Failed to update search index: " << index_result.error().message() << std::endl;
    }

    if (options.json) {
      nlohmann::json output;
      output["note_id"] = set_note_id_str_;
      output["tags"] = updated_note.metadata().tags();
      std::cout << output.dump() << std::endl;
    } else {
      std::cout << "Set tags for note " << set_note_id_str_ << std::endl;
      if (!options.quiet) {
        const auto& current_tags = updated_note.metadata().tags();
        std::cout << "Current tags: ";
        if (current_tags.empty()) {
          std::cout << "none";
        } else {
          for (size_t i = 0; i < current_tags.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << current_tags[i];
          }
        }
        std::cout << std::endl;
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
  // Default behavior is list
  cmd->add_flag("--count,-c", show_count_, "Show note count for each tag");
  
  // Add subcommand - nx tags add <note_id> <tag1> <tag2> ...
  auto add_cmd = cmd->add_subcommand("add", "Add tags to a note");
  add_cmd->add_option("note_id", note_id_str_, "Note ID")->required();
  add_cmd->add_option("tags", tags_to_add_, "Tags to add")->required();
  
  // Remove subcommand - nx tags remove <note_id> <tag1> <tag2> ...
  auto remove_cmd = cmd->add_subcommand("remove", "Remove tags from a note");
  remove_cmd->add_option("note_id", remove_note_id_str_, "Note ID")->required();
  remove_cmd->add_option("tags", tags_to_remove_, "Tags to remove")->required();
  
  // Set subcommand - nx tags set <note_id> <tag1> <tag2> ...
  auto set_cmd = cmd->add_subcommand("set", "Set tags for a note (replaces all existing tags)");
  set_cmd->add_option("note_id", set_note_id_str_, "Note ID")->required();
  set_cmd->add_option("tags", tags_to_set_, "Tags to set");
}

} // namespace nx::cli