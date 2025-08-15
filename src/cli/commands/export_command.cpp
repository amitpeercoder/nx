#include "nx/cli/commands/export_command.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

#include "nx/import_export/exporter.hpp"

namespace nx::cli {

ExportCommand::ExportCommand(Application& app) : app_(app) {}

void ExportCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("format", format_, "Export format: markdown, json, zip, html, pdf")
     ->check(CLI::IsMember({"markdown", "md", "json", "zip", "html", "htm", "pdf"}));
  
  cmd->add_option("--output,-o", output_path_, "Output path (file or directory)")
     ->required();
  
  cmd->add_option("--tags,-t", tag_filter_, "Filter by tags (export only notes with these tags)");
  cmd->add_option("--notebook,--nb", notebook_filter_, "Filter by notebook");
  cmd->add_option("--since", date_filter_, "Export notes since date (YYYY-MM-DD)");
  
  cmd->add_flag("--no-metadata", include_metadata_, "Exclude metadata from export");
  cmd->add_flag("--no-structure", preserve_structure_, "Don't preserve directory structure");
  cmd->add_flag("--include-attachments", include_attachments_, "Include note attachments");
  cmd->add_flag("--compress", compress_, "Compress output where supported");
  cmd->add_option("--template", template_file_, "Custom template file (for HTML export)");
}

Result<int> ExportCommand::execute(const GlobalOptions& options) {
  try {
    // Parse export format
    auto format_result = nx::import_export::ExportManager::parseFormat(format_);
    if (!format_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": ")" << format_result.error().message() << R"(", "success": false})" << std::endl;
      } else {
        std::cout << "Error: " << format_result.error().message() << std::endl;
      }
      return 1;
    }

    auto export_format = format_result.value();

    // Get all notes
    auto notes_result = app_.noteStore().list();
    if (!notes_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": ")" << notes_result.error().message() << R"(", "success": false})" << std::endl;
      } else {
        std::cout << "Error: " << notes_result.error().message() << std::endl;
      }
      return 1;
    }

    // Load full note content
    std::vector<nx::core::Note> notes;
    for (const auto& note_id : notes_result.value()) {
      auto note_result = app_.noteStore().load(note_id);
      if (note_result.has_value()) {
        notes.push_back(note_result.value());
      } else if (!options.quiet) {
        std::cerr << "Warning: Failed to load note " << note_id.toString() 
                  << ": " << note_result.error().message() << std::endl;
      }
    }

    if (notes.empty()) {
      if (options.json) {
        std::cout << R"({"error": "No notes found to export", "success": false})" << std::endl;
      } else {
        std::cout << "Error: No notes found to export" << std::endl;
      }
      return 1;
    }

    // Set up export options
    nx::import_export::ExportOptions export_options;
    export_options.format = export_format;
    export_options.output_path = output_path_;
    export_options.include_metadata = include_metadata_;
    export_options.preserve_structure = preserve_structure_;
    export_options.include_attachments = include_attachments_;
    export_options.tag_filter = tag_filter_;
    export_options.compress = compress_;
    export_options.template_file = template_file_;
    
    if (!notebook_filter_.empty()) {
      export_options.notebook_filter = notebook_filter_;
    }
    
    if (!date_filter_.empty()) {
      export_options.date_filter = date_filter_;
    }

    // Perform export
    auto export_result = nx::import_export::ExportManager::exportNotes(notes, export_options);
    if (!export_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": ")" << export_result.error().message() << R"(", "success": false})" << std::endl;
      } else {
        std::cout << "Error: " << export_result.error().message() << std::endl;
      }
      return 1;
    }

    // Count exported notes after filtering
    auto filtered_notes = nx::import_export::ExportManager::filterNotes(notes, export_options);

    if (options.json) {
      nlohmann::json result;
      result["success"] = true;
      result["format"] = format_;
      result["output_path"] = output_path_;
      result["total_notes"] = notes.size();
      result["exported_notes"] = filtered_notes.size();
      result["filters"] = {
        {"tags", tag_filter_},
        {"notebook", notebook_filter_},
        {"since", date_filter_}
      };
      std::cout << result.dump(2) << std::endl;
    } else {
      std::cout << "Export completed successfully!" << std::endl;
      std::cout << "Format: " << format_ << std::endl;
      std::cout << "Output: " << output_path_ << std::endl;
      std::cout << "Exported " << filtered_notes.size() << " of " << notes.size() << " notes" << std::endl;
      
      if (filtered_notes.size() < notes.size()) {
        std::cout << "Filters applied:" << std::endl;
        if (!tag_filter_.empty()) {
          std::cout << "  Tags: ";
          for (size_t i = 0; i < tag_filter_.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << tag_filter_[i];
          }
          std::cout << std::endl;
        }
        if (!notebook_filter_.empty()) {
          std::cout << "  Notebook: " << notebook_filter_ << std::endl;
        }
        if (!date_filter_.empty()) {
          std::cout << "  Since: " << date_filter_ << std::endl;
        }
      }
    }

    return 0;

  } catch (const std::exception& e) {
    if (options.json) {
      std::cout << R"({"error": ")" << e.what() << R"(", "success": false})" << std::endl;
    } else {
      std::cout << "Error: " << e.what() << std::endl;
    }
    return 1;
  }
}

} // namespace nx::cli