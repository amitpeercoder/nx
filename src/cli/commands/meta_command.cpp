#include "nx/cli/commands/meta_command.hpp"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <iomanip>
#include <nlohmann/json.hpp>

namespace nx::cli {

MetaCommand::MetaCommand(Application& app) : app_(app) {
}

Result<int> MetaCommand::execute(const GlobalOptions& options) {
  try {
    if (subcommand_ == "get") {
      return executeGet();
    } else if (subcommand_ == "set") {
      return executeSet();
    } else if (subcommand_ == "list" || subcommand_ == "ls") {
      return executeList();
    } else if (subcommand_ == "remove" || subcommand_ == "rm") {
      return executeRemove();
    } else if (subcommand_ == "clear") {
      return executeClear();
    } else if (subcommand_ == "update") {
      return executeUpdate();
    } else {
      std::cout << "Error: Unknown meta subcommand: " << subcommand_ << std::endl;
      std::cout << "Available subcommands: get, set, list, remove, clear, update" << std::endl;
      return 1;
    }
    
  } catch (const std::exception& e) {
    if (options.json) {
      std::cout << R"({"error": ")" << e.what() << R"(", "subcommand": ")" << subcommand_ << R"("}))" << std::endl;
    } else {
      std::cout << "Error: " << e.what() << std::endl;
    }
    return 1;
  }
}

void MetaCommand::setupCommand(CLI::App* cmd) {
  // Get subcommand
  auto get_cmd = cmd->add_subcommand("get", "Get metadata value");
  get_cmd->add_option("note_id", note_id_, "Note ID (partial ID accepted)")->required();
  get_cmd->add_option("key", key_, "Metadata key");
  get_cmd->add_flag("--all", list_all_, "Get all metadata");
  get_cmd->callback([this]() { subcommand_ = "get"; });
  
  // Set subcommand
  auto set_cmd = cmd->add_subcommand("set", "Set metadata value");
  set_cmd->add_option("note_id", note_id_, "Note ID (partial ID accepted)")->required();
  set_cmd->add_option("key", key_, "Metadata key")->required();
  set_cmd->add_option("value", value_, "Metadata value")->required();
  set_cmd->callback([this]() { subcommand_ = "set"; });
  
  // List subcommand
  auto list_cmd = cmd->add_subcommand("list", "List all metadata for note");
  list_cmd->add_option("note_id", note_id_, "Note ID (partial ID accepted)")->required();
  list_cmd->callback([this]() { subcommand_ = "list"; });
  
  auto ls_cmd = cmd->add_subcommand("ls", "List all metadata for note (alias for list)");
  ls_cmd->add_option("note_id", note_id_, "Note ID (partial ID accepted)")->required();
  ls_cmd->callback([this]() { subcommand_ = "ls"; });
  
  // Remove subcommand
  auto remove_cmd = cmd->add_subcommand("remove", "Remove metadata key");
  remove_cmd->add_option("note_id", note_id_, "Note ID (partial ID accepted)")->required();
  remove_cmd->add_option("key", key_, "Metadata key to remove")->required();
  remove_cmd->callback([this]() { subcommand_ = "remove"; });
  
  auto rm_cmd = cmd->add_subcommand("rm", "Remove metadata key (alias for remove)");
  rm_cmd->add_option("note_id", note_id_, "Note ID (partial ID accepted)")->required();
  rm_cmd->add_option("key", key_, "Metadata key to remove")->required();
  rm_cmd->callback([this]() { subcommand_ = "rm"; });
  
  // Clear subcommand
  auto clear_cmd = cmd->add_subcommand("clear", "Clear all custom metadata");
  clear_cmd->add_option("note_id", note_id_, "Note ID (partial ID accepted)")->required();
  clear_cmd->callback([this]() { subcommand_ = "clear"; });
  
  // Update subcommand
  auto update_cmd = cmd->add_subcommand("update", "Update multiple metadata values");
  update_cmd->add_option("note_id", note_id_, "Note ID (partial ID accepted)")->required();
  update_cmd->add_option("--set", key_value_pairs_, "Set metadata (key=value)");
  update_cmd->callback([this]() { subcommand_ = "update"; });
  
  cmd->require_subcommand(1);
}

Result<int> MetaCommand::executeGet() {
  auto note_id_result = resolveNoteId(note_id_);
  if (!note_id_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << note_id_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error: " << note_id_result.error().message() << std::endl;
    }
    return 1;
  }

  auto note_result = app_.noteStore().load(note_id_result.value());
  if (!note_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << note_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error: " << note_result.error().message() << std::endl;
    }
    return 1;
  }

  const auto& metadata = note_result.value().metadata();

  if (list_all_ || key_.empty()) {
    // Show all metadata
    if (app_.globalOptions().json) {
      nlohmann::json output;
      output["note_id"] = metadata.id().toString();
      output["title"] = metadata.title();
      
      auto timestamp_to_string = [](const std::chrono::system_clock::time_point& tp) {
        auto time_t = std::chrono::system_clock::to_time_t(tp);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time_t), "%Y-%m-%dT%H:%M:%SZ");
        return ss.str();
      };
      
      output["created"] = timestamp_to_string(metadata.created());
      output["updated"] = timestamp_to_string(metadata.updated());
      
      if (!metadata.tags().empty()) {
        output["tags"] = metadata.tags();
      }
      
      if (metadata.notebook().has_value()) {
        output["notebook"] = metadata.notebook().value();
      }
      
      if (!metadata.customFields().empty()) {
        nlohmann::json custom = nlohmann::json::object();
        for (const auto& [k, v] : metadata.customFields()) {
          custom[k] = v;
        }
        output["custom"] = custom;
      }
      
      std::cout << output.dump(2) << std::endl;
    } else {
      outputMetadata(metadata, app_.globalOptions());
    }
  } else {
    // Show specific key
    auto value_result = getMetadataField(metadata, key_);
    if (!value_result.has_value()) {
      if (app_.globalOptions().json) {
        std::cout << R"({"error": ")" << value_result.error().message() << R"("}))" << std::endl;
      } else {
        std::cout << "Error: " << value_result.error().message() << std::endl;
      }
      return 1;
    }

    if (app_.globalOptions().json) {
      nlohmann::json output;
      output["note_id"] = metadata.id().toString();
      output["key"] = key_;
      output["value"] = value_result.value();
      std::cout << output.dump(2) << std::endl;
    } else {
      outputKeyValue(key_, value_result.value(), app_.globalOptions());
    }
  }

  return 0;
}

Result<int> MetaCommand::executeSet() {
  auto note_id_result = resolveNoteId(note_id_);
  if (!note_id_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << note_id_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error: " << note_id_result.error().message() << std::endl;
    }
    return 1;
  }

  auto note_result = app_.noteStore().load(note_id_result.value());
  if (!note_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << note_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error: " << note_result.error().message() << std::endl;
    }
    return 1;
  }

  auto note = note_result.value();
  auto& metadata = note.metadata();

  // Special handling for title - update content instead of metadata
  if (key_ == "title") {
    auto title_result = setNoteTitle(note, value_);
    if (!title_result.has_value()) {
      if (app_.globalOptions().json) {
        std::cout << R"({"error": ")" << title_result.error().message() << R"("}))" << std::endl;
      } else {
        std::cout << "Error: " << title_result.error().message() << std::endl;
      }
      return 1;
    }
  } else {
    auto set_result = setMetadataField(metadata, key_, value_);
    if (!set_result.has_value()) {
      if (app_.globalOptions().json) {
        std::cout << R"({"error": ")" << set_result.error().message() << R"("}))" << std::endl;
      } else {
        std::cout << "Error: " << set_result.error().message() << std::endl;
      }
      return 1;
    }
  }

  // Save the note
  auto store_result = app_.noteStore().store(note);
  if (!store_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << store_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error saving note: " << store_result.error().message() << std::endl;
    }
    return 1;
  }

  if (app_.globalOptions().json) {
    nlohmann::json output;
    output["success"] = true;
    output["note_id"] = note.id().toString();
    output["key"] = key_;
    output["value"] = value_;
    output["action"] = "set";
    std::cout << output.dump(2) << std::endl;
  } else {
    std::cout << "Set " << key_ << " = " << value_ << " for note " << note.id().toString() << std::endl;
  }

  return 0;
}

Result<int> MetaCommand::executeList() {
  return executeGet(); // Same as get --all
}

Result<int> MetaCommand::executeRemove() {
  auto note_id_result = resolveNoteId(note_id_);
  if (!note_id_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << note_id_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error: " << note_id_result.error().message() << std::endl;
    }
    return 1;
  }

  auto note_result = app_.noteStore().load(note_id_result.value());
  if (!note_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << note_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error: " << note_result.error().message() << std::endl;
    }
    return 1;
  }

  auto note = note_result.value();
  auto& metadata = note.metadata();

  // Check if the key exists first
  auto get_result = getMetadataField(metadata, key_);
  if (!get_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << get_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error: " << get_result.error().message() << std::endl;
    }
    return 1;
  }

  // Remove based on key type
  if (key_ == "tags") {
    metadata.setTags({});
  } else if (key_ == "notebook") {
    metadata.setNotebook(std::optional<std::string>{});
  } else {
    // Remove custom field
    metadata.removeCustomField(key_);
  }

  // Save the note
  auto store_result = app_.noteStore().store(note);
  if (!store_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << store_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error saving note: " << store_result.error().message() << std::endl;
    }
    return 1;
  }

  if (app_.globalOptions().json) {
    nlohmann::json output;
    output["success"] = true;
    output["note_id"] = note.id().toString();
    output["key"] = key_;
    output["action"] = "removed";
    std::cout << output.dump(2) << std::endl;
  } else {
    std::cout << "Removed " << key_ << " from note " << note.id().toString() << std::endl;
  }

  return 0;
}

Result<int> MetaCommand::executeClear() {
  auto note_id_result = resolveNoteId(note_id_);
  if (!note_id_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << note_id_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error: " << note_id_result.error().message() << std::endl;
    }
    return 1;
  }

  auto note_result = app_.noteStore().load(note_id_result.value());
  if (!note_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << note_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error: " << note_result.error().message() << std::endl;
    }
    return 1;
  }

  auto note = note_result.value();
  auto& metadata = note.metadata();

  // Clear all custom fields (preserve built-in metadata like title, dates, etc.)
  auto custom_fields = metadata.customFields();
  for (const auto& [key, value] : custom_fields) {
    metadata.removeCustomField(key);
  }

  // Save the note
  auto store_result = app_.noteStore().store(note);
  if (!store_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << store_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error saving note: " << store_result.error().message() << std::endl;
    }
    return 1;
  }

  if (app_.globalOptions().json) {
    nlohmann::json output;
    output["success"] = true;
    output["note_id"] = note.id().toString();
    output["cleared_fields"] = static_cast<int>(custom_fields.size());
    output["action"] = "cleared";
    std::cout << output.dump(2) << std::endl;
  } else {
    std::cout << "Cleared " << custom_fields.size() << " custom metadata fields from note " << note.id().toString() << std::endl;
  }

  return 0;
}

Result<int> MetaCommand::executeUpdate() {
  auto note_id_result = resolveNoteId(note_id_);
  if (!note_id_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << note_id_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error: " << note_id_result.error().message() << std::endl;
    }
    return 1;
  }

  auto note_result = app_.noteStore().load(note_id_result.value());
  if (!note_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << note_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error: " << note_result.error().message() << std::endl;
    }
    return 1;
  }

  auto note = note_result.value();
  auto& metadata = note.metadata();

  auto key_values = parseKeyValuePairs(key_value_pairs_);
  if (key_values.empty()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": "No key-value pairs specified. Use --set key=value"}))" << std::endl;
    } else {
      std::cout << "Error: No key-value pairs specified. Use --set key=value" << std::endl;
    }
    return 1;
  }

  std::vector<std::string> updated_keys;
  for (const auto& [key, value] : key_values) {
    // Special handling for title - update content instead of metadata
    if (key == "title") {
      auto title_result = setNoteTitle(note, value);
      if (!title_result.has_value()) {
        if (app_.globalOptions().json) {
          std::cout << R"({"error": "Error setting )" << key << R"(: )" << title_result.error().message() << R"("}))" << std::endl;
        } else {
          std::cout << "Error setting " << key << ": " << title_result.error().message() << std::endl;
        }
        return 1;
      }
    } else {
      auto set_result = setMetadataField(metadata, key, value);
      if (!set_result.has_value()) {
        if (app_.globalOptions().json) {
          std::cout << R"({"error": "Error setting )" << key << R"(: )" << set_result.error().message() << R"("}))" << std::endl;
        } else {
          std::cout << "Error setting " << key << ": " << set_result.error().message() << std::endl;
        }
        return 1;
      }
    }
    updated_keys.push_back(key);
  }

  // Save the note
  auto store_result = app_.noteStore().store(note);
  if (!store_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << store_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error saving note: " << store_result.error().message() << std::endl;
    }
    return 1;
  }

  if (app_.globalOptions().json) {
    nlohmann::json output;
    output["success"] = true;
    output["note_id"] = note.id().toString();
    output["updated_keys"] = updated_keys;
    output["action"] = "updated";
    std::cout << output.dump(2) << std::endl;
  } else {
    std::cout << "Updated " << updated_keys.size() << " metadata field(s) for note " << note.id().toString() << std::endl;
    for (const auto& key : updated_keys) {
      std::cout << "  " << key << " = " << key_values[key] << std::endl;
    }
  }

  return 0;
}

// Helper methods

void MetaCommand::outputMetadata(const nx::core::Metadata& metadata, const GlobalOptions& options) {
  if (!options.quiet) {
    std::cout << "Note: " << metadata.id().toString() << std::endl;
    std::cout << "Title: " << metadata.title() << std::endl;
    
    auto time_to_string = [](const std::chrono::system_clock::time_point& tp) {
      auto time_t = std::chrono::system_clock::to_time_t(tp);
      std::stringstream ss;
      ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
      return ss.str();
    };
    
    std::cout << "Created: " << time_to_string(metadata.created()) << std::endl;
    std::cout << "Updated: " << time_to_string(metadata.updated()) << std::endl;
    
    if (!metadata.tags().empty()) {
      std::cout << "Tags: ";
      for (size_t i = 0; i < metadata.tags().size(); ++i) {
        std::cout << metadata.tags()[i];
        if (i < metadata.tags().size() - 1) std::cout << ", ";
      }
      std::cout << std::endl;
    }
    
    if (metadata.notebook().has_value()) {
      std::cout << "Notebook: " << metadata.notebook().value() << std::endl;
    }
    
    const auto& custom_fields = metadata.customFields();
    if (!custom_fields.empty()) {
      std::cout << "Custom Fields:" << std::endl;
      for (const auto& [key, value] : custom_fields) {
        std::cout << "  " << key << ": " << value << std::endl;
      }
    }
  }
}

void MetaCommand::outputKeyValue(const std::string& key, const std::string& value, const GlobalOptions& options) {
  if (!options.quiet) {
    std::cout << key << ": " << value << std::endl;
  }
}

std::map<std::string, std::string> MetaCommand::parseKeyValuePairs(const std::vector<std::string>& pairs) {
  std::map<std::string, std::string> result;
  
  for (const auto& pair : pairs) {
    auto eq_pos = pair.find('=');
    if (eq_pos != std::string::npos) {
      std::string key = pair.substr(0, eq_pos);
      std::string value = pair.substr(eq_pos + 1);
      result[key] = value;
    }
  }
  
  return result;
}

Result<nx::core::NoteId> MetaCommand::resolveNoteId(const std::string& partial_id) {
  return app_.noteStore().resolveSingle(partial_id);
}

Result<std::string> MetaCommand::getMetadataField(const nx::core::Metadata& metadata, const std::string& key) {
  if (key == "id") {
    return metadata.id().toString();
  } else if (key == "title") {
    return metadata.title();
  } else if (key == "created") {
    auto time_t = std::chrono::system_clock::to_time_t(metadata.created());
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
  } else if (key == "updated") {
    auto time_t = std::chrono::system_clock::to_time_t(metadata.updated());
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
  } else if (key == "tags") {
    std::string result;
    const auto& tags = metadata.tags();
    for (size_t i = 0; i < tags.size(); ++i) {
      result += tags[i];
      if (i < tags.size() - 1) result += ",";
    }
    return result;
  } else if (key == "notebook") {
    if (metadata.notebook().has_value()) {
      return metadata.notebook().value();
    } else {
      return std::unexpected(makeError(ErrorCode::kNotFound, "Notebook not set"));
    }
  } else {
    // Check custom fields
    auto custom_value = metadata.getCustomField(key);
    if (custom_value.has_value()) {
      return custom_value.value();
    } else {
      return std::unexpected(makeError(ErrorCode::kNotFound, "Metadata key not found: " + key));
    }
  }
}

Result<void> MetaCommand::setNoteTitle(nx::core::Note& note, const std::string& new_title) {
  // Update content first line to change the title
  std::string updated_content = note.content();
  if (updated_content.starts_with("# ")) {
    // Replace the first line (title heading)
    size_t first_newline = updated_content.find('\n');
    if (first_newline != std::string::npos) {
      updated_content = "# " + new_title + updated_content.substr(first_newline);
    } else {
      updated_content = "# " + new_title;
    }
  } else {
    // Content doesn't start with heading, prepend one
    updated_content = "# " + new_title + "\n\n" + updated_content;
  }
  
  note.setContent(updated_content);
  return {};
}

Result<void> MetaCommand::setMetadataField(nx::core::Metadata& metadata, const std::string& key, const std::string& value) {
  if (key == "title") {
    return std::unexpected(makeError(ErrorCode::kValidationError, "Use setNoteTitle() to set title - it updates content instead of metadata"));
  } else if (key == "tags") {
    // Parse comma-separated tags
    std::vector<std::string> tags;
    std::stringstream ss(value);
    std::string tag;
    while (std::getline(ss, tag, ',')) {
      // Trim whitespace
      tag.erase(0, tag.find_first_not_of(" \t"));
      tag.erase(tag.find_last_not_of(" \t") + 1);
      if (!tag.empty()) {
        tags.push_back(tag);
      }
    }
    metadata.setTags(tags);
  } else if (key == "notebook") {
    metadata.setNotebook(value);
  } else if (key == "id" || key == "created" || key == "updated") {
    return std::unexpected(makeError(ErrorCode::kValidationError, "Cannot modify read-only field: " + key));
  } else {
    // Set custom field
    metadata.setCustomField(key, value);
  }
  
  return {};
}

std::vector<std::pair<std::string, std::string>> MetaCommand::getAllMetadataFields(const nx::core::Metadata& metadata) {
  std::vector<std::pair<std::string, std::string>> fields;
  
  fields.emplace_back("id", metadata.id().toString());
  fields.emplace_back("title", metadata.title());
  
  auto time_to_string = [](const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M:%S");
    return ss.str();
  };
  
  fields.emplace_back("created", time_to_string(metadata.created()));
  fields.emplace_back("updated", time_to_string(metadata.updated()));
  
  if (!metadata.tags().empty()) {
    std::string tags_str;
    const auto& tags = metadata.tags();
    for (size_t i = 0; i < tags.size(); ++i) {
      tags_str += tags[i];
      if (i < tags.size() - 1) tags_str += ",";
    }
    fields.emplace_back("tags", tags_str);
  }
  
  if (metadata.notebook().has_value()) {
    fields.emplace_back("notebook", metadata.notebook().value());
  }
  
  // Add custom fields
  for (const auto& [key, value] : metadata.customFields()) {
    fields.emplace_back(key, value);
  }
  
  return fields;
}

} // namespace nx::cli