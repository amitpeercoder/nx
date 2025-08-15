#include "nx/cli/commands/suggest_links_command.hpp"

#include <iostream>
#include <algorithm>
#include <sstream>
#include <set>
#include <nlohmann/json.hpp>

#include "nx/core/note_id.hpp"
#include "nx/util/http_client.hpp"

namespace nx::cli {

SuggestLinksCommand::SuggestLinksCommand(Application& app) : app_(app) {
}

void SuggestLinksCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("note_id", note_id_, "ID of the note to suggest links for")->required();
  cmd->add_flag("--apply", apply_, "Apply the suggested links to the note (add wikilinks)");
  cmd->add_flag("--dry-run", dry_run_, "Show what would be done without making changes");
  cmd->add_option("--limit,-l", limit_, "Maximum number of links to suggest")
     ->check(CLI::Range(1, 20));
  cmd->add_option("--min-score", min_score_, "Minimum relevance score (0.0-1.0)")
     ->check(CLI::Range(0.0, 1.0));
}

Result<int> SuggestLinksCommand::execute(const GlobalOptions& options) {
  try {
    // Validate AI configuration
    auto ai_check = validateAiConfig();
    if (!ai_check.has_value()) {
      return std::unexpected(ai_check.error());
    }
    
    // Load the target note
    auto note_result = loadNote();
    if (!note_result.has_value()) {
      return std::unexpected(note_result.error());
    }
    
    auto note = note_result.value();
    
    if (note.content().empty() || note.content() == "# " + note.title() + "\n\n") {
      if (options.json) {
        nlohmann::json result;
        result["note_id"] = note_id_;
        result["suggestions"] = nlohmann::json::array();
        result["message"] = "Note content is empty";
        result["success"] = true;
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "Note content is empty. No links to suggest." << std::endl;
      }
      return 0;
    }
    
    // Get all other notes for comparison
    auto other_notes_result = getAllOtherNotes();
    if (!other_notes_result.has_value()) {
      return std::unexpected(other_notes_result.error());
    }
    
    auto other_notes = other_notes_result.value();
    
    if (other_notes.empty()) {
      if (options.json) {
        nlohmann::json result;
        result["note_id"] = note_id_;
        result["suggestions"] = nlohmann::json::array();
        result["message"] = "No other notes found in collection";
        result["success"] = true;
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "No other notes found in your collection to link to." << std::endl;
      }
      return 0;
    }
    
    // Generate link suggestions
    auto suggestions_result = suggestLinks(note, other_notes);
    if (!suggestions_result.has_value()) {
      return std::unexpected(suggestions_result.error());
    }
    
    auto suggestions = suggestions_result.value();
    
    // Filter by minimum score
    suggestions.erase(std::remove_if(suggestions.begin(), suggestions.end(),
                                   [this](const LinkSuggestion& s) {
                                     return s.relevance_score < min_score_;
                                   }), suggestions.end());
    
    // Limit results
    if (suggestions.size() > static_cast<size_t>(limit_)) {
      suggestions.resize(static_cast<size_t>(limit_));
    }
    
    if (suggestions.empty()) {
      if (options.json) {
        nlohmann::json result;
        result["note_id"] = note_id_;
        result["suggestions"] = nlohmann::json::array();
        result["message"] = "No relevant links found above minimum score";
        result["min_score"] = min_score_;
        result["success"] = true;
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "No relevant link suggestions found above minimum score of " 
                  << min_score_ << std::endl;
      }
      return 0;
    }
    
    // Handle dry-run
    if (dry_run_) {
      if (options.json) {
        nlohmann::json result;
        result["note_id"] = note_id_;
        result["would_apply"] = apply_;
        result["dry_run"] = true;
        
        nlohmann::json suggestions_json = nlohmann::json::array();
        for (const auto& suggestion : suggestions) {
          nlohmann::json s_json;
          s_json["target_note_id"] = suggestion.target_note_id;
          s_json["target_title"] = suggestion.target_title;
          s_json["reason"] = suggestion.reason;
          s_json["relevance_score"] = suggestion.relevance_score;
          s_json["suggested_text"] = suggestion.suggested_text;
          s_json["insertion_point"] = suggestion.insertion_point;
          suggestions_json.push_back(s_json);
        }
        result["suggestions"] = suggestions_json;
        
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "Dry run mode - no changes will be made." << std::endl;
        std::cout << std::endl;
        std::cout << "Link suggestions for note: " << note.title() << " (" << note_id_ << ")" << std::endl;
        std::cout << std::endl;
        
        for (size_t i = 0; i < suggestions.size(); ++i) {
          const auto& suggestion = suggestions[i];
          std::cout << (i + 1) << ". [[" << suggestion.suggested_text << "]] ";
          std::cout << "(score: " << std::fixed << std::setprecision(2) << suggestion.relevance_score << ")" << std::endl;
          std::cout << "   Target: " << suggestion.target_title << " (" << suggestion.target_note_id << ")" << std::endl;
          std::cout << "   Reason: " << suggestion.reason << std::endl;
          if (suggestion.insertion_point > 0) {
            std::cout << "   Suggested insertion: line " << suggestion.insertion_point << std::endl;
          }
          std::cout << std::endl;
        }
        
        if (apply_) {
          std::cout << "Would add these wikilinks to the note." << std::endl;
        }
      }
      return 0;
    }
    
    // Apply links if requested
    if (apply_) {
      auto apply_result = applyLinks(suggestions);
      if (!apply_result.has_value()) {
        return std::unexpected(apply_result.error());
      }
      
      if (!options.quiet) {
        std::cout << "Added " << suggestions.size() << " wikilink(s) to the note." << std::endl;
      }
    }
    
    // Output results
    outputResult(suggestions, options);
    
    return 0;
    
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kUnknownError, 
                                     "Failed to execute suggest-links command: " + std::string(e.what())));
  }
}

Result<void> SuggestLinksCommand::validateAiConfig() {
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

Result<nx::core::Note> SuggestLinksCommand::loadNote() {
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

Result<std::vector<nx::core::Note>> SuggestLinksCommand::getAllOtherNotes() {
  // Use the efficient note store interface to get all notes
  nx::store::NoteQuery query;
  query.limit = 0;  // No limit - get all notes
  
  // Get all note IDs efficiently
  auto note_ids_result = app_.noteStore().list(query);
  if (!note_ids_result.has_value()) {
    return std::unexpected(note_ids_result.error());
  }
  
  std::vector<nx::core::Note> notes;
  auto target_id = nx::core::NoteId::fromString(note_id_).value();
  
  // Filter out the target note and collect IDs for batch loading
  std::vector<nx::core::NoteId> ids_to_load;
  for (const auto& note_id : note_ids_result.value()) {
    if (note_id != target_id) {
      ids_to_load.push_back(note_id);
    }
  }
  
  // Use batch loading for better performance
  auto notes_result = app_.noteStore().loadBatch(ids_to_load);
  if (!notes_result.has_value()) {
    return std::unexpected(notes_result.error());
  }
  
  return notes_result.value();
}

Result<std::vector<SuggestLinksCommand::LinkSuggestion>> SuggestLinksCommand::suggestLinks(
    const nx::core::Note& note, const std::vector<nx::core::Note>& other_notes) {
  
  const auto& ai_config = app_.config().ai.value();
  
  if (ai_config.provider != "anthropic") {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                   "Only Anthropic provider is currently supported"));
  }
  
  // Prepare context of other notes for AI analysis
  std::string other_notes_context = "Available notes to link to:\n\n";
  for (size_t i = 0; i < other_notes.size() && i < 10; ++i) {  // Limit for API call size
    const auto& other_note = other_notes[i];
    other_notes_context += std::to_string(i + 1) + ". \"" + other_note.title() + "\" (" + 
                          other_note.id().toString() + ")\n";
    other_notes_context += "   Content preview: " + 
                          other_note.content().substr(0, std::min(other_note.content().length(), size_t(100))) + "...\n";
    
    const auto& tags = other_note.metadata().tags();
    if (!tags.empty()) {
      other_notes_context += "   Tags: ";
      for (size_t j = 0; j < tags.size(); ++j) {
        if (j > 0) other_notes_context += ", ";
        other_notes_context += tags[j];
      }
      other_notes_context += "\n";
    }
    other_notes_context += "\n";
  }
  
  // Prepare the request payload for Anthropic API
  nlohmann::json request_body;
  request_body["model"] = ai_config.model;
  request_body["max_tokens"] = 1024;
  
  std::string system_prompt = "You are a helpful assistant that suggests relevant links between notes. "
                             "Analyze the current note and suggest which other notes would be most relevant to link to. "
                             "Consider topic similarity, shared concepts, complementary information, and thematic connections. "
                             "Return a JSON array where each suggestion has: \"target_note_id\", \"target_title\", \"reason\", "
                             "\"relevance_score\" (0.0-1.0), \"suggested_text\", \"insertion_point\" (line number, 0 for end). "
                             "Suggest up to " + std::to_string(limit_) + " most relevant links.";
  
  request_body["system"] = system_prompt;
  
  nlohmann::json messages = nlohmann::json::array();
  nlohmann::json user_message;
  user_message["role"] = "user";
  user_message["content"] = "Current note to suggest links for:\n\nTitle: " + note.title() + 
                           "\nContent:\n" + note.content() + "\n\n" + other_notes_context + 
                           "\nSuggest the most relevant notes to link to and explain why:";
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
    std::vector<LinkSuggestion> suggestions;
    try {
      auto links_json = nlohmann::json::parse(ai_response);
      if (!links_json.is_array()) {
        return std::unexpected(makeError(ErrorCode::kParseError, 
                                       "AI response is not a JSON array"));
      }
      
      for (const auto& link_json : links_json) {
        if (!link_json.is_object()) continue;
        
        LinkSuggestion suggestion;
        if (link_json.contains("target_note_id") && link_json["target_note_id"].is_string()) {
          suggestion.target_note_id = link_json["target_note_id"].get<std::string>();
        }
        if (link_json.contains("target_title") && link_json["target_title"].is_string()) {
          suggestion.target_title = link_json["target_title"].get<std::string>();
        }
        if (link_json.contains("reason") && link_json["reason"].is_string()) {
          suggestion.reason = link_json["reason"].get<std::string>();
        }
        if (link_json.contains("relevance_score") && link_json["relevance_score"].is_number()) {
          suggestion.relevance_score = link_json["relevance_score"].get<double>();
        }
        if (link_json.contains("suggested_text") && link_json["suggested_text"].is_string()) {
          suggestion.suggested_text = link_json["suggested_text"].get<std::string>();
        }
        if (link_json.contains("insertion_point") && link_json["insertion_point"].is_number()) {
          suggestion.insertion_point = link_json["insertion_point"].get<int>();
        }
        
        // Validate and set defaults
        if (suggestion.target_note_id.empty() || suggestion.target_title.empty()) {
          continue;  // Skip invalid suggestions
        }
        
        if (suggestion.suggested_text.empty()) {
          suggestion.suggested_text = suggestion.target_title;
        }
        
        if (suggestion.relevance_score < 0.0 || suggestion.relevance_score > 1.0) {
          suggestion.relevance_score = 0.5;  // Default
        }
        
        suggestions.push_back(suggestion);
      }
      
    } catch (const nlohmann::json::exception&) {
      // Fallback: try to extract suggestions from free text response
      std::istringstream stream(ai_response);
      std::string line;
      while (std::getline(stream, line) && suggestions.size() < static_cast<size_t>(limit_)) {
        // Look for note references in the response
        for (const auto& other_note : other_notes) {
          if (line.find(other_note.title()) != std::string::npos || 
              line.find(other_note.id().toString()) != std::string::npos) {
            
            LinkSuggestion suggestion;
            suggestion.target_note_id = other_note.id().toString();
            suggestion.target_title = other_note.title();
            suggestion.suggested_text = other_note.title();
            suggestion.relevance_score = 0.6;  // Default fallback score
            suggestion.reason = "AI identified as relevant";
            suggestion.insertion_point = 0;  // End of note
            
            // Check if we already have this suggestion
            bool already_added = false;
            for (const auto& existing : suggestions) {
              if (existing.target_note_id == suggestion.target_note_id) {
                already_added = true;
                break;
              }
            }
            
            if (!already_added) {
              suggestions.push_back(suggestion);
            }
            break;
          }
        }
      }
    }
    
    return suggestions;
    
  } catch (const nlohmann::json::exception& e) {
    return std::unexpected(makeError(ErrorCode::kParseError, 
                                   "Failed to parse Anthropic API response: " + std::string(e.what())));
  }
}

Result<void> SuggestLinksCommand::applyLinks(const std::vector<LinkSuggestion>& suggestions) {
  // Load the current note
  auto note_result = loadNote();
  if (!note_result.has_value()) {
    return std::unexpected(note_result.error());
  }
  
  auto note = note_result.value();
  std::string content = note.content();
  
  // Add links section at the end of the note
  content += "\n\n## Related Notes\n\n";
  
  for (const auto& suggestion : suggestions) {
    content += "- [[" + suggestion.suggested_text + "]] - " + suggestion.reason + "\n";
  }
  
  // Create updated note
  auto updated_metadata = note.metadata();
  updated_metadata.touch();  // Update modified time
  
  nx::core::Note updated_note(std::move(updated_metadata), content);
  
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

void SuggestLinksCommand::outputResult(const std::vector<LinkSuggestion>& suggestions, 
                                      const GlobalOptions& options) {
  if (options.json) {
    nlohmann::json result;
    result["note_id"] = note_id_;
    result["total_suggestions"] = suggestions.size();
    
    nlohmann::json suggestions_json = nlohmann::json::array();
    for (const auto& suggestion : suggestions) {
      nlohmann::json s_json;
      s_json["target_note_id"] = suggestion.target_note_id;
      s_json["target_title"] = suggestion.target_title;
      s_json["reason"] = suggestion.reason;
      s_json["relevance_score"] = suggestion.relevance_score;
      s_json["suggested_text"] = suggestion.suggested_text;
      s_json["insertion_point"] = suggestion.insertion_point;
      suggestions_json.push_back(s_json);
    }
    result["suggestions"] = suggestions_json;
    result["applied"] = apply_;
    result["success"] = true;
    
    std::cout << result.dump(2) << std::endl;
  } else {
    if (!apply_) {
      std::cout << "Link suggestions for note " << note_id_ << ":" << std::endl;
      std::cout << std::endl;
      
      for (size_t i = 0; i < suggestions.size(); ++i) {
        const auto& suggestion = suggestions[i];
        std::cout << (i + 1) << ". [[" << suggestion.suggested_text << "]]" << std::endl;
        std::cout << "   Target: " << suggestion.target_title << " (" << suggestion.target_note_id << ")" << std::endl;
        std::cout << "   Relevance: " << std::fixed << std::setprecision(2) << suggestion.relevance_score << std::endl;
        std::cout << "   Reason: " << suggestion.reason << std::endl;
        std::cout << std::endl;
      }
      
      std::cout << "To apply these links, use: nx suggest-links " << note_id_ << " --apply" << std::endl;
    }
  }
}

} // namespace nx::cli