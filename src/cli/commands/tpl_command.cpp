#include "nx/cli/commands/tpl_command.hpp"

#include <iostream>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <nlohmann/json.hpp>
#include "nx/util/safe_process.hpp"
#include "nx/util/filesystem.hpp"

namespace nx::cli {

TplCommand::TplCommand(Application& app) : app_(app) {
}

Result<int> TplCommand::execute(const GlobalOptions& options) {
  try {
    if (subcommand_ == "list" || subcommand_ == "ls") {
      return executeList();
    } else if (subcommand_ == "show") {
      return executeShow();
    } else if (subcommand_ == "create" || subcommand_ == "add") {
      return executeCreate();
    } else if (subcommand_ == "edit") {
      return executeEdit();
    } else if (subcommand_ == "delete" || subcommand_ == "rm") {
      return executeDelete();
    } else if (subcommand_ == "use") {
      return executeUse();
    } else if (subcommand_ == "search") {
      return executeSearch();
    } else if (subcommand_ == "install") {
      return executeInstall();
    } else {
      std::cout << "Error: Unknown template subcommand: " << subcommand_ << std::endl;
      std::cout << "Available subcommands: list, show, create, edit, delete, use, search, install" << std::endl;
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

void TplCommand::setupCommand(CLI::App* cmd) {
  // List subcommand
  auto list_cmd = cmd->add_subcommand("list", "List available templates");
  list_cmd->add_option("--category,-c", category_, "Filter by category");
  list_cmd->callback([this]() { subcommand_ = "list"; });
  
  auto ls_cmd = cmd->add_subcommand("ls", "List available templates (alias for list)");
  ls_cmd->add_option("--category,-c", category_, "Filter by category");
  ls_cmd->callback([this]() { subcommand_ = "ls"; });
  
  // Show subcommand
  auto show_cmd = cmd->add_subcommand("show", "Show template details");
  show_cmd->add_option("template", template_name_, "Template name")->required();
  show_cmd->callback([this]() { subcommand_ = "show"; });
  
  // Create subcommand
  auto create_cmd = cmd->add_subcommand("create", "Create new template");
  create_cmd->add_option("name", template_name_, "Template name")->required();
  create_cmd->add_option("--from-file,-f", template_file_, "Create from file");
  create_cmd->add_option("--description,-d", description_, "Template description");
  create_cmd->add_option("--category,-c", category_, "Template category");
  create_cmd->add_flag("--force", force_, "Overwrite existing template");
  create_cmd->callback([this]() { subcommand_ = "create"; });
  
  auto add_cmd = cmd->add_subcommand("add", "Create new template (alias for create)");
  add_cmd->add_option("name", template_name_, "Template name")->required();
  add_cmd->add_option("--from-file,-f", template_file_, "Create from file");
  add_cmd->add_option("--description,-d", description_, "Template description");
  add_cmd->add_option("--category,-c", category_, "Template category");
  add_cmd->add_flag("--force", force_, "Overwrite existing template");
  add_cmd->callback([this]() { subcommand_ = "add"; });
  
  // Edit subcommand
  auto edit_cmd = cmd->add_subcommand("edit", "Edit existing template");
  edit_cmd->add_option("template", template_name_, "Template name")->required();
  edit_cmd->callback([this]() { subcommand_ = "edit"; });
  
  // Delete subcommand
  auto delete_cmd = cmd->add_subcommand("delete", "Delete template");
  delete_cmd->add_option("template", template_name_, "Template name")->required();
  delete_cmd->add_flag("--force", force_, "Delete without confirmation");
  delete_cmd->callback([this]() { subcommand_ = "delete"; });
  
  auto rm_cmd = cmd->add_subcommand("rm", "Delete template (alias for delete)");
  rm_cmd->add_option("template", template_name_, "Template name")->required();
  rm_cmd->add_flag("--force", force_, "Delete without confirmation");
  rm_cmd->callback([this]() { subcommand_ = "rm"; });
  
  // Use subcommand
  auto use_cmd = cmd->add_subcommand("use", "Create note from template");
  use_cmd->add_option("template", template_name_, "Template name")->required();
  use_cmd->add_option("--var,-v", variables_, "Template variables (key=value)");
  use_cmd->callback([this]() { subcommand_ = "use"; });
  
  // Search subcommand
  auto search_cmd = cmd->add_subcommand("search", "Search templates");
  search_cmd->add_option("query", template_name_, "Search query")->required();
  search_cmd->callback([this]() { subcommand_ = "search"; });
  
  // Install subcommand
  auto install_cmd = cmd->add_subcommand("install", "Install built-in templates");
  install_cmd->add_flag("--builtins", install_builtins_, "Install built-in templates");
  install_cmd->callback([this]() { subcommand_ = "install"; });
  
  cmd->require_subcommand(1);
}

Result<int> TplCommand::executeList() {
  auto& template_mgr = app_.templateManager();
  
  auto templates_result = template_mgr.listTemplates(category_);
  if (!templates_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << templates_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error listing templates: " << templates_result.error().message() << std::endl;
    }
    return 1;
  }

  auto templates = templates_result.value();
  if (templates.empty()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"templates": []}))" << std::endl;
    } else {
      std::string category_filter = category_.empty() ? "" : " in category '" + category_ + "'";
      std::cout << "No templates found" << category_filter << std::endl;
      if (category_.empty()) {
        std::cout << "\nTip: Run 'nx tpl install --builtins' to install built-in templates" << std::endl;
      }
    }
    return 0;
  }

  outputTemplateList(templates, app_.globalOptions());
  return 0;
}

Result<int> TplCommand::executeShow() {
  if (template_name_.empty()) {
    return std::unexpected(makeError(ErrorCode::kValidationError, "Template name is required"));
  }

  auto& template_mgr = app_.templateManager();
  
  auto info_result = template_mgr.getTemplateInfo(template_name_);
  if (!info_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << info_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error: " << info_result.error().message() << std::endl;
    }
    return 1;
  }

  auto content_result = template_mgr.getTemplate(template_name_);
  if (!content_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << content_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error: " << content_result.error().message() << std::endl;
    }
    return 1;
  }

  if (app_.globalOptions().json) {
    nlohmann::json output;
    auto info = info_result.value();
    
    output["name"] = info.name;
    output["description"] = info.description;
    output["category"] = info.category;
    output["variables"] = info.variables;
    output["content"] = content_result.value();
    
    std::cout << output.dump(2) << std::endl;
  } else {
    outputTemplateInfo(info_result.value(), app_.globalOptions());
    std::cout << "\n--- Template Content ---\n" << std::endl;
    std::cout << content_result.value() << std::endl;
  }

  return 0;
}

Result<int> TplCommand::executeCreate() {
  if (template_name_.empty()) {
    return std::unexpected(makeError(ErrorCode::kValidationError, "Template name is required"));
  }

  auto& template_mgr = app_.templateManager();
  
  std::string content;
  
  if (!template_file_.empty()) {
    // Read from file
    std::ifstream file(template_file_);
    if (!file.is_open()) {
      if (app_.globalOptions().json) {
        std::cout << R"({"error": "Cannot read file: )" << template_file_ << R"("}))" << std::endl;
      } else {
        std::cout << "Error: Cannot read file: " << template_file_ << std::endl;
      }
      return 1;
    }
    
    content = std::string((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
  } else {
    // Use basic template
    content = R"(# {{title}}

{{content}}

Created: {{date}}
Tags: {{tags}})";
  }

  // Check if template exists and force is not set
  auto existing = template_mgr.getTemplate(template_name_);
  if (existing.has_value() && !force_) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": "Template already exists. Use --force to overwrite."}))" << std::endl;
    } else {
      std::cout << "Error: Template '" << template_name_ << "' already exists. Use --force to overwrite." << std::endl;
    }
    return 1;
  }

  // Delete existing if force is set
  if (existing.has_value() && force_) {
    template_mgr.deleteTemplate(template_name_);
  }

  auto result = template_mgr.createTemplate(template_name_, content, description_, category_);
  if (!result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error creating template: " << result.error().message() << std::endl;
    }
    return 1;
  }

  if (app_.globalOptions().json) {
    nlohmann::json output;
    output["success"] = true;
    output["template"] = template_name_;
    output["action"] = "created";
    std::cout << output.dump(2) << std::endl;
  } else {
    std::cout << "Template '" << template_name_ << "' created successfully!" << std::endl;
    
    // Show variables found in template
    auto variables = template_mgr.extractVariables(content);
    if (!variables.empty()) {
      std::cout << "Variables found: ";
      for (size_t i = 0; i < variables.size(); ++i) {
        std::cout << "{{" << variables[i] << "}}";
        if (i < variables.size() - 1) std::cout << ", ";
      }
      std::cout << std::endl;
    }
  }

  return 0;
}

Result<int> TplCommand::executeEdit() {
  if (template_name_.empty()) {
    return std::unexpected(makeError(ErrorCode::kValidationError, "Template name is required"));
  }

  auto& template_mgr = app_.templateManager();
  
  // Check if template exists
  auto existing_content = template_mgr.getTemplate(template_name_);
  if (!existing_content.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": "Template not found: )" << template_name_ << R"("}))" << std::endl;
    } else {
      std::cout << "Error: Template '" << template_name_ << "' not found." << std::endl;
    }
    return 1;
  }

  // Get editor from config or environment
  std::string editor = app_.config().editor;
  
  if (editor.empty()) {
    const char* visual_env = std::getenv("VISUAL");
    const char* editor_env = std::getenv("EDITOR");
    
    if (visual_env && strlen(visual_env) > 0) {
      editor = visual_env;
    } else if (editor_env && strlen(editor_env) > 0) {
      editor = editor_env;
    } else {
      // Fallback editors in order of preference
      const std::vector<std::string> fallback_editors = {"nano", "micro", "nvim", "vim", "vi"};
      for (const auto& candidate : fallback_editors) {
        if (nx::util::SafeProcess::commandExists(candidate)) {
          editor = candidate;
          break;
        }
      }
      
      // Final fallback
      if (editor.empty()) {
        editor = "vi";
      }
    }
  }

  // Create temporary file with template content
  auto temp_result = nx::util::SecureTempFile::create();
  if (!temp_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": "Failed to create temporary file: )" << temp_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error: Failed to create temporary file: " << temp_result.error().message() << std::endl;
    }
    return 1;
  }
  
  auto temp_file = std::move(*temp_result);
  
  // Write current template content to temporary file
  auto write_result = temp_file.write(*existing_content);
  if (!write_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": "Failed to write template to temporary file: )" << write_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error: Failed to write template to temporary file: " << write_result.error().message() << std::endl;
    }
    return 1;
  }
  
  // Save current terminal state
  auto save_result = nx::util::TerminalControl::saveSettings();
  if (!save_result.has_value() && !app_.globalOptions().quiet) {
    std::cerr << "Warning: Failed to save terminal settings: " << save_result.error().message() << std::endl;
  }
  
  // Launch editor safely
  auto process_result = nx::util::SafeProcess::execute(editor, {temp_file.path().string()});
  
  // Restore terminal state
  auto restore_result = nx::util::TerminalControl::restoreSaneState();
  if (!restore_result.has_value() && !app_.globalOptions().quiet) {
    std::cerr << "Warning: Failed to restore terminal state: " << restore_result.error().message() << std::endl;
  }
  
  if (!process_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": "Failed to launch editor: )" << process_result.error().message() << R"(", "editor": ")" << editor << R"("}))" << std::endl;
    } else {
      std::cout << "Error: Failed to launch editor: " << process_result.error().message() << std::endl;
    }
    return 1;
  }
  
  if (process_result->exit_code != 0) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": "Editor exited with non-zero status", "editor": ")" << editor << R"(", "exit_code": )" << process_result->exit_code << "}" << std::endl;
    } else {
      std::cout << "Warning: Editor exited with status " << process_result->exit_code << std::endl;
      std::cout << "Template not updated." << std::endl;
    }
    return process_result->exit_code;
  }
  
  // Read updated content from temporary file
  auto updated_content_result = temp_file.read();
  if (!updated_content_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": "Failed to read updated template content: )" << updated_content_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error: Failed to read updated template content: " << updated_content_result.error().message() << std::endl;
    }
    return 1;
  }
  
  // Check if content was actually changed
  if (*updated_content_result == *existing_content) {
    if (app_.globalOptions().json) {
      std::cout << R"({"message": "Template content unchanged", "template": ")" << template_name_ << R"("}))" << std::endl;
    } else {
      std::cout << "Template content unchanged." << std::endl;
    }
    return 0;
  }
  
  // Update the template with new content
  auto update_result = template_mgr.updateTemplate(template_name_, *updated_content_result);
  if (!update_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": "Failed to update template: )" << update_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error: Failed to update template: " << update_result.error().message() << std::endl;
    }
    return 1;
  }
  
  // Output success
  if (app_.globalOptions().json) {
    std::cout << R"({"success": true, "template": ")" << template_name_ << R"(", "editor": ")" << editor << R"("}))" << std::endl;
  } else if (!app_.globalOptions().quiet) {
    std::cout << "Template '" << template_name_ << "' updated successfully." << std::endl;
  }
  
  return 0;
}

Result<int> TplCommand::executeDelete() {
  if (template_name_.empty()) {
    return std::unexpected(makeError(ErrorCode::kValidationError, "Template name is required"));
  }

  auto& template_mgr = app_.templateManager();
  
  // Check if template exists
  auto existing = template_mgr.getTemplate(template_name_);
  if (!existing.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": "Template not found: )" << template_name_ << R"("}))" << std::endl;
    } else {
      std::cout << "Error: Template '" << template_name_ << "' not found." << std::endl;
    }
    return 1;
  }

  // Confirmation if not forced
  if (!force_ && !app_.globalOptions().json) {
    std::cout << "Are you sure you want to delete template '" << template_name_ << "'? (y/N): ";
    std::string response;
    std::getline(std::cin, response);
    std::transform(response.begin(), response.end(), response.begin(), ::tolower);
    
    if (response != "y" && response != "yes") {
      std::cout << "Deletion cancelled." << std::endl;
      return 0;
    }
  }

  auto result = template_mgr.deleteTemplate(template_name_);
  if (!result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error deleting template: " << result.error().message() << std::endl;
    }
    return 1;
  }

  if (app_.globalOptions().json) {
    nlohmann::json output;
    output["success"] = true;
    output["template"] = template_name_;
    output["action"] = "deleted";
    std::cout << output.dump(2) << std::endl;
  } else {
    std::cout << "Template '" << template_name_ << "' deleted successfully!" << std::endl;
  }

  return 0;
}

Result<int> TplCommand::executeUse() {
  if (template_name_.empty()) {
    return std::unexpected(makeError(ErrorCode::kValidationError, "Template name is required"));
  }

  auto& template_mgr = app_.templateManager();
  
  // Parse variables
  auto var_map = parseVariables(variables_);
  
  // Process template
  auto result = template_mgr.processTemplate(template_name_, var_map);
  if (!result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error processing template: " << result.error().message() << std::endl;
    }
    return 1;
  }

  // Create note from template
  auto note_result = template_mgr.createNoteFromTemplate(template_name_, var_map);
  if (!note_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << note_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error creating note from template: " << note_result.error().message() << std::endl;
    }
    return 1;
  }

  auto note = note_result.value();
  
  // Store the note
  auto store_result = app_.noteStore().store(note);
  if (!store_result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << store_result.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error storing note: " << store_result.error().message() << std::endl;
    }
    return 1;
  }

  if (app_.globalOptions().json) {
    nlohmann::json output;
    output["success"] = true;
    output["template"] = template_name_;
    output["note_id"] = note.id().toString();
    output["title"] = note.title();
    output["action"] = "created_from_template";
    std::cout << output.dump(2) << std::endl;
  } else {
    std::cout << "Created note from template '" << template_name_ << "'" << std::endl;
    std::cout << "Note ID: " << note.id().toString() << std::endl;
    std::cout << "Title: " << note.title() << std::endl;
    
    if (!note.tags().empty()) {
      std::cout << "Tags: ";
      for (size_t i = 0; i < note.tags().size(); ++i) {
        std::cout << note.tags()[i];
        if (i < note.tags().size() - 1) std::cout << ", ";
      }
      std::cout << std::endl;
    }
  }

  return 0;
}

Result<int> TplCommand::executeSearch() {
  if (template_name_.empty()) {
    return std::unexpected(makeError(ErrorCode::kValidationError, "Search query is required"));
  }

  auto& template_mgr = app_.templateManager();
  
  auto results = template_mgr.searchTemplates(template_name_);
  if (!results.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << results.error().message() << R"("}))" << std::endl;
    } else {
      std::cout << "Error searching templates: " << results.error().message() << std::endl;
    }
    return 1;
  }

  auto templates = results.value();
  if (templates.empty()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"templates": [], "query": ")" << template_name_ << R"("}))" << std::endl;
    } else {
      std::cout << "No templates found matching '" << template_name_ << "'" << std::endl;
    }
    return 0;
  }

  if (app_.globalOptions().json) {
    nlohmann::json output;
    output["query"] = template_name_;
    
    nlohmann::json template_array = nlohmann::json::array();
    for (const auto& tpl : templates) {
      nlohmann::json template_json;
      template_json["name"] = tpl.name;
      template_json["description"] = tpl.description;
      template_json["category"] = tpl.category;
      template_json["variables"] = tpl.variables;
      template_array.push_back(template_json);
    }
    output["templates"] = template_array;
    
    std::cout << output.dump(2) << std::endl;
  } else {
    std::cout << "Found " << templates.size() << " template(s) matching '" << template_name_ << "':\n" << std::endl;
    outputTemplateList(templates, app_.globalOptions());
  }

  return 0;
}

Result<int> TplCommand::executeInstall() {
  auto& template_mgr = app_.templateManager();
  
  if (install_builtins_) {
    auto result = template_mgr.installBuiltinTemplates();
    if (!result.has_value()) {
      if (app_.globalOptions().json) {
        std::cout << R"({"error": ")" << result.error().message() << R"("}))" << std::endl;
      } else {
        std::cout << "Error installing built-in templates: " << result.error().message() << std::endl;
      }
      return 1;
    }

    if (app_.globalOptions().json) {
      nlohmann::json output;
      output["success"] = true;
      output["action"] = "installed_builtins";
      std::cout << output.dump(2) << std::endl;
    } else {
      std::cout << "Built-in templates installed successfully!" << std::endl;
      std::cout << "Run 'nx tpl list' to see available templates." << std::endl;
    }

    return 0;
  }

  // Default install behavior
  if (app_.globalOptions().json) {
    nlohmann::json output;
    output["error"] = "No installation option specified. Use --builtins to install built-in templates.";
    std::cout << output.dump(2) << std::endl;
  } else {
    std::cout << "No installation option specified." << std::endl;
    std::cout << "Use: nx tpl install --builtins" << std::endl;
  }

  return 1;
}

// Helper methods

void TplCommand::outputTemplateList(const std::vector<nx::template_system::TemplateInfo>& templates, 
                                   const GlobalOptions& options) {
  if (options.json) {
    nlohmann::json output;
    nlohmann::json template_array = nlohmann::json::array();
    
    for (const auto& tpl : templates) {
      nlohmann::json template_json;
      template_json["name"] = tpl.name;
      template_json["description"] = tpl.description;
      template_json["category"] = tpl.category;
      template_json["variables"] = tpl.variables;
      template_array.push_back(template_json);
    }
    output["templates"] = template_array;
    
    std::cout << output.dump(2) << std::endl;
  } else {
    // Group by category
    std::map<std::string, std::vector<nx::template_system::TemplateInfo>> grouped;
    for (const auto& tpl : templates) {
      grouped[tpl.category].push_back(tpl);
    }

    for (const auto& [category, category_templates] : grouped) {
      std::cout << "\n" << category << ":" << std::endl;
      std::cout << std::string(category.length() + 1, '-') << std::endl;
      
      for (const auto& tpl : category_templates) {
        std::cout << "  " << tpl.name;
        
        if (!tpl.description.empty()) {
          std::cout << " - " << tpl.description;
        }
        
        if (!tpl.variables.empty()) {
          std::cout << " (variables: ";
          for (size_t i = 0; i < tpl.variables.size(); ++i) {
            std::cout << "{{" << tpl.variables[i] << "}}";
            if (i < tpl.variables.size() - 1) std::cout << ", ";
          }
          std::cout << ")";
        }
        
        std::cout << std::endl;
      }
    }
    
    std::cout << std::endl;
  }
}

void TplCommand::outputTemplateInfo(const nx::template_system::TemplateInfo& info, 
                                   const GlobalOptions& options) {
  if (!options.json) {
    std::cout << "Template: " << info.name << std::endl;
    std::cout << "Description: " << (info.description.empty() ? "None" : info.description) << std::endl;
    std::cout << "Category: " << info.category << std::endl;
    
    if (!info.variables.empty()) {
      std::cout << "Variables: ";
      for (size_t i = 0; i < info.variables.size(); ++i) {
        std::cout << "{{" << info.variables[i] << "}}";
        if (i < info.variables.size() - 1) std::cout << ", ";
      }
      std::cout << std::endl;
    } else {
      std::cout << "Variables: None" << std::endl;
    }
  }
}

std::map<std::string, std::string> TplCommand::parseVariables(const std::vector<std::string>& var_strings) {
  std::map<std::string, std::string> variables;
  
  for (const auto& var_string : var_strings) {
    auto eq_pos = var_string.find('=');
    if (eq_pos != std::string::npos) {
      std::string key = var_string.substr(0, eq_pos);
      std::string value = var_string.substr(eq_pos + 1);
      variables[key] = value;
    }
  }
  
  return variables;
}

std::string TplCommand::promptForTemplate(const std::vector<nx::template_system::TemplateInfo>& templates) {
  // For CLI use, just return the first template name as a fallback
  if (!templates.empty()) {
    return templates[0].name;
  }
  return "";
}

} // namespace nx::cli