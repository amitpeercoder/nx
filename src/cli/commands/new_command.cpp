#include "nx/cli/commands/new_command.hpp"

#include <iostream>
#include <sstream>
#include <unistd.h>
#include <nlohmann/json.hpp>

#include "nx/core/note.hpp"
#include "nx/core/metadata.hpp"
#include "nx/util/http_client.hpp"

namespace nx::cli {

NewCommand::NewCommand(Application& app) : app_(app) {
}

void NewCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("title", title_, "Note title (optional)");
  cmd->add_option("--tags,-t", tags_, "Tags for the note (comma-separated)");
  cmd->add_option("--notebook,--nb", notebook_, "Notebook name");
  cmd->add_option("--from", template_name_, "Create from template");
  cmd->add_flag("--edit,-e", edit_after_create_, "Edit after creation");
}

Result<int> NewCommand::execute(const GlobalOptions& options) {
  try {
    // Generate new note ID
    auto note_id = nx::core::NoteId::generate();
    
    // Create metadata (title will be updated after AI processing if needed)
    nx::core::Metadata metadata(note_id, title_.empty() ? "Untitled" : title_);
    
    // Set notebook
    if (!notebook_.empty()) {
      metadata.setNotebook(notebook_);
    }
    
    // Determine content based on priority:
    // 1. Template content (if --from specified)
    // 2. Stdin input (if available)
    // 3. Default content
    std::string content;
    std::string body_content;
    
    if (!template_name_.empty()) {
      // Simple template system - check for common templates
      if (template_name_ == "daily" || template_name_ == "journal") {
        body_content = "# Daily Journal\n\n## Tasks\n- [ ] \n\n## Notes\n\n## Reflection\n\n";
      } else if (template_name_ == "meeting") {
        body_content = "# Meeting Notes\n\n**Date:** \n**Attendees:** \n\n## Agenda\n- \n\n## Discussion\n\n## Action Items\n- [ ] \n\n";
      } else if (template_name_ == "project") {
        body_content = "# Project Notes\n\n## Overview\n\n## Goals\n- \n\n## Tasks\n- [ ] \n\n## Resources\n\n";
      } else {
        // Unknown template - use default with note about template
        body_content = "# " + title_ + "\n\n*Note: Template '" + template_name_ + "' not found, using default.*\n\n";
      }
    } else if (hasStdinInput()) {
      // Read content from stdin
      body_content = readStdinContent();
      
      // Remove trailing newline if present
      if (!body_content.empty() && body_content.back() == '\n') {
        body_content.pop_back();
      }
      
      // Use AI to auto-generate missing title and/or tags for piped content
      if (!body_content.empty() && app_.config().ai.has_value()) {
        // Auto-generate title if not provided
        if (title_.empty()) {
          auto title_result = generateTitleFromContent(body_content);
          if (title_result.has_value()) {
            title_ = *title_result;
            if (!options.quiet) {
              std::cerr << "AI-generated title: " << title_ << std::endl;
            }
          } else if (options.verbose) {
            std::cerr << "Warning: Failed to generate title with AI: " << title_result.error().message() << std::endl;
          }
        }
        
        // Auto-generate tags if not provided
        if (tags_.empty()) {
          auto tags_result = generateTagsFromContent(body_content);
          if (tags_result.has_value()) {
            tags_ = *tags_result;
            if (!options.quiet && !tags_.empty()) {
              std::cerr << "AI-generated tags: ";
              for (size_t i = 0; i < tags_.size(); ++i) {
                if (i > 0) std::cerr << ", ";
                std::cerr << tags_[i];
              }
              std::cerr << std::endl;
            }
          } else if (options.verbose) {
            std::cerr << "Warning: Failed to generate tags with AI: " << tags_result.error().message() << std::endl;
          }
        }
      }
    }
    
    // Update metadata title if AI generated one
    if (!title_.empty()) {
      metadata.setTitle(title_);
    }
    
    // Process tags after potential AI generation
    for (const auto& tag_string : tags_) {
      // Split comma-separated tags
      std::stringstream ss(tag_string);
      std::string tag;
      while (std::getline(ss, tag, ',')) {
        // Trim whitespace
        tag.erase(0, tag.find_first_not_of(" \t"));
        tag.erase(tag.find_last_not_of(" \t") + 1);
        if (!tag.empty()) {
          metadata.addTag(tag);
        }
      }
    }
    
    // Create final content with title header
    if (title_.empty()) {
      if (body_content.empty()) {
        content = "# Untitled\n\n";
      } else {
        content = "# Untitled\n\n" + body_content;
      }
    } else {
      if (body_content.empty()) {
        content = "# " + title_ + "\n\n";
      } else {
        content = "# " + title_ + "\n\n" + body_content;
      }
    }
    
    nx::core::Note note(std::move(metadata), content);
    
    // Store the note
    auto store_result = app_.noteStore().store(note);
    if (!store_result.has_value()) {
      return std::unexpected(store_result.error());
    }
    
    // Add to search index
    auto index_result = app_.searchIndex().addNote(note);
    if (!index_result.has_value()) {
      // Non-fatal error - warn but continue
      if (!options.quiet) {
        std::cerr << "Warning: Failed to add note to search index: " 
                  << index_result.error().message() << std::endl;
      }
    }
    
    // Output result
    if (options.json) {
      nlohmann::json result;
      result["id"] = note.id().toString();
      result["title"] = note.title();
      result["created"] = std::chrono::duration_cast<std::chrono::milliseconds>(
          note.metadata().created().time_since_epoch()).count();
      result["tags"] = note.metadata().tags();
      if (note.notebook().has_value()) {
        result["notebook"] = *note.notebook();
      }
      result["success"] = true;
      
      std::cout << result.dump(2) << std::endl;
    } else {
      std::cout << "Created note: " << note.id().toString();
      if (!note.title().empty()) {
        std::cout << " (" << note.title() << ")";
      }
      std::cout << std::endl;
      
      if (options.verbose) {
        std::cout << "  Tags: ";
        const auto& tags = note.metadata().tags();
        if (tags.empty()) {
          std::cout << "none";
        } else {
          for (size_t i = 0; i < tags.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << tags[i];
          }
        }
        std::cout << std::endl;
        
        if (note.notebook().has_value()) {
          std::cout << "  Notebook: " << *note.notebook() << std::endl;
        }
      }
    }
    
    return 0;
    
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kUnknownError, 
                                     "Failed to create note: " + std::string(e.what())));
  }
}

bool NewCommand::hasStdinInput() const {
  return !isatty(STDIN_FILENO);
}

std::string NewCommand::readStdinContent() const {
  std::ostringstream buffer;
  std::string line;
  
  while (std::getline(std::cin, line)) {
    buffer << line << '\n';
  }
  
  return buffer.str();
}

Result<std::string> NewCommand::generateTitleFromContent(const std::string& content) {
  const auto& ai_config = app_.config().ai.value();
  
  if (ai_config.provider != "anthropic") {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                   "Only Anthropic provider is currently supported"));
  }
  
  // Prepare the request payload for Anthropic API
  nlohmann::json request_body;
  request_body["model"] = ai_config.model;
  request_body["max_tokens"] = 128;
  
  std::string system_prompt = "You are a helpful assistant that generates concise, descriptive titles for notes based on their content. "
                             "Analyze the provided content and suggest a single, clear title that captures the main topic or purpose. "
                             "The title should be specific and informative. Return only the title text, no quotes or extra formatting.";
  
  request_body["system"] = system_prompt;
  
  nlohmann::json messages = nlohmann::json::array();
  nlohmann::json user_message;
  user_message["role"] = "user";
  
  // Limit content length to avoid token limits
  std::string limited_content = content;
  if (limited_content.length() > 2000) {
    limited_content = limited_content.substr(0, 2000) + "...";
  }
  
  std::string context = "Generate a concise title for this content:\n\n" + limited_content;
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
  
  try {
    nlohmann::json response_json = nlohmann::json::parse(response->body);
    
    if (response_json.contains("error")) {
      std::string error_message = "Anthropic API error";
      if (response_json["error"].contains("message")) {
        error_message = response_json["error"]["message"];
      }
      return std::unexpected(makeError(ErrorCode::kNetworkError, error_message));
    }
    
    if (!response_json.contains("content") || !response_json["content"].is_array() || 
        response_json["content"].empty()) {
      return std::unexpected(makeError(ErrorCode::kParseError, 
                                     "Invalid response format from Anthropic API"));
    }
    
    auto content_item = response_json["content"][0];
    if (!content_item.contains("text")) {
      return std::unexpected(makeError(ErrorCode::kParseError, 
                                     "No text content in Anthropic API response"));
    }
    
    std::string generated_title = content_item["text"];
    
    // Clean up the title - remove quotes and trim whitespace
    if (generated_title.front() == '"' && generated_title.back() == '"') {
      generated_title = generated_title.substr(1, generated_title.length() - 2);
    }
    
    // Trim whitespace
    generated_title.erase(0, generated_title.find_first_not_of(" \t\n\r"));
    generated_title.erase(generated_title.find_last_not_of(" \t\n\r") + 1);
    
    // Limit title length
    if (generated_title.length() > 100) {
      generated_title = generated_title.substr(0, 100);
    }
    
    return generated_title;
    
  } catch (const nlohmann::json::parse_error& e) {
    return std::unexpected(makeError(ErrorCode::kParseError, 
                                   "Failed to parse Anthropic API response: " + std::string(e.what())));
  }
}

Result<std::vector<std::string>> NewCommand::generateTagsFromContent(const std::string& content) {
  const auto& ai_config = app_.config().ai.value();
  
  if (ai_config.provider != "anthropic") {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                   "Only Anthropic provider is currently supported"));
  }
  
  // Get existing tags for context
  auto existing_tags_result = app_.noteStore().getAllTags();
  std::vector<std::string> existing_tags;
  if (existing_tags_result.has_value()) {
    existing_tags = *existing_tags_result;
  }
  
  // Prepare the request payload for Anthropic API
  nlohmann::json request_body;
  request_body["model"] = ai_config.model;
  request_body["max_tokens"] = 256;
  
  std::string system_prompt = "You are a helpful assistant that suggests relevant tags for notes. "
                             "Analyze the content and suggest 3-5 concise, relevant tags. Tags should be lowercase, "
                             "single words or short phrases with hyphens. Return only a JSON array of tag strings.";
  
  request_body["system"] = system_prompt;
  
  nlohmann::json messages = nlohmann::json::array();
  nlohmann::json user_message;
  user_message["role"] = "user";
  
  // Limit content length to avoid token limits
  std::string limited_content = content;
  if (limited_content.length() > 1500) {
    limited_content = limited_content.substr(0, 1500) + "...";
  }
  
  std::string context = "Content to tag:\n\n" + limited_content;
  
  if (!existing_tags.empty()) {
    context += "\n\nExisting tags in collection (for consistency): ";
    for (size_t i = 0; i < existing_tags.size() && i < 15; ++i) {
      if (i > 0) context += ", ";
      context += existing_tags[i];
    }
  }
  
  context += "\n\nSuggest 3-5 relevant tags for this content:";
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
  
  try {
    nlohmann::json response_json = nlohmann::json::parse(response->body);
    
    if (response_json.contains("error")) {
      std::string error_message = "Anthropic API error";
      if (response_json["error"].contains("message")) {
        error_message = response_json["error"]["message"];
      }
      return std::unexpected(makeError(ErrorCode::kNetworkError, error_message));
    }
    
    if (!response_json.contains("content") || !response_json["content"].is_array() || 
        response_json["content"].empty()) {
      return std::unexpected(makeError(ErrorCode::kParseError, 
                                     "Invalid response format from Anthropic API"));
    }
    
    auto content_item = response_json["content"][0];
    if (!content_item.contains("text")) {
      return std::unexpected(makeError(ErrorCode::kParseError, 
                                     "No text content in Anthropic API response"));
    }
    
    std::string ai_response = content_item["text"];
    
    // Parse the JSON array of tags from the AI response
    try {
      nlohmann::json tags_json = nlohmann::json::parse(ai_response);
      
      if (!tags_json.is_array()) {
        return std::unexpected(makeError(ErrorCode::kParseError, 
                                       "AI response is not a JSON array"));
      }
      
      std::vector<std::string> tags;
      for (const auto& tag_json : tags_json) {
        if (tag_json.is_string()) {
          std::string tag = tag_json.get<std::string>();
          // Clean and validate tag
          if (!tag.empty() && tag.length() <= 50) {
            tags.push_back(tag);
          }
        }
      }
      
      return tags;
      
    } catch (const nlohmann::json::parse_error&) {
      return std::unexpected(makeError(ErrorCode::kParseError, 
                                     "Failed to parse AI response as JSON array"));
    }
    
  } catch (const nlohmann::json::parse_error& e) {
    return std::unexpected(makeError(ErrorCode::kParseError, 
                                   "Failed to parse Anthropic API response: " + std::string(e.what())));
  }
}

} // namespace nx::cli