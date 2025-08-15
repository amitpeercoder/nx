#include "nx/cli/commands/ask_command.hpp"

#include <iostream>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <nlohmann/json.hpp>

#include "nx/core/note.hpp"
#include "nx/index/query_parser.hpp"
#include "nx/util/http_client.hpp"

namespace nx::cli {

AskCommand::AskCommand(Application& app) : app_(app) {
}

void AskCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("query", query_, "Question to ask about your notes")->required();
  cmd->add_option("--tag,-t", tags_, "Filter by tags (can be used multiple times)");
  cmd->add_option("--notebook,--nb", notebook_, "Filter by notebook");
  cmd->add_option("--since", since_, "Filter notes since date (YYYY-MM-DD)");
  cmd->add_option("--limit,-l", limit_, "Maximum number of relevant notes to include as context")
     ->check(CLI::Range(1, 20));
}

Result<int> AskCommand::execute(const GlobalOptions& options) {
  try {
    // Validate AI configuration
    auto ai_check = validateAiConfig();
    if (!ai_check.has_value()) {
      return std::unexpected(ai_check.error());
    }
    
    if (query_.empty()) {
      return std::unexpected(makeError(ErrorCode::kInvalidArgument, "Query cannot be empty"));
    }
    
    // Find relevant notes using search
    auto notes_result = findRelevantNotes();
    if (!notes_result.has_value()) {
      return std::unexpected(notes_result.error());
    }
    
    auto& relevant_notes = notes_result.value();
    
    if (relevant_notes.empty()) {
      if (options.json) {
        nlohmann::json result;
        result["query"] = query_;
        result["answer"] = "No relevant notes found for your query.";
        result["sources"] = nlohmann::json::array();
        result["success"] = true;
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "No relevant notes found for your query." << std::endl;
      }
      return 0;
    }
    
    // Build context from relevant notes
    auto context_result = buildContextFromNotes(relevant_notes);
    if (!context_result.has_value()) {
      return std::unexpected(context_result.error());
    }
    
    // Call AI API
    auto answer_result = callAiApi(context_result.value(), query_);
    if (!answer_result.has_value()) {
      return std::unexpected(answer_result.error());
    }
    
    // Output result
    outputResult(answer_result.value(), relevant_notes, options);
    
    return 0;
    
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kUnknownError, 
                                     "Failed to execute ask command: " + std::string(e.what())));
  }
}

Result<void> AskCommand::validateAiConfig() {
  const auto& config = app_.config();
  
  if (!config.ai.has_value()) {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                     "AI is not configured. Please configure AI settings in your config file."));
  }
  
  const auto& ai_config = config.ai.value();
  
  if (ai_config.provider.empty()) {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                     "AI provider not specified in configuration"));
  }
  
  if (ai_config.model.empty()) {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                     "AI model not specified in configuration"));
  }
  
  if (ai_config.api_key.empty()) {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                     "AI API key not specified in configuration"));
  }
  
  // Validate provider
  if (ai_config.provider != "openai" && ai_config.provider != "anthropic") {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                     "Unsupported AI provider: " + ai_config.provider + 
                                     ". Supported providers: openai, anthropic"));
  }
  
  return {};
}

Result<std::vector<nx::core::Note>> AskCommand::findRelevantNotes() {
  // Build search query based on filters
  std::string search_query = query_;
  
  // Create search query with filters
  nx::index::SearchQuery parsed_query;
  parsed_query.text = query_;
  parsed_query.tags = tags_;
  parsed_query.notebook = notebook_.empty() ? std::nullopt : std::optional<std::string>(notebook_);
  
  // Parse since date if provided
  if (!since_.empty()) {
    try {
      // Simple date parsing - support formats like "2024-01-01", "1 week ago", "yesterday"
      if (since_ == "yesterday") {
        auto yesterday = std::chrono::system_clock::now() - std::chrono::hours(24);
        parsed_query.since = yesterday;
      } else if (since_ == "week" || since_ == "1 week ago") {
        auto week_ago = std::chrono::system_clock::now() - std::chrono::hours(24 * 7);
        parsed_query.since = week_ago;
      } else if (since_ == "month" || since_ == "1 month ago") {
        auto month_ago = std::chrono::system_clock::now() - std::chrono::hours(24 * 30);
        parsed_query.since = month_ago;
      } else if (since_.find("-") != std::string::npos) {
        // Try to parse ISO date format (YYYY-MM-DD)
        std::istringstream ss(since_);
        std::tm tm = {};
        ss >> std::get_time(&tm, "%Y-%m-%d");
        if (!ss.fail()) {
          auto time_t = std::mktime(&tm);
          parsed_query.since = std::chrono::system_clock::from_time_t(time_t);
        } else {
          // Fall back to text search
          parsed_query.text += " since:" + since_;
        }
      } else {
        // Fall back to text search
        parsed_query.text += " since:" + since_;
      }
    } catch (...) {
      // Fall back to text search if parsing fails
      parsed_query.text += " since:" + since_;
    }
  }
  
  // Search for relevant notes
  // Set limit in query
  parsed_query.limit = static_cast<size_t>(limit_);
  auto search_result = app_.searchIndex().search(parsed_query);
  if (!search_result.has_value()) {
    return std::unexpected(search_result.error());
  }
  
  std::vector<nx::core::Note> notes;
  
  // Load the actual note content
  for (const auto& search_hit : search_result.value()) {
    auto note_result = app_.noteStore().load(search_hit.id);
    if (note_result.has_value()) {
      notes.push_back(note_result.value());
    }
    // Continue on load errors - we still have other notes
  }
  
  return notes;
}

Result<std::string> AskCommand::buildContextFromNotes(const std::vector<nx::core::Note>& notes) {
  std::string context = "Relevant notes from your collection:\n\n";
  
  for (size_t i = 0; i < notes.size(); ++i) {
    const auto& note = notes[i];
    
    context += "Note " + std::to_string(i + 1) + ":\n";
    context += "Title: " + note.title() + "\n";
    context += "ID: " + note.id().toString() + "\n";
    
    // Add tags if present
    const auto& tags = note.metadata().tags();
    if (!tags.empty()) {
      context += "Tags: ";
      for (size_t j = 0; j < tags.size(); ++j) {
        if (j > 0) context += ", ";
        context += tags[j];
      }
      context += "\n";
    }
    
    // Add notebook if present
    if (note.notebook().has_value()) {
      context += "Notebook: " + note.notebook().value() + "\n";
    }
    
    context += "Content:\n" + note.content() + "\n";
    context += "---\n\n";
  }
  
  return context;
}

Result<std::string> AskCommand::callAiApi(const std::string& context, const std::string& query) {
  const auto& ai_config = app_.config().ai.value();
  
  if (ai_config.provider != "anthropic") {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                   "Only Anthropic provider is currently supported"));
  }
  
  // Prepare the request payload for Anthropic API
  nlohmann::json request_body;
  request_body["model"] = ai_config.model;
  request_body["max_tokens"] = 2048;
  
  std::string system_prompt = "You are a knowledgeable assistant helping the user understand their notes. "
                             "Answer questions based on the provided note context. "
                             "Be concise but thorough, and cite specific notes when relevant.";
  
  request_body["system"] = system_prompt;
  
  nlohmann::json messages = nlohmann::json::array();
  nlohmann::json user_message;
  user_message["role"] = "user";
  user_message["content"] = "Here is the context from my notes:\n\n" + context + 
                           "\n\nBased on this context, please answer this question: " + query;
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

void AskCommand::outputResult(const std::string& answer, const std::vector<nx::core::Note>& sources, 
                             const GlobalOptions& options) {
  if (options.json) {
    nlohmann::json result;
    result["query"] = query_;
    result["answer"] = answer;
    
    // Add source information
    nlohmann::json sources_json = nlohmann::json::array();
    for (const auto& note : sources) {
      nlohmann::json source;
      source["id"] = note.id().toString();
      source["title"] = note.title();
      source["tags"] = note.metadata().tags();
      if (note.notebook().has_value()) {
        source["notebook"] = note.notebook().value();
      }
      sources_json.push_back(source);
    }
    result["sources"] = sources_json;
    result["success"] = true;
    
    std::cout << result.dump(2) << std::endl;
  } else {
    std::cout << "Query: " << query_ << std::endl;
    std::cout << std::endl;
    std::cout << "Answer:" << std::endl;
    std::cout << answer << std::endl;
    
    if (options.verbose && !sources.empty()) {
      std::cout << std::endl;
      std::cout << "Sources (" << sources.size() << " notes):" << std::endl;
      for (size_t i = 0; i < sources.size(); ++i) {
        const auto& note = sources[i];
        std::cout << "  " << (i + 1) << ". " << note.title() 
                  << " (" << note.id().toString() << ")";
        
        const auto& tags = note.metadata().tags();
        if (!tags.empty()) {
          std::cout << " [";
          for (size_t j = 0; j < tags.size(); ++j) {
            if (j > 0) std::cout << ", ";
            std::cout << tags[j];
          }
          std::cout << "]";
        }
        std::cout << std::endl;
      }
    }
  }
}

} // namespace nx::cli