#include "nx/cli/commands/notebook_command.hpp"

#include <iostream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include "nx/store/notebook_manager.hpp"
#include "nx/util/error_handler.hpp"

namespace nx::cli {

NotebookCommand::NotebookCommand(Application& app) : app_(app) {}

Result<int> NotebookCommand::execute(const GlobalOptions& options) {
  switch (sub_command_) {
    case SubCommand::List:
      return executeList(options);
    case SubCommand::Create:
      return executeCreate(options);
    case SubCommand::Rename:
      return executeRename(options);
    case SubCommand::Delete:
      return executeDelete(options);
    case SubCommand::Info:
      return executeInfo(options);
  }
  return std::unexpected(makeError(ErrorCode::kInvalidArgument, "Invalid subcommand"));
}

std::string NotebookCommand::name() const {
  return "notebook";
}

std::string NotebookCommand::description() const {
  return "Manage notebooks";
}

void NotebookCommand::setupCommand(CLI::App* cmd) {
  // Add subcommands
  auto list_cmd = cmd->add_subcommand("list", "List all notebooks");
  list_cmd->add_flag("--stats", with_stats_, "Include detailed statistics");
  list_cmd->callback([this]() { sub_command_ = SubCommand::List; });

  auto create_cmd = cmd->add_subcommand("create", "Create a new notebook");
  create_cmd->add_option("name", notebook_name_, "Notebook name")->required();
  create_cmd->callback([this]() { sub_command_ = SubCommand::Create; });

  auto rename_cmd = cmd->add_subcommand("rename", "Rename an existing notebook");
  rename_cmd->add_option("old_name", notebook_name_, "Current notebook name")->required();
  rename_cmd->add_option("new_name", new_name_, "New notebook name")->required();
  rename_cmd->callback([this]() { sub_command_ = SubCommand::Rename; });

  auto delete_cmd = cmd->add_subcommand("delete", "Delete a notebook");
  delete_cmd->add_option("name", notebook_name_, "Notebook name")->required();
  delete_cmd->add_flag("--force", force_, "Force deletion even if notebook contains notes");
  delete_cmd->callback([this]() { sub_command_ = SubCommand::Delete; });

  auto info_cmd = cmd->add_subcommand("info", "Show detailed information about a notebook");
  info_cmd->add_option("name", notebook_name_, "Notebook name")->required();
  info_cmd->add_flag("--stats", with_stats_, "Include detailed statistics");
  info_cmd->callback([this]() { sub_command_ = SubCommand::Info; });

  // Global options
  cmd->add_flag("--json", json_output_, "Output in JSON format");
  
  // Require exactly one subcommand
  cmd->require_subcommand(1, 1);
}

Result<int> NotebookCommand::executeList(const GlobalOptions& options) {
  auto& notebook_manager = app_.notebookManager();
  
  auto notebooks_result = notebook_manager.listNotebooks(with_stats_);
  if (!notebooks_result.has_value()) {
    displayError(notebooks_result.error(), "listing notebooks", options);
    return 1;
  }

  const auto& notebooks = notebooks_result.value();
  
  if (options.json || json_output_) {
    printNotebookList(notebooks, true);
  } else {
    if (notebooks.empty()) {
      std::cout << "No notebooks found." << std::endl;
      return 0;
    }
    
    printNotebookList(notebooks, false);
  }

  return 0;
}

Result<int> NotebookCommand::executeCreate(const GlobalOptions& options) {
  auto& notebook_manager = app_.notebookManager();
  
  auto result = notebook_manager.createNotebook(notebook_name_);
  if (!result.has_value()) {
    displayError(result.error(), "creating notebook '" + notebook_name_ + "'", options);
    return 1;
  }

  if (options.json || json_output_) {
    nlohmann::json output = {
      {"success", true},
      {"message", "Notebook created successfully"},
      {"notebook", notebook_name_}
    };
    std::cout << output.dump(2) << std::endl;
  } else {
    std::cout << "Created notebook: " << notebook_name_ << std::endl;
  }

  return 0;
}

Result<int> NotebookCommand::executeRename(const GlobalOptions& options) {
  auto& notebook_manager = app_.notebookManager();
  
  auto result = notebook_manager.renameNotebook(notebook_name_, new_name_);
  if (!result.has_value()) {
    displayError(result.error(), "renaming notebook '" + notebook_name_ + "' to '" + new_name_ + "'", options);
    return 1;
  }

  if (options.json || json_output_) {
    nlohmann::json output = {
      {"success", true},
      {"message", "Notebook renamed successfully"},
      {"old_name", notebook_name_},
      {"new_name", new_name_}
    };
    std::cout << output.dump(2) << std::endl;
  } else {
    std::cout << "Renamed notebook '" << notebook_name_ << "' to '" << new_name_ << "'" << std::endl;
  }

  return 0;
}

Result<int> NotebookCommand::executeDelete(const GlobalOptions& options) {
  auto& notebook_manager = app_.notebookManager();
  
  auto result = notebook_manager.deleteNotebook(notebook_name_, force_);
  if (!result.has_value()) {
    displayError(result.error(), "deleting notebook '" + notebook_name_ + "'", options);
    return 1;
  }

  if (options.json || json_output_) {
    nlohmann::json output = {
      {"success", true},
      {"message", "Notebook deleted successfully"},
      {"notebook", notebook_name_},
      {"force", force_}
    };
    std::cout << output.dump(2) << std::endl;
  } else {
    std::cout << "Deleted notebook: " << notebook_name_ << std::endl;
  }

  return 0;
}

Result<int> NotebookCommand::executeInfo(const GlobalOptions& options) {
  auto& notebook_manager = app_.notebookManager();
  
  auto info_result = notebook_manager.getNotebookInfo(notebook_name_, with_stats_);
  if (!info_result.has_value()) {
    displayError(info_result.error(), "getting info for notebook '" + notebook_name_ + "'", options);
    return 1;
  }

  const auto& info = info_result.value();
  
  if (options.json || json_output_) {
    printNotebookInfo(info, true);
  } else {
    printNotebookInfo(info, false);
  }

  return 0;
}

void NotebookCommand::printNotebookInfo(const nx::store::NotebookInfo& info, bool json_format) const {
  if (json_format) {
    nlohmann::json output = {
      {"name", info.name},
      {"note_count", info.note_count},
      {"created", std::chrono::duration_cast<std::chrono::seconds>(info.created.time_since_epoch()).count()},
      {"last_modified", std::chrono::duration_cast<std::chrono::seconds>(info.last_modified.time_since_epoch()).count()},
      {"recent_notes", info.recent_notes},
      {"total_size", info.total_size}
    };
    
    // Add tags
    nlohmann::json tags_array = nlohmann::json::array();
    for (const auto& tag : info.tags) {
      tags_array.push_back(tag);
    }
    output["tags"] = tags_array;
    
    // Add tag counts
    nlohmann::json tag_counts_obj = nlohmann::json::object();
    for (const auto& [tag, count] : info.tag_counts) {
      tag_counts_obj[tag] = count;
    }
    output["tag_counts"] = tag_counts_obj;
    
    std::cout << output.dump(2) << std::endl;
  } else {
    std::cout << "Notebook: " << info.name << std::endl;
    std::cout << "Notes: " << info.note_count << std::endl;
    
    if (with_stats_) {
      std::cout << "Recent notes (last week): " << info.recent_notes << std::endl;
      std::cout << "Total size: " << info.total_size << " bytes" << std::endl;
      
      // Format timestamps
      auto created_time = std::chrono::system_clock::to_time_t(info.created);
      auto modified_time = std::chrono::system_clock::to_time_t(info.last_modified);
      
      std::cout << "Created: " << std::put_time(std::localtime(&created_time), "%Y-%m-%d %H:%M:%S") << std::endl;
      std::cout << "Last modified: " << std::put_time(std::localtime(&modified_time), "%Y-%m-%d %H:%M:%S") << std::endl;
      
      if (!info.tags.empty()) {
        std::cout << "Top tags:" << std::endl;
        for (size_t i = 0; i < std::min(info.tags.size(), size_t(10)); ++i) {
          const auto& tag = info.tags[i];
          auto count_it = info.tag_counts.find(tag);
          size_t count = (count_it != info.tag_counts.end()) ? count_it->second : 0;
          std::cout << "  " << tag << " (" << count << ")" << std::endl;
        }
      }
    }
  }
}

void NotebookCommand::printNotebookList(const std::vector<nx::store::NotebookInfo>& notebooks, bool json_format) const {
  if (json_format) {
    nlohmann::json output = nlohmann::json::array();
    
    for (const auto& notebook : notebooks) {
      nlohmann::json notebook_obj = {
        {"name", notebook.name},
        {"note_count", notebook.note_count}
      };
      
      if (with_stats_) {
        notebook_obj["created"] = std::chrono::duration_cast<std::chrono::seconds>(notebook.created.time_since_epoch()).count();
        notebook_obj["last_modified"] = std::chrono::duration_cast<std::chrono::seconds>(notebook.last_modified.time_since_epoch()).count();
        notebook_obj["recent_notes"] = notebook.recent_notes;
        notebook_obj["total_size"] = notebook.total_size;
        
        // Add top tags
        nlohmann::json tags_array = nlohmann::json::array();
        for (size_t i = 0; i < std::min(notebook.tags.size(), size_t(5)); ++i) {
          tags_array.push_back(notebook.tags[i]);
        }
        notebook_obj["top_tags"] = tags_array;
      }
      
      output.push_back(notebook_obj);
    }
    
    std::cout << output.dump(2) << std::endl;
  } else {
    // Table header
    if (with_stats_) {
      std::cout << std::left << std::setw(20) << "Name" 
                << std::setw(8) << "Notes"
                << std::setw(8) << "Recent"
                << std::setw(12) << "Size"
                << "Top Tags" << std::endl;
      std::cout << std::string(60, '-') << std::endl;
    } else {
      std::cout << std::left << std::setw(20) << "Name" 
                << "Notes" << std::endl;
      std::cout << std::string(30, '-') << std::endl;
    }
    
    // Table rows
    for (const auto& notebook : notebooks) {
      std::cout << std::left << std::setw(20) << notebook.name
                << std::setw(8) << notebook.note_count;
      
      if (with_stats_) {
        std::cout << std::setw(8) << notebook.recent_notes
                  << std::setw(12) << notebook.total_size;
        
        // Show top 3 tags
        std::vector<std::string> top_tags;
        for (size_t i = 0; i < std::min(notebook.tags.size(), size_t(3)); ++i) {
          top_tags.push_back(notebook.tags[i]);
        }
        
        if (!top_tags.empty()) {
          std::cout << "[";
          for (size_t i = 0; i < top_tags.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << top_tags[i];
          }
          std::cout << "]";
        }
      }
      
      std::cout << std::endl;
    }
    
    std::cout << "\nTotal: " << notebooks.size() << " notebook" << (notebooks.size() != 1 ? "s" : "") << std::endl;
  }
}

void NotebookCommand::displayError(const Error& error, const std::string& operation, const GlobalOptions& options) const {
  // Create contextual error with operation context
  auto context = nx::util::ErrorContext{}
    .withOperation(operation);
  
  auto contextual_error = nx::util::makeContextualError(
    error.code(), error.message(), context, nx::util::ErrorSeverity::kError);
  
  // Use ErrorHandler to format the error message appropriately
  auto& error_handler = nx::util::ErrorHandler::instance();
  
  if (options.json || json_output_) {
    // Output JSON error format
    std::cerr << error_handler.formatUserError(contextual_error, true) << std::endl;
  } else {
    // Output user-friendly error with colors and suggestions
    std::cerr << error_handler.formatUserError(contextual_error, false) << std::endl;
  }
}

} // namespace nx::cli