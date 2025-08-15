#include "nx/cli/commands/tasks_command.hpp"

#include <iostream>
#include <sstream>
#include <algorithm>
#include <nlohmann/json.hpp>

#include "nx/core/note_id.hpp"
#include "nx/util/http_client.hpp"

namespace nx::cli {

TasksCommand::TasksCommand(Application& app) : app_(app) {
}

void TasksCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("note_id", note_id_, "ID of the note to extract tasks from")->required();
  cmd->add_option("--priority,-p", priority_filter_, "Filter by priority level (high, medium, low)");
  cmd->add_flag("--context,-c", include_context_, "Include surrounding context for each task");
}

Result<int> TasksCommand::execute(const GlobalOptions& options) {
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
    
    if (note.content().empty() || note.content() == "# " + note.title() + "\n\n") {
      if (options.json) {
        nlohmann::json result;
        result["note_id"] = note_id_;
        result["tasks"] = nlohmann::json::array();
        result["message"] = "Note content is empty";
        result["success"] = true;
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "Note content is empty. No tasks to extract." << std::endl;
      }
      return 0;
    }
    
    // Extract tasks from the note
    auto tasks_result = extractTasks(note);
    if (!tasks_result.has_value()) {
      return std::unexpected(tasks_result.error());
    }
    
    auto tasks = tasks_result.value();
    
    // Apply priority filter if specified
    if (!priority_filter_.empty()) {
      tasks.erase(std::remove_if(tasks.begin(), tasks.end(), 
                                [this](const Task& task) {
                                  return task.priority != priority_filter_;
                                }), tasks.end());
    }
    
    if (tasks.empty()) {
      if (options.json) {
        nlohmann::json result;
        result["note_id"] = note_id_;
        result["tasks"] = nlohmann::json::array();
        result["message"] = priority_filter_.empty() ? "No tasks found in note" : 
                           "No tasks found with priority: " + priority_filter_;
        result["success"] = true;
        std::cout << result.dump(2) << std::endl;
      } else {
        if (priority_filter_.empty()) {
          std::cout << "No actionable tasks found in this note." << std::endl;
        } else {
          std::cout << "No tasks found with priority: " << priority_filter_ << std::endl;
        }
      }
      return 0;
    }
    
    // Output results
    outputResult(tasks, options);
    
    return 0;
    
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kUnknownError, 
                                     "Failed to execute tasks command: " + std::string(e.what())));
  }
}

Result<void> TasksCommand::validateAiConfig() {
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

Result<nx::core::Note> TasksCommand::loadNote() {
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

Result<std::vector<TasksCommand::Task>> TasksCommand::extractTasks(const nx::core::Note& note) {
  const auto& ai_config = app_.config().ai.value();
  
  if (ai_config.provider != "anthropic") {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                   "Only Anthropic provider is currently supported"));
  }
  
  // Prepare the request payload for Anthropic API
  nlohmann::json request_body;
  request_body["model"] = ai_config.model;
  request_body["max_tokens"] = 1024;
  
  std::string system_prompt = "You are a helpful assistant that extracts actionable tasks from note content. "
                             "Identify specific action items, TODO items, commitments, and things that need to be done. "
                             "For each task, determine the priority (high, medium, low) based on urgency and importance. "
                             "Return a JSON array where each task has: \"description\", \"priority\", \"context\" (surrounding text). "
                             "Only extract genuine action items, not general concepts or ideas.";
  
  request_body["system"] = system_prompt;
  
  nlohmann::json messages = nlohmann::json::array();
  nlohmann::json user_message;
  user_message["role"] = "user";
  user_message["content"] = "Extract actionable tasks from this note:\n\nTitle: " + note.title() + 
                           "\n\nContent:\n" + note.content();
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
    std::vector<Task> tasks;
    try {
      auto tasks_json = nlohmann::json::parse(ai_response);
      if (!tasks_json.is_array()) {
        return std::unexpected(makeError(ErrorCode::kParseError, 
                                       "AI response is not a JSON array"));
      }
      
      for (const auto& task_json : tasks_json) {
        if (!task_json.is_object()) continue;
        
        Task task;
        if (task_json.contains("description") && task_json["description"].is_string()) {
          task.description = task_json["description"].get<std::string>();
        }
        if (task_json.contains("priority") && task_json["priority"].is_string()) {
          task.priority = task_json["priority"].get<std::string>();
        }
        if (task_json.contains("context") && task_json["context"].is_string()) {
          task.context = task_json["context"].get<std::string>();
        }
        
        // Validate priority values
        if (task.priority != "high" && task.priority != "medium" && task.priority != "low") {
          task.priority = "medium";  // Default
        }
        
        if (!task.description.empty()) {
          tasks.push_back(task);
        }
      }
      
    } catch (const nlohmann::json::exception&) {
      // Fallback: try to extract tasks from free text response
      std::istringstream stream(ai_response);
      std::string line;
      while (std::getline(stream, line)) {
        // Look for task-like patterns in the response
        if (line.find("- ") != std::string::npos || 
            line.find("TODO") != std::string::npos ||
            line.find("Action:") != std::string::npos) {
          
          Task task;
          task.description = line;
          task.priority = "medium";  // Default priority
          task.context = "";  // No context in fallback
          
          // Clean up the description
          size_t pos = task.description.find("- ");
          if (pos != std::string::npos) {
            task.description = task.description.substr(pos + 2);
          }
          
          // Trim whitespace
          task.description.erase(0, task.description.find_first_not_of(" \t"));
          task.description.erase(task.description.find_last_not_of(" \t") + 1);
          
          if (!task.description.empty()) {
            tasks.push_back(task);
          }
        }
      }
    }
    
    return tasks;
    
  } catch (const nlohmann::json::exception& e) {
    return std::unexpected(makeError(ErrorCode::kParseError, 
                                   "Failed to parse Anthropic API response: " + std::string(e.what())));
  }
}

void TasksCommand::outputResult(const std::vector<Task>& tasks, const GlobalOptions& options) {
  if (options.json) {
    nlohmann::json result;
    result["note_id"] = note_id_;
    result["total_tasks"] = tasks.size();
    
    nlohmann::json tasks_json = nlohmann::json::array();
    for (const auto& task : tasks) {
      nlohmann::json task_json;
      task_json["description"] = task.description;
      task_json["priority"] = task.priority;
      task_json["line_number"] = task.line_number;
      if (include_context_ && !task.context.empty()) {
        task_json["context"] = task.context;
      }
      tasks_json.push_back(task_json);
    }
    result["tasks"] = tasks_json;
    
    // Add priority breakdown
    nlohmann::json priority_count;
    priority_count["high"] = std::count_if(tasks.begin(), tasks.end(), 
                                          [](const Task& t) { return t.priority == "high"; });
    priority_count["medium"] = std::count_if(tasks.begin(), tasks.end(), 
                                            [](const Task& t) { return t.priority == "medium"; });
    priority_count["low"] = std::count_if(tasks.begin(), tasks.end(), 
                                         [](const Task& t) { return t.priority == "low"; });
    result["priority_breakdown"] = priority_count;
    
    result["success"] = true;
    std::cout << result.dump(2) << std::endl;
  } else {
    std::cout << "Action items extracted from note " << note_id_ << ":" << std::endl;
    std::cout << std::endl;
    
    // Group by priority
    std::vector<std::string> priorities = {"high", "medium", "low"};
    for (const auto& priority : priorities) {
      std::vector<Task> priority_tasks;
      std::copy_if(tasks.begin(), tasks.end(), std::back_inserter(priority_tasks),
                   [&priority](const Task& t) { return t.priority == priority; });
      
      if (!priority_tasks.empty()) {
        std::string priority_upper = priority;
        std::transform(priority_upper.begin(), priority_upper.end(), 
                      priority_upper.begin(), ::toupper);
        std::cout << priority_upper << " PRIORITY (" << priority_tasks.size() << "):" << std::endl;
        
        for (size_t i = 0; i < priority_tasks.size(); ++i) {
          const auto& task = priority_tasks[i];
          std::cout << "  " << (i + 1) << ". " << task.description;
          
          if (task.line_number > 0) {
            std::cout << " (line " << task.line_number << ")";
          }
          std::cout << std::endl;
          
          if (include_context_ && !task.context.empty()) {
            std::cout << "     Context: " << task.context << std::endl;
          }
        }
        std::cout << std::endl;
      }
    }
    
    if (options.verbose) {
      int high_count = std::count_if(tasks.begin(), tasks.end(), 
                                    [](const Task& t) { return t.priority == "high"; });
      int medium_count = std::count_if(tasks.begin(), tasks.end(), 
                                      [](const Task& t) { return t.priority == "medium"; });
      int low_count = std::count_if(tasks.begin(), tasks.end(), 
                                   [](const Task& t) { return t.priority == "low"; });
      
      std::cout << "Summary: " << tasks.size() << " tasks total ";
      std::cout << "(" << high_count << " high, " << medium_count << " medium, " << low_count << " low)" << std::endl;
    }
  }
}

} // namespace nx::cli