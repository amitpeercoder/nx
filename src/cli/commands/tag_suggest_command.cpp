#include "nx/cli/commands/tag_suggest_command.hpp"

#include <iostream>
#include <algorithm>
#include <set>
#include <nlohmann/json.hpp>

#include "nx/core/note_id.hpp"
#include "nx/util/http_client.hpp"

namespace nx::cli {

TagSuggestCommand::TagSuggestCommand(Application& app) : app_(app) {
}

void TagSuggestCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("note_id", note_id_, "ID of the note to suggest tags for")->required();
  cmd->add_flag("--apply", apply_, "Apply the suggested tags to the note");
  cmd->add_flag("--dry-run", dry_run_, "Show what would be done without making changes");
  cmd->add_option("--limit,-l", limit_, "Maximum number of tags to suggest")
     ->check(CLI::Range(1, 10));
}

Result<int> TagSuggestCommand::execute(const GlobalOptions& options) {
  try {
    // Validate AI configuration
    auto ai_check = validateAiConfig();
    if (!ai_check.has_value()) {
      return std::unexpected(ai_check.error());
    }
    
    // Load the note
    auto note_result = loadNote();
    if (!note_result.has_value()) {
      return std::unexpected(note_result.error());
    }
    
    auto note = note_result.value();
    
    // Get existing tags from the collection for context
    auto existing_tags_result = getExistingTags();
    if (!existing_tags_result.has_value()) {
      return std::unexpected(existing_tags_result.error());
    }
    
    auto existing_tags = existing_tags_result.value();
    
    // Generate tag suggestions
    auto suggestions_result = suggestTags(note);
    if (!suggestions_result.has_value()) {
      return std::unexpected(suggestions_result.error());
    }
    
    auto suggested_tags = suggestions_result.value();
    
    // Filter out tags that are already on the note
    const auto& current_tags = note.metadata().tags();
    std::vector<std::string> new_suggestions;
    for (const auto& tag : suggested_tags) {
      if (std::find(current_tags.begin(), current_tags.end(), tag) == current_tags.end()) {
        new_suggestions.push_back(tag);
      }
    }
    
    if (new_suggestions.empty()) {
      if (options.json) {
        nlohmann::json result;
        result["note_id"] = note_id_;
        result["existing_tags"] = current_tags;
        result["suggested_tags"] = nlohmann::json::array();
        result["message"] = "No new tags to suggest - note already has relevant tags";
        result["success"] = true;
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "No new tags to suggest for this note." << std::endl;
        if (!current_tags.empty()) {
          std::cout << "Current tags: ";
          for (size_t i = 0; i < current_tags.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << current_tags[i];
          }
          std::cout << std::endl;
        }
      }
      return 0;
    }
    
    // Handle dry-run
    if (dry_run_) {
      if (options.json) {
        nlohmann::json result;
        result["note_id"] = note_id_;
        result["existing_tags"] = current_tags;
        result["suggested_tags"] = new_suggestions;
        result["would_apply"] = apply_;
        result["dry_run"] = true;
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "Dry run mode - no changes will be made." << std::endl;
        std::cout << std::endl;
        std::cout << "Note: " << note.title() << " (" << note_id_ << ")" << std::endl;
        
        if (!current_tags.empty()) {
          std::cout << "Current tags: ";
          for (size_t i = 0; i < current_tags.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << current_tags[i];
          }
          std::cout << std::endl;
        }
        
        std::cout << "Suggested tags: ";
        for (size_t i = 0; i < new_suggestions.size(); ++i) {
          if (i > 0) std::cout << ", ";
          std::cout << new_suggestions[i];
        }
        std::cout << std::endl;
        
        if (apply_) {
          std::cout << std::endl;
          std::cout << "Would add these tags to the note." << std::endl;
        }
      }
      return 0;
    }
    
    // Apply tags if requested
    if (apply_) {
      auto apply_result = applyTags(new_suggestions);
      if (!apply_result.has_value()) {
        return std::unexpected(apply_result.error());
      }
      
      if (!options.quiet) {
        std::cout << "Added " << new_suggestions.size() << " new tag(s) to the note." << std::endl;
      }
    }
    
    // Output result
    outputResult(new_suggestions, existing_tags, options);
    
    return 0;
    
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kUnknownError, 
                                     "Failed to execute tag-suggest command: " + std::string(e.what())));
  }
}

Result<void> TagSuggestCommand::validateAiConfig() {
  const auto& config = app_.config();
  
  if (!config.ai.has_value()) {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                     "AI is not configured. Please configure AI settings in your config file."));
  }
  
  const auto& ai_config = config.ai.value();
  
  if (ai_config.provider.empty() || ai_config.model.empty() || ai_config.api_key.empty()) {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                     "AI configuration is incomplete. Please check provider, model, and API key."));
  }
  
  return {};
}

Result<nx::core::Note> TagSuggestCommand::loadNote() {
  // Parse note ID
  auto parsed_id = nx::core::NoteId::fromString(note_id_);
  if (!parsed_id.has_value()) {
    return std::unexpected(makeError(ErrorCode::kInvalidArgument, 
                                     "Invalid note ID format: " + note_id_));
  }
  
  // Load note from store
  auto note_result = app_.noteStore().load(parsed_id.value());
  if (!note_result.has_value()) {
    return std::unexpected(makeError(ErrorCode::kFileNotFound, 
                                     "Note not found: " + note_id_));
  }
  
  return note_result.value();
}

Result<std::vector<std::string>> TagSuggestCommand::suggestTags(const nx::core::Note& note) {
  const auto& ai_config = app_.config().ai.value();
  
  if (ai_config.provider != "anthropic") {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                   "Only Anthropic provider is currently supported"));
  }
  
  // Get existing tags for context
  auto existing_tags_result = getExistingTags();
  std::vector<std::string> existing_tags;
  if (existing_tags_result.has_value()) {
    existing_tags = existing_tags_result.value();
  }
  
  // Prepare the request payload for Anthropic API
  nlohmann::json request_body;
  request_body["model"] = ai_config.model;
  request_body["max_tokens"] = 512;
  
  std::string system_prompt = "You are a helpful assistant that suggests relevant tags for notes. "
                             "Analyze the note content and suggest " + std::to_string(limit_) + 
                             " concise, relevant tags. Tags should be lowercase, single words or short phrases with hyphens. "
                             "Return only a JSON array of tag strings, no other text.";
  
  request_body["system"] = system_prompt;
  
  nlohmann::json messages = nlohmann::json::array();
  nlohmann::json user_message;
  user_message["role"] = "user";
  
  std::string context = "Note title: " + note.title() + "\n\nNote content:\n" + note.content();
  
  if (!existing_tags.empty()) {
    context += "\n\nExisting tags in the collection (for consistency): ";
    for (size_t i = 0; i < existing_tags.size() && i < 20; ++i) {
      if (i > 0) context += ", ";
      context += existing_tags[i];
    }
  }
  
  context += "\n\nSuggest " + std::to_string(limit_) + " relevant tags for this note:";
  user_message["content"] = context;
  messages.push_back(user_message);
  
  request_body["messages"] = messages;
  
  // Make HTTP request to Anthropic API
  nx::util::HttpClient client;
  
  std::vector<std::string> headers = {
    "Content-Type: application/json",
    "x-api-key: " + ai_config.api_key,
    "anthropic-version: 2023-06-01"
  };
  
  auto response = client.post("https://api.anthropic.com/v1/messages", 
                             request_body.dump(), headers);
  
  if (!response.has_value()) {
    return std::unexpected(makeError(ErrorCode::kNetworkError, 
                                   "Failed to call Anthropic API: " + response.error().message()));
  }
  
  if (response->status_code != 200) {
    return std::unexpected(makeError(ErrorCode::kNetworkError, 
                                   "Anthropic API returned error " + std::to_string(response->status_code) + 
                                   ": " + response->body));
  }
  
  // Parse response
  try {
    auto response_json = nlohmann::json::parse(response->body);
    
    if (!response_json.contains("content") || !response_json["content"].is_array() || 
        response_json["content"].empty()) {
      return std::unexpected(makeError(ErrorCode::kParseError, 
                                     "Invalid response format from Anthropic API"));
    }
    
    auto& content_obj = response_json["content"][0];
    if (!content_obj.contains("text") || !content_obj["text"].is_string()) {
      return std::unexpected(makeError(ErrorCode::kParseError, 
                                     "Missing text content in Anthropic API response"));
    }
    
    std::string ai_response = content_obj["text"].get<std::string>();
    
    // Try to parse the AI response as JSON array
    try {
      auto tags_json = nlohmann::json::parse(ai_response);
      if (!tags_json.is_array()) {
        return std::unexpected(makeError(ErrorCode::kParseError, 
                                       "AI response is not a JSON array"));
      }
      
      std::vector<std::string> suggestions;
      for (const auto& tag_json : tags_json) {
        if (tag_json.is_string()) {
          suggestions.push_back(tag_json.get<std::string>());
        }
      }
      
      return suggestions;
      
    } catch (const nlohmann::json::exception&) {
      // Fallback: try to extract tags from free text response
      std::vector<std::string> suggestions;
      std::istringstream stream(ai_response);
      std::string line;
      while (std::getline(stream, line) && suggestions.size() < static_cast<size_t>(limit_)) {
        // Remove common prefixes and clean up
        size_t pos = line.find("- ");
        if (pos != std::string::npos) {
          line = line.substr(pos + 2);
        }
        pos = line.find(". ");
        if (pos != std::string::npos) {
          line = line.substr(pos + 2);
        }
        
        // Clean and validate tag
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (!line.empty() && line.find(' ') == std::string::npos) {
          std::transform(line.begin(), line.end(), line.begin(), ::tolower);
          suggestions.push_back(line);
        }
      }
      
      return suggestions;
    }
    
  } catch (const nlohmann::json::exception& e) {
    return std::unexpected(makeError(ErrorCode::kParseError, 
                                   "Failed to parse Anthropic API response: " + std::string(e.what())));
  }
}

Result<std::vector<std::string>> TagSuggestCommand::getExistingTags() {
  // Get all existing tags in the collection for better AI context
  // This helps the AI suggest consistent tags that fit with the existing taxonomy
  
  std::vector<std::string> existing_tags;
  
  // Get all existing tags from the note store
  auto tags_result = app_.noteStore().getAllTags();
  if (tags_result) {
    existing_tags = *tags_result;
  } else {
    // Fallback to some default suggestions if store fails
    existing_tags = {"work", "personal", "ideas", "meetings", "research", "projects"};
  }
  existing_tags.push_back("todo");
  existing_tags.push_back("notes");
  
  return existing_tags;
}

Result<void> TagSuggestCommand::applyTags(const std::vector<std::string>& suggested_tags) {
  // Load the current note
  auto note_result = loadNote();
  if (!note_result.has_value()) {
    return std::unexpected(note_result.error());
  }
  
  auto note = note_result.value();
  
  // Add suggested tags to existing tags
  auto updated_metadata = note.metadata();
  for (const auto& tag : suggested_tags) {
    updated_metadata.addTag(tag);
  }
  updated_metadata.touch();  // Update modified time
  
  // Create updated note
  nx::core::Note updated_note(std::move(updated_metadata), note.content());
  
  // Store the updated note
  auto store_result = app_.noteStore().store(updated_note);
  if (!store_result.has_value()) {
    return std::unexpected(store_result.error());
  }
  
  // Update search index
  auto index_result = app_.searchIndex().updateNote(updated_note);
  if (!index_result.has_value()) {
    // Non-fatal - warn but continue
    std::cerr << "Warning: Failed to update search index: " 
              << index_result.error().message() << std::endl;
  }
  
  return {};
}

void TagSuggestCommand::outputResult(const std::vector<std::string>& suggested_tags, 
                                   const std::vector<std::string>& existing_tags, 
                                   const GlobalOptions& options) {
  if (options.json) {
    nlohmann::json result;
    result["note_id"] = note_id_;
    result["suggested_tags"] = suggested_tags;
    result["existing_tags_in_collection"] = existing_tags;
    result["applied"] = apply_;
    result["success"] = true;
    
    std::cout << result.dump(2) << std::endl;
  } else {
    if (!apply_) {
      std::cout << "Suggested tags for note " << note_id_ << ":" << std::endl;
      for (size_t i = 0; i < suggested_tags.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << suggested_tags[i] << std::endl;
      }
      std::cout << std::endl;
      std::cout << "To apply these tags to the note, use: nx tag-suggest " << note_id_ << " --apply" << std::endl;
      
      if (options.verbose && !existing_tags.empty()) {
        std::cout << std::endl;
        std::cout << "Existing tags in your collection: ";
        for (size_t i = 0; i < existing_tags.size() && i < 10; ++i) {
          if (i > 0) std::cout << ", ";
          std::cout << existing_tags[i];
        }
        if (existing_tags.size() > 10) {
          std::cout << " (and " << (existing_tags.size() - 10) << " more)";
        }
        std::cout << std::endl;
      }
    }
  }
}

} // namespace nx::cli