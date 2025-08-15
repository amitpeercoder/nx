#include "nx/cli/commands/summarize_command.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

#include "nx/core/note_id.hpp"
#include "nx/util/http_client.hpp"

namespace nx::cli {

SummarizeCommand::SummarizeCommand(Application& app) : app_(app) {
}

void SummarizeCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("note_id", note_id_, "ID of the note to summarize")->required();
  cmd->add_option("--style,-s", style_, "Summary style (bullets, exec)")
     ->check(CLI::IsMember({"bullets", "exec"}));
  cmd->add_flag("--apply", apply_, "Apply the summary to the note (update content)");
  cmd->add_flag("--dry-run", dry_run_, "Show what would be done without making changes");
}

Result<int> SummarizeCommand::execute(const GlobalOptions& options) {
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
    
    // Generate summary
    auto summary_result = generateSummary(note);
    if (!summary_result.has_value()) {
      return std::unexpected(summary_result.error());
    }
    
    auto summary = summary_result.value();
    
    // Handle dry-run
    if (dry_run_) {
      if (options.json) {
        nlohmann::json result;
        result["note_id"] = note_id_;
        result["original_content"] = note.content();
        result["summary"] = summary;
        result["style"] = style_;
        result["would_apply"] = apply_;
        result["dry_run"] = true;
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "Dry run mode - no changes will be made." << std::endl;
        std::cout << std::endl;
        std::cout << "Note: " << note.title() << " (" << note_id_ << ")" << std::endl;
        std::cout << "Summary (" << style_ << " style):" << std::endl;
        std::cout << summary << std::endl;
        
        if (apply_) {
          std::cout << std::endl;
          std::cout << "Would update note content with this summary." << std::endl;
        }
      }
      return 0;
    }
    
    // Apply summary if requested
    if (apply_) {
      auto apply_result = applySummary(summary);
      if (!apply_result.has_value()) {
        return std::unexpected(apply_result.error());
      }
      
      if (!options.quiet) {
        std::cout << "Note updated with AI-generated summary." << std::endl;
      }
    }
    
    // Output result
    outputResult(summary, options);
    
    return 0;
    
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kUnknownError, 
                                     "Failed to execute summarize command: " + std::string(e.what())));
  }
}

Result<void> SummarizeCommand::validateAiConfig() {
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

Result<nx::core::Note> SummarizeCommand::loadNote() {
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

Result<std::string> SummarizeCommand::generateSummary(const nx::core::Note& note) {
  const auto& ai_config = app_.config().ai.value();
  
  if (ai_config.provider != "anthropic") {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                   "Only Anthropic provider is currently supported"));
  }
  
  // Prepare the request payload for Anthropic API
  nlohmann::json request_body;
  request_body["model"] = ai_config.model;
  request_body["max_tokens"] = 1024;
  
  // Prepare the prompt based on style
  std::string system_prompt;
  if (style_ == "bullets") {
    system_prompt = "You are a helpful assistant that creates concise bullet-point summaries. "
                   "Summarize the given note content in 3-5 bullet points, capturing the key information and insights.";
  } else if (style_ == "exec") {
    system_prompt = "You are a professional assistant that creates executive summaries. "
                   "Provide a high-level overview focusing on key insights, main arguments, and actionable takeaways.";
  } else {
    system_prompt = "You are a helpful assistant that creates clear, concise summaries of note content.";
  }
  
  request_body["system"] = system_prompt;
  
  nlohmann::json messages = nlohmann::json::array();
  nlohmann::json user_message;
  user_message["role"] = "user";
  user_message["content"] = "Please summarize this note:\n\nTitle: " + note.title() + "\n\nContent:\n" + note.content();
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
    
    auto& content = response_json["content"][0];
    if (!content.contains("text") || !content["text"].is_string()) {
      return std::unexpected(makeError(ErrorCode::kParseError, 
                                     "Missing text content in Anthropic API response"));
    }
    
    return content["text"].get<std::string>();
    
  } catch (const nlohmann::json::exception& e) {
    return std::unexpected(makeError(ErrorCode::kParseError, 
                                   "Failed to parse Anthropic API response: " + std::string(e.what())));
  }
}

Result<void> SummarizeCommand::applySummary(const std::string& summary) {
  // Load the current note
  auto note_result = loadNote();
  if (!note_result.has_value()) {
    return std::unexpected(note_result.error());
  }
  
  auto note = note_result.value();
  
  // Update note content with summary
  // Replace the entire content - in practice, you might want to:
  // - Append the summary
  // - Insert it at the beginning
  // - Replace only the content after the title
  // This behavior could be configurable
  
  std::string new_content = "# " + note.title() + "\n\n";
  new_content += "## AI Summary\n\n";
  new_content += summary;
  new_content += "\n\n## Original Content\n\n";
  new_content += note.content();
  
  // Create updated note
  auto updated_metadata = note.metadata();
  updated_metadata.touch();  // Update modified time
  
  nx::core::Note updated_note(std::move(updated_metadata), new_content);
  
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

void SummarizeCommand::outputResult(const std::string& summary, const GlobalOptions& options) {
  if (options.json) {
    nlohmann::json result;
    result["note_id"] = note_id_;
    result["summary"] = summary;
    result["style"] = style_;
    result["applied"] = apply_;
    result["success"] = true;
    
    std::cout << result.dump(2) << std::endl;
  } else {
    if (!apply_) {
      std::cout << "Summary (" << style_ << " style):" << std::endl;
      std::cout << summary << std::endl;
      std::cout << std::endl;
      std::cout << "To apply this summary to the note, use: nx summarize " << note_id_ << " --apply" << std::endl;
    }
  }
}

} // namespace nx::cli