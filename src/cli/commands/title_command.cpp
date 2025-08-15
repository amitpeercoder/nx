#include "nx/cli/commands/title_command.hpp"

#include <iostream>
#include <algorithm>
#include <nlohmann/json.hpp>

#include "nx/core/note_id.hpp"
#include "nx/util/http_client.hpp"

namespace nx::cli {

TitleCommand::TitleCommand(Application& app) : app_(app) {
}

void TitleCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("note_id", note_id_, "ID of the note to suggest titles for")->required();
  cmd->add_flag("--apply", apply_, "Apply the first suggested title to the note");
  cmd->add_flag("--dry-run", dry_run_, "Show what would be done without making changes");
  cmd->add_option("--count,-c", count_, "Number of title suggestions to generate")
     ->check(CLI::Range(1, 10));
}

Result<int> TitleCommand::execute(const GlobalOptions& options) {
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
    std::string current_title = note.title();
    
    // Generate title suggestions
    auto suggestions_result = suggestTitles(note);
    if (!suggestions_result.has_value()) {
      return std::unexpected(suggestions_result.error());
    }
    
    auto suggested_titles = suggestions_result.value();
    
    if (suggested_titles.empty()) {
      if (options.json) {
        nlohmann::json result;
        result["note_id"] = note_id_;
        result["current_title"] = current_title;
        result["suggested_titles"] = nlohmann::json::array();
        result["message"] = "No title suggestions generated";
        result["success"] = true;
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "No title suggestions could be generated for this note." << std::endl;
        std::cout << "Current title: " << current_title << std::endl;
      }
      return 0;
    }
    
    // Handle dry-run
    if (dry_run_) {
      if (options.json) {
        nlohmann::json result;
        result["note_id"] = note_id_;
        result["current_title"] = current_title;
        result["suggested_titles"] = suggested_titles;
        result["would_apply"] = apply_ ? suggested_titles[0] : "";
        result["dry_run"] = true;
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "Dry run mode - no changes will be made." << std::endl;
        std::cout << std::endl;
        std::cout << "Note: " << note_id_ << std::endl;
        std::cout << "Current title: " << current_title << std::endl;
        std::cout << std::endl;
        std::cout << "Suggested titles:" << std::endl;
        for (size_t i = 0; i < suggested_titles.size(); ++i) {
          std::cout << "  " << (i + 1) << ". " << suggested_titles[i] << std::endl;
        }
        
        if (apply_) {
          std::cout << std::endl;
          std::cout << "Would update note title to: \"" << suggested_titles[0] << "\"" << std::endl;
        }
      }
      return 0;
    }
    
    // Apply title if requested
    if (apply_ && !suggested_titles.empty()) {
      auto apply_result = applyTitle(suggested_titles[0]);
      if (!apply_result.has_value()) {
        return std::unexpected(apply_result.error());
      }
      
      if (!options.quiet) {
        std::cout << "Note title updated to: \"" << suggested_titles[0] << "\"" << std::endl;
      }
    }
    
    // Output result
    outputResult(suggested_titles, current_title, options);
    
    return 0;
    
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kUnknownError, 
                                     "Failed to execute title command: " + std::string(e.what())));
  }
}

Result<void> TitleCommand::validateAiConfig() {
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

Result<nx::core::Note> TitleCommand::loadNote() {
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

Result<std::vector<std::string>> TitleCommand::suggestTitles(const nx::core::Note& note) {
  const auto& ai_config = app_.config().ai.value();
  
  if (ai_config.provider != "anthropic") {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                   "Only Anthropic provider is currently supported"));
  }
  
  // Prepare the request payload for Anthropic API
  nlohmann::json request_body;
  request_body["model"] = ai_config.model;
  request_body["max_tokens"] = 256;
  
  std::string system_prompt = "You are a helpful assistant that suggests concise, descriptive titles for notes. "
                             "Analyze the note content and suggest " + std::to_string(count_) + 
                             " different title options. Titles should be clear, specific, and capture the main topic. "
                             "Return only a JSON array of title strings, no other text.";
  
  request_body["system"] = system_prompt;
  
  nlohmann::json messages = nlohmann::json::array();
  nlohmann::json user_message;
  user_message["role"] = "user";
  
  std::string context = "Current title: " + note.title() + "\n\nNote content:\n" + note.content();
  context += "\n\nSuggest " + std::to_string(count_) + " better titles for this note:";
  
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
      auto titles_json = nlohmann::json::parse(ai_response);
      if (!titles_json.is_array()) {
        return std::unexpected(makeError(ErrorCode::kParseError, 
                                       "AI response is not a JSON array"));
      }
      
      std::vector<std::string> suggestions;
      for (const auto& title_json : titles_json) {
        if (title_json.is_string()) {
          suggestions.push_back(title_json.get<std::string>());
        }
      }
      
      return suggestions;
      
    } catch (const nlohmann::json::exception&) {
      // Fallback: try to extract titles from free text response
      std::vector<std::string> suggestions;
      std::istringstream stream(ai_response);
      std::string line;
      while (std::getline(stream, line) && suggestions.size() < static_cast<size_t>(count_)) {
        // Remove common prefixes and clean up
        size_t pos = line.find("- ");
        if (pos != std::string::npos) {
          line = line.substr(pos + 2);
        }
        pos = line.find(". ");
        if (pos != std::string::npos && pos < 5) {  // Only if number is at start
          line = line.substr(pos + 2);
        }
        
        // Remove quotes if present
        if (line.front() == '"' && line.back() == '"') {
          line = line.substr(1, line.length() - 2);
        }
        
        // Clean and validate title
        line.erase(0, line.find_first_not_of(" \t"));
        line.erase(line.find_last_not_of(" \t") + 1);
        
        if (!line.empty() && line.length() > 3) {
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

Result<void> TitleCommand::applyTitle(const std::string& new_title) {
  // Load the current note
  auto note_result = loadNote();
  if (!note_result.has_value()) {
    return std::unexpected(note_result.error());
  }
  
  auto note = note_result.value();
  
  // Update note metadata with new title
  auto updated_metadata = note.metadata();
  updated_metadata.setTitle(new_title);
  updated_metadata.touch();  // Update modified time
  
  // Update content if it starts with a title heading
  std::string updated_content = note.content();
  if (updated_content.starts_with("# ")) {
    // Replace the first line (title heading)
    size_t first_newline = updated_content.find('\n');
    if (first_newline != std::string::npos) {
      updated_content = "# " + new_title + updated_content.substr(first_newline);
    } else {
      updated_content = "# " + new_title;
    }
  }
  
  // Create updated note
  nx::core::Note updated_note(std::move(updated_metadata), updated_content);
  
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

void TitleCommand::outputResult(const std::vector<std::string>& suggested_titles, 
                               const std::string& current_title, 
                               const GlobalOptions& options) {
  if (options.json) {
    nlohmann::json result;
    result["note_id"] = note_id_;
    result["current_title"] = current_title;
    result["suggested_titles"] = suggested_titles;
    result["applied"] = apply_ ? (suggested_titles.empty() ? "" : suggested_titles[0]) : "";
    result["success"] = true;
    
    std::cout << result.dump(2) << std::endl;
  } else {
    if (!apply_) {
      std::cout << "Title suggestions for note " << note_id_ << ":" << std::endl;
      std::cout << "Current: " << current_title << std::endl;
      std::cout << std::endl;
      std::cout << "Suggestions:" << std::endl;
      for (size_t i = 0; i < suggested_titles.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << suggested_titles[i] << std::endl;
      }
      
      if (!suggested_titles.empty()) {
        std::cout << std::endl;
        std::cout << "To apply the first suggestion, use: nx title " << note_id_ << " --apply" << std::endl;
      }
    }
  }
}

} // namespace nx::cli