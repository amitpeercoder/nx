#include "nx/cli/commands/import_command.hpp"

#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>

#include "nx/import_export/import_manager.hpp"

namespace nx::cli {

ImportCommand::ImportCommand(Application& app) : app_(app) {
}

Result<int> ImportCommand::execute(const GlobalOptions& options) {
  try {
    if (subcommand_ == "dir") {
      return executeDir();
    } else if (subcommand_ == "file") {
      return executeFile();
    } else if (subcommand_ == "obsidian") {
      return executeObsidian();
    } else if (subcommand_ == "notion") {
      return executeNotion();
    } else {
      std::cout << "Error: Unknown import subcommand: " << subcommand_ << std::endl;
      std::cout << "Available subcommands: dir, file, obsidian, notion" << std::endl;
      return 1;
    }
    
  } catch (const std::exception& e) {
    if (options.json) {
      std::cout << R"({"error": ")" << e.what() << R"(", "subcommand": ")" << subcommand_ << R"("})" << std::endl;
    } else {
      std::cout << "Error: " << e.what() << std::endl;
    }
    return 1;
  }
}

void ImportCommand::setupCommand(CLI::App* cmd) {
  // Add subcommands
  auto dir_cmd = cmd->add_subcommand("dir", "Import directory of notes");
  dir_cmd->add_option("path", source_path_, "Directory path to import from")->required();
  dir_cmd->add_option("--notebook,-n", target_notebook_, "Target notebook name");
  dir_cmd->add_flag("--recursive,-r", recursive_, "Import files recursively");
  dir_cmd->add_option("--extensions,-e", extensions_, "File extensions to import");
  dir_cmd->add_flag("--preserve-structure,-p", preserve_structure_, 
                   "Create notebooks from directory structure");
  dir_cmd->add_flag("--overwrite", overwrite_, "Overwrite existing notes");
  dir_cmd->add_flag("--skip-hidden", skip_hidden_, "Skip hidden files");
  dir_cmd->callback([this]() { subcommand_ = "dir"; });
  
  auto file_cmd = cmd->add_subcommand("file", "Import single file");
  file_cmd->add_option("path", source_path_, "File path to import")->required();
  file_cmd->add_option("--notebook,-n", target_notebook_, "Target notebook name");
  file_cmd->callback([this]() { subcommand_ = "file"; });
  
  auto obsidian_cmd = cmd->add_subcommand("obsidian", "Import Obsidian vault");
  obsidian_cmd->add_option("vault_path", source_path_, "Path to Obsidian vault")->required();
  obsidian_cmd->callback([this]() { subcommand_ = "obsidian"; });
  
  auto notion_cmd = cmd->add_subcommand("notion", "Import Notion export");
  notion_cmd->add_option("export_path", source_path_, "Path to Notion export")->required();
  notion_cmd->callback([this]() { subcommand_ = "notion"; });
  
  cmd->require_subcommand(1);
}

Result<int> ImportCommand::executeDir() {
  if (source_path_.empty()) {
    return std::unexpected(makeError(ErrorCode::kValidationError, "Source directory path is required"));
  }
  
  nx::import_export::ImportManager importer(app_.noteStore());
  
  nx::import_export::ImportManager::ImportOptions options;
  options.source_dir = source_path_;
  options.target_notebook = target_notebook_;
  options.recursive = recursive_;
  options.extensions = extensions_;
  options.preserve_structure = preserve_structure_;
  options.overwrite_existing = overwrite_;
  options.skip_hidden = skip_hidden_;
  
  auto result = importer.importDirectory(options);
  if (!result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << result.error().message() 
                << R"(", "source_dir": ")" << source_path_ << R"("})" << std::endl;
    } else {
      std::cout << "Error importing directory: " << result.error().message() << std::endl;
    }
    return 1;
  }
  
  outputResult(result.value(), app_.globalOptions());
  return 0;
}

Result<int> ImportCommand::executeFile() {
  if (source_path_.empty()) {
    return std::unexpected(makeError(ErrorCode::kValidationError, "Source file path is required"));
  }
  
  nx::import_export::ImportManager importer(app_.noteStore());
  
  auto result = importer.importFile(source_path_, target_notebook_);
  if (!result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << result.error().message() 
                << R"(", "source_file": ")" << source_path_ << R"("})" << std::endl;
    } else {
      std::cout << "Error importing file: " << result.error().message() << std::endl;
    }
    return 1;
  }
  
  outputResult(result.value(), app_.globalOptions());
  return 0;
}

Result<int> ImportCommand::executeObsidian() {
  if (source_path_.empty()) {
    return std::unexpected(makeError(ErrorCode::kValidationError, "Obsidian vault path is required"));
  }
  
  nx::import_export::ObsidianImporter importer(app_.noteStore());
  
  auto result = importer.importVault(source_path_);
  if (!result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << result.error().message() 
                << R"(", "vault_path": ")" << source_path_ << R"("})" << std::endl;
    } else {
      std::cout << "Error importing Obsidian vault: " << result.error().message() << std::endl;
    }
    return 1;
  }
  
  outputResult(result.value(), app_.globalOptions());
  return 0;
}

Result<int> ImportCommand::executeNotion() {
  if (source_path_.empty()) {
    return std::unexpected(makeError(ErrorCode::kValidationError, "Notion export path is required"));
  }
  
  nx::import_export::NotionImporter importer(app_.noteStore());
  
  auto result = importer.importExport(source_path_);
  if (!result.has_value()) {
    if (app_.globalOptions().json) {
      std::cout << R"({"error": ")" << result.error().message() 
                << R"(", "export_path": ")" << source_path_ << R"("})" << std::endl;
    } else {
      std::cout << "Error importing Notion export: " << result.error().message() << std::endl;
    }
    return 1;
  }
  
  outputResult(result.value(), app_.globalOptions());
  return 0;
}

void ImportCommand::outputResult(const nx::import_export::ImportManager::ImportResult& result, 
                                const GlobalOptions& options) {
  if (options.json) {
    nlohmann::json output;
    output["notes_imported"] = result.notes_imported;
    output["files_skipped"] = result.files_skipped;
    output["files_failed"] = result.files_failed;
    output["errors"] = result.errors;
    
    nlohmann::json created_notes = nlohmann::json::array();
    for (const auto& note_id : result.created_notes) {
      created_notes.push_back(note_id.toString());
    }
    output["created_notes"] = created_notes;
    
    nlohmann::json notebooks_created = nlohmann::json::object();
    for (const auto& [notebook, count] : result.notebooks_created) {
      notebooks_created[notebook] = count;
    }
    output["notebooks_created"] = notebooks_created;
    
    std::cout << output.dump(2) << std::endl;
    
  } else {
    if (!options.quiet) {
      std::cout << "Import completed successfully!" << std::endl;
      std::cout << "  Notes imported: " << result.notes_imported << std::endl;
      std::cout << "  Files skipped: " << result.files_skipped << std::endl;
      
      if (result.files_failed > 0) {
        std::cout << "  Files failed: " << result.files_failed << std::endl;
      }
      
      if (!result.notebooks_created.empty()) {
        std::cout << "  Notebooks created:" << std::endl;
        for (const auto& [notebook, count] : result.notebooks_created) {
          std::cout << "    " << notebook << ": " << count << " notes" << std::endl;
        }
      }
      
      if (!result.errors.empty()) {
        std::cout << "\nErrors encountered:" << std::endl;
        for (const auto& error : result.errors) {
          std::cout << "  - " << error << std::endl;
        }
      }
    }
  }
}

} // namespace nx::cli