#include "nx/cli/commands/rewrite_command.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

#include "nx/core/note_id.hpp"
#include "nx/util/http_client.hpp"
#include "nx/util/security.hpp"

namespace nx::cli {

RewriteCommand::RewriteCommand(Application& app) : app_(app) {
}

void RewriteCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("note_id", note_id_, "ID of the note to rewrite")->required();
  cmd->add_option("--tone,-t", tone_, "Writing tone to use")
     ->check(CLI::IsMember({"crisp", "neutral", "professional", "casual", "academic", "conversational"}));
  cmd->add_flag("--apply", apply_, "Apply the rewritten content to the note");
  cmd->add_flag("--dry-run", dry_run_, "Show what would be done without making changes");
}

Result<int> RewriteCommand::execute(const GlobalOptions& options) {
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
    std::string original_content = note.content();
    
    if (original_content.empty() || original_content == "# " + note.title() + "\n\n") {
      if (options.json) {
        nlohmann::json result;
        result["note_id"] = note_id_;
        result["error"] = "Note content is empty or contains only title";
        result["success"] = false;
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "Note content is empty or contains only a title. Nothing to rewrite." << std::endl;
      }
      return 0;
    }
    
    // Generate rewritten content
    auto rewrite_result = rewriteContent(note);
    if (!rewrite_result.has_value()) {
      return std::unexpected(rewrite_result.error());
    }
    
    auto rewritten_content = rewrite_result.value();
    
    // Handle dry-run
    if (dry_run_) {
      if (options.json) {
        nlohmann::json result;
        result["note_id"] = note_id_;
        result["tone"] = tone_;
        result["original_content"] = original_content;
        result["rewritten_content"] = rewritten_content;
        result["would_apply"] = apply_;
        result["dry_run"] = true;
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "Dry run mode - no changes will be made." << std::endl;
        std::cout << std::endl;
        std::cout << "Note: " << note.title() << " (" << note_id_ << ")" << std::endl;
        std::cout << "Tone: " << tone_ << std::endl;
        std::cout << std::endl;
        std::cout << "=== ORIGINAL CONTENT ===" << std::endl;
        std::cout << original_content << std::endl;
        std::cout << "=== REWRITTEN CONTENT ===" << std::endl;
        std::cout << rewritten_content << std::endl;
        
        if (apply_) {
          std::cout << std::endl;
          std::cout << "Would replace note content with rewritten version." << std::endl;
        }
      }
      return 0;
    }
    
    // Apply rewrite if requested
    if (apply_) {
      auto apply_result = applyRewrite(rewritten_content);
      if (!apply_result.has_value()) {
        return std::unexpected(apply_result.error());
      }
      
      if (!options.quiet) {
        std::cout << "Note content rewritten with " << tone_ << " tone." << std::endl;
      }
    }
    
    // Output result
    outputResult(original_content, rewritten_content, options);
    
    return 0;
    
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kUnknownError, 
                                     "Failed to execute rewrite command: " + std::string(e.what())));
  }
}

Result<void> RewriteCommand::validateAiConfig() {
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
  
  // Validate API key format
  if (!nx::util::Security::validateApiKeyFormat(ai_config.api_key, ai_config.provider)) {
    return std::unexpected(makeError(ErrorCode::kConfigError,
                                     "Invalid API key format for provider: " + ai_config.provider));
  }
  
  return {};
}

Result<nx::core::Note> RewriteCommand::loadNote() {
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

Result<std::string> RewriteCommand::rewriteContent(const nx::core::Note& note) {
  const auto& ai_config = app_.config().ai.value();
  
  if (ai_config.provider != "anthropic") {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                   "Only Anthropic provider is currently supported"));
  }
  
  // Prepare the request payload for Anthropic API
  nlohmann::json request_body;
  request_body["model"] = ai_config.model;
  request_body["max_tokens"] = 2048;
  
  std::string system_prompt = "You are a skilled editor that rewrites content in different tones while preserving the original meaning, structure, and any markdown formatting. ";
  
  if (tone_ == "crisp") {
    system_prompt += "Rewrite the content to be crisp and concise: use direct language, eliminate unnecessary words, prefer active voice, and make every sentence clear and to the point.";
  } else if (tone_ == "professional") {
    system_prompt += "Rewrite the content in a professional business tone: use formal language, maintain clarity, ensure appropriate structure, and adopt a respectful, diplomatic style.";
  } else if (tone_ == "casual") {
    system_prompt += "Rewrite the content in a casual, friendly tone: use conversational language, make it approachable and relaxed, while keeping it clear and engaging.";
  } else if (tone_ == "academic") {
    system_prompt += "Rewrite the content in an academic tone: use scholarly language, maintain objectivity, include precise terminology, and adopt a formal analytical style.";
  } else if (tone_ == "conversational") {
    system_prompt += "Rewrite the content in a conversational tone: make it feel like a natural dialogue, use accessible language, and maintain an engaging, personal style.";
  } else {
    system_prompt += "Rewrite the content in a neutral, balanced tone: maintain clarity and readability while being neither too formal nor too casual.";
  }
  
  system_prompt += " Preserve all markdown formatting, code blocks, links, and structural elements. Return only the rewritten content, no explanations or meta-text.";
  
  request_body["system"] = system_prompt;
  
  nlohmann::json messages = nlohmann::json::array();
  nlohmann::json user_message;
  user_message["role"] = "user";
  user_message["content"] = "Please rewrite this content:\n\n" + note.content();
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
    
    return content_obj["text"].get<std::string>();
    
  } catch (const nlohmann::json::exception& e) {
    return std::unexpected(makeError(ErrorCode::kParseError, 
                                   "Failed to parse Anthropic API response: " + std::string(e.what())));
  }
}

Result<void> RewriteCommand::applyRewrite(const std::string& rewritten_content) {
  // Load the current note
  auto note_result = loadNote();
  if (!note_result.has_value()) {
    return std::unexpected(note_result.error());
  }
  
  auto note = note_result.value();
  
  // Update note with rewritten content
  auto updated_metadata = note.metadata();
  updated_metadata.touch();  // Update modified time
  
  // Create updated note with rewritten content
  nx::core::Note updated_note(std::move(updated_metadata), rewritten_content);
  
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

void RewriteCommand::outputResult(const std::string& original_content, 
                                 const std::string& rewritten_content, 
                                 const GlobalOptions& options) {
  if (options.json) {
    nlohmann::json result;
    result["note_id"] = note_id_;
    result["tone"] = tone_;
    result["original_content"] = original_content;
    result["rewritten_content"] = rewritten_content;
    result["applied"] = apply_;
    result["success"] = true;
    
    std::cout << result.dump(2) << std::endl;
  } else {
    if (!apply_) {
      std::cout << "Rewritten content (" << tone_ << " tone):" << std::endl;
      std::cout << "==================================" << std::endl;
      std::cout << rewritten_content << std::endl;
      std::cout << "==================================" << std::endl;
      std::cout << std::endl;
      std::cout << "To apply this rewrite, use: nx rewrite " << note_id_ << " --tone " << tone_ << " --apply" << std::endl;
      
      if (options.verbose) {
        std::cout << std::endl;
        std::cout << "Original content length: " << original_content.length() << " characters" << std::endl;
        std::cout << "Rewritten content length: " << rewritten_content.length() << " characters" << std::endl;
      }
    }
  }
}

} // namespace nx::cli