#include "nx/cli/commands/outline_command.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <regex>
#include <nlohmann/json.hpp>

#include "nx/core/note.hpp"
#include "nx/core/metadata.hpp"
#include "nx/util/http_client.hpp"

namespace nx::cli {

OutlineCommand::OutlineCommand(Application& app) : app_(app) {
}

void OutlineCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("topic", topic_, "Topic to generate outline for")->required();
  cmd->add_option("--depth,-d", depth_, "Maximum outline depth (1-5)")
     ->check(CLI::Range(1, 5));
  cmd->add_option("--style,-s", style_, "Outline style")
     ->check(CLI::IsMember({"bullets", "numbered", "tree"}));
  cmd->add_flag("--create,-c", create_note_, "Create a note with the outline");
  cmd->add_option("--title", custom_title_, "Custom title for created note");
}

Result<int> OutlineCommand::execute(const GlobalOptions& options) {
  try {
    // Validate AI configuration
    auto ai_check = validateAiConfig();
    if (!ai_check.has_value()) {
      return std::unexpected(ai_check.error());
    }
    
    if (topic_.empty()) {
      return std::unexpected(makeError(ErrorCode::kInvalidArgument, "Topic cannot be empty"));
    }
    
    // Generate outline
    auto outline_result = generateOutline(topic_);
    if (!outline_result.has_value()) {
      return std::unexpected(outline_result.error());
    }
    
    auto outline = outline_result.value();
    
    if (outline.empty()) {
      if (options.json) {
        nlohmann::json result;
        result["topic"] = topic_;
        result["outline"] = nlohmann::json::array();
        result["message"] = "No outline could be generated for this topic";
        result["success"] = true;
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "No outline could be generated for topic: " << topic_ << std::endl;
      }
      return 0;
    }
    
    // Create note if requested
    std::optional<nx::core::NoteId> created_note_id;
    if (create_note_) {
      auto create_result = createNoteWithOutline(outline);
      if (!create_result.has_value()) {
        return std::unexpected(create_result.error());
      }
      created_note_id = create_result.value();
      
      if (!options.quiet) {
        std::cout << "Created note with outline: " << created_note_id->toString() << std::endl;
      }
    }
    
    // Output results
    outputResult(outline, created_note_id, options);
    
    return 0;
    
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kUnknownError, 
                                     "Failed to execute outline command: " + std::string(e.what())));
  }
}

Result<void> OutlineCommand::validateAiConfig() {
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

Result<std::vector<OutlineCommand::OutlineNode>> OutlineCommand::generateOutline(const std::string& topic) {
  const auto& ai_config = app_.config().ai.value();
  
  if (ai_config.provider != "anthropic") {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                   "Only Anthropic provider is currently supported"));
  }
  
  // Prepare the request payload for Anthropic API
  nlohmann::json request_body;
  request_body["model"] = ai_config.model;
  request_body["max_tokens"] = 1536;
  
  std::string system_prompt = "You are a helpful assistant that creates detailed, hierarchical outlines for any topic. "
                             "Create a comprehensive outline for the given topic with up to " + std::to_string(depth_) + " levels of depth. "
                             "Return a JSON array where each item has: \"title\", \"level\" (1-" + std::to_string(depth_) + "), \"children\" (array of sub-items). "
                             "Structure the outline logically with main topics, subtopics, and supporting details.";
  
  request_body["system"] = system_prompt;
  
  nlohmann::json messages = nlohmann::json::array();
  nlohmann::json user_message;
  user_message["role"] = "user";
  user_message["content"] = "Create a detailed outline for the topic: \"" + topic + 
                           "\". Include " + std::to_string(depth_) + " levels of hierarchy.";
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
    std::vector<OutlineNode> outline;
    try {
      auto outline_json = nlohmann::json::parse(ai_response);
      if (!outline_json.is_array()) {
        return std::unexpected(makeError(ErrorCode::kParseError, 
                                       "AI response is not a JSON array"));
      }
      
      outline = parseOutlineFromJson(outline_json);
      
    } catch (const nlohmann::json::exception&) {
      // Fallback: try to extract outline from free text response
      outline = parseOutlineFromText(ai_response);
    }
    
    return outline;
    
  } catch (const nlohmann::json::exception& e) {
    return std::unexpected(makeError(ErrorCode::kParseError, 
                                   "Failed to parse Anthropic API response: " + std::string(e.what())));
  }
}

std::vector<OutlineCommand::OutlineNode> OutlineCommand::parseOutlineFromJson(const nlohmann::json& outline_json) {
  std::vector<OutlineNode> outline;
  
  for (const auto& item : outline_json) {
    if (!item.is_object()) continue;
    
    OutlineNode node;
    if (item.contains("title") && item["title"].is_string()) {
      node.text = item["title"].get<std::string>();
    }
    if (item.contains("level") && item["level"].is_number()) {
      node.level = item["level"].get<int>();
    } else {
      node.level = 1;  // Default level
    }
    
    // Parse children recursively
    if (item.contains("children") && item["children"].is_array()) {
      node.children = parseOutlineFromJson(item["children"]);
    }
    
    if (!node.text.empty()) {
      outline.push_back(node);
    }
  }
  
  return outline;
}

std::vector<OutlineCommand::OutlineNode> OutlineCommand::parseOutlineFromText(const std::string& text) {
  std::vector<OutlineNode> outline;
  
  std::istringstream stream(text);
  std::string line;
  
  while (std::getline(stream, line)) {
    // Skip empty lines
    if (line.empty()) continue;
    
    OutlineNode node;
    node.level = 1;  // Default level
    
    // Determine level based on indentation or prefixes
    size_t indent = 0;
    while (indent < line.length() && (line[indent] == ' ' || line[indent] == '\t')) {
      indent++;
    }
    
    if (indent > 0) {
      node.level = std::min(static_cast<int>(indent / 2) + 1, depth_);
    }
    
    // Remove common prefixes
    std::string clean_line = line.substr(indent);
    if (clean_line.find("- ") == 0) {
      clean_line = clean_line.substr(2);
    } else if (clean_line.find("• ") == 0) {
      clean_line = clean_line.substr(2);
    } else if (std::regex_match(clean_line, std::regex("^\\d+\\. .*"))) {
      size_t dot_pos = clean_line.find(". ");
      if (dot_pos != std::string::npos) {
        clean_line = clean_line.substr(dot_pos + 2);
      }
    }
    
    // Trim whitespace
    clean_line.erase(0, clean_line.find_first_not_of(" \t"));
    clean_line.erase(clean_line.find_last_not_of(" \t") + 1);
    
    if (!clean_line.empty() && clean_line.length() > 2) {
      node.text = clean_line;
      outline.push_back(node);
    }
  }
  
  return outline;
}

Result<nx::core::NoteId> OutlineCommand::createNoteWithOutline(const std::vector<OutlineNode>& outline) {
  // Generate new note ID
  auto note_id = nx::core::NoteId::generate();
  
  // Determine title
  std::string title = custom_title_.empty() ? topic_ + " - Outline" : custom_title_;
  
  // Create metadata
  nx::core::Metadata metadata(note_id, title);
  metadata.addTag("outline");
  metadata.addTag("ai-generated");
  
  // Format outline content
  std::string content = formatOutline(outline, style_);
  
  // Add header information
  std::string full_content = "# " + title + "\n\n";
  full_content += "*AI-generated outline for: " + topic_ + "*\n\n";
  full_content += content;
  full_content += "\n\n---\n*Generated using " + app_.config().ai->provider + " AI*";
  
  // Create note
  nx::core::Note note(std::move(metadata), full_content);
  
  // Store the note
  auto store_result = app_.noteStore().store(note);
  if (!store_result.has_value()) {
    return std::unexpected(store_result.error());
  }
  
  // Add to search index
  auto index_result = app_.searchIndex().addNote(note);
  if (!index_result.has_value()) {
    // Non-fatal - warn but continue
    std::cerr << "Warning: Failed to add note to search index: " 
              << index_result.error().message() << std::endl;
  }
  
  return note_id;
}

std::string OutlineCommand::formatOutline(const std::vector<OutlineNode>& outline, const std::string& style) {
  std::string result;
  
  for (size_t i = 0; i < outline.size(); ++i) {
    std::string prefix;
    if (style == "numbered") {
      prefix = std::to_string(i + 1) + ". ";
    } else if (style == "bullets") {
      prefix = "- ";
    } else { // tree
      prefix = "• ";
    }
    
    result += formatOutlineNode(outline[i], style, prefix);
  }
  
  return result;
}

std::string OutlineCommand::formatOutlineNode(const OutlineNode& node, const std::string& style, const std::string& prefix) {
  std::string result;
  std::string indent(std::max(0, (node.level - 1) * 2), ' ');
  
  result += indent + prefix + node.text + "\n";
  
  // Format children
  for (size_t i = 0; i < node.children.size(); ++i) {
    std::string child_prefix;
    if (style == "numbered") {
      if (node.level == 1) {
        child_prefix = std::string(1, 'a' + i) + ". ";
      } else {
        child_prefix = std::to_string(i + 1) + ". ";
      }
    } else if (style == "bullets") {
      child_prefix = node.level == 1 ? "  - " : "    - ";
    } else { // tree
      child_prefix = node.level == 1 ? "  ○ " : "    ▪ ";
    }
    
    result += formatOutlineNode(node.children[i], style, child_prefix);
  }
  
  return result;
}

void OutlineCommand::outputResult(const std::vector<OutlineNode>& outline, 
                                 const std::optional<nx::core::NoteId>& created_note_id,
                                 const GlobalOptions& options) {
  if (options.json) {
    nlohmann::json result;
    result["topic"] = topic_;
    result["style"] = style_;
    result["depth"] = depth_;
    
    // Convert outline to JSON structure
    std::function<nlohmann::json(const OutlineNode&)> node_to_json = [&](const OutlineNode& node) -> nlohmann::json {
      nlohmann::json node_json;
      node_json["text"] = node.text;
      node_json["level"] = node.level;
      
      if (!node.children.empty()) {
        nlohmann::json children_json = nlohmann::json::array();
        for (const auto& child : node.children) {
          children_json.push_back(node_to_json(child));
        }
        node_json["children"] = children_json;
      }
      
      return node_json;
    };
    
    nlohmann::json outline_json = nlohmann::json::array();
    for (const auto& node : outline) {
      outline_json.push_back(node_to_json(node));
    }
    result["outline"] = outline_json;
    
    if (created_note_id.has_value()) {
      result["created_note_id"] = created_note_id->toString();
    }
    
    result["success"] = true;
    std::cout << result.dump(2) << std::endl;
  } else {
    if (!create_note_) {
      std::cout << "AI-Generated Outline for: " << topic_ << std::endl;
      std::cout << std::string(topic_.length() + 25, '=') << std::endl;
      std::cout << std::endl;
      
      std::cout << formatOutline(outline, style_) << std::endl;
      
      std::cout << "To create a note with this outline, use: nx outline \"" << topic_ << "\" --create" << std::endl;
    }
  }
}

} // namespace nx::cli