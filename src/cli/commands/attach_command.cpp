#include "nx/cli/commands/attach_command.hpp"

#include <iostream>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace nx::cli {

AttachCommand::AttachCommand(Application& app) : app_(app) {
}

Result<int> AttachCommand::execute(const GlobalOptions& options) {
  try {
    // Resolve the note ID
    auto resolved_id = app_.noteStore().resolveSingle(note_id_);
    if (!resolved_id.has_value()) {
      if (options.json) {
        std::cout << R"({"error": ")" << resolved_id.error().message() << R"(", "note_id": ")" << note_id_ << R"("})" << std::endl;
      } else {
        std::cout << "Error: " << resolved_id.error().message() << std::endl;
      }
      return 1;
    }

    // Verify the file exists
    std::filesystem::path source_file(file_path_);
    if (!std::filesystem::exists(source_file)) {
      auto error_msg = "File not found: " + file_path_;
      if (options.json) {
        std::cout << R"({"error": ")" << error_msg << R"(", "file_path": ")" << file_path_ << R"("})" << std::endl;
      } else {
        std::cout << "Error: " << error_msg << std::endl;
      }
      return 1;
    }

    // Store the attachment
    auto attach_result = app_.attachmentStore().store(*resolved_id, source_file, description_);
    if (!attach_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": ")" << attach_result.error().message() << R"(", "note_id": ")" 
                  << resolved_id->toString() << R"(", "file_path": ")" << file_path_ << R"("})" << std::endl;
      } else {
        std::cout << "Error attaching file: " << attach_result.error().message() << std::endl;
      }
      return 1;
    }

    auto attachment = attach_result.value();

    // Output result
    if (options.json) {
      nlohmann::json result;
      result["attachment_id"] = attachment.id.toString();
      result["note_id"] = attachment.parent_note.toString();
      result["filename"] = attachment.original_name;
      result["size"] = attachment.size;
      result["mime_type"] = attachment.mime_type;
      if (!attachment.description.empty()) {
        result["description"] = attachment.description;
      }
      result["created"] = std::chrono::duration_cast<std::chrono::milliseconds>(
          attachment.created.time_since_epoch()).count();
      result["relative_path"] = attachment.relativePath();
      
      std::cout << result.dump(2) << std::endl;
    } else {
      if (!options.quiet) {
        std::cout << "Successfully attached file to note" << std::endl;
        std::cout << "  Note: " << resolved_id->toString() << std::endl;
        std::cout << "  File: " << attachment.original_name << " (" << attachment.size << " bytes)" << std::endl;
        std::cout << "  Attachment ID: " << attachment.id.toString() << std::endl;
        if (!attachment.description.empty()) {
          std::cout << "  Description: " << attachment.description << std::endl;
        }
        std::cout << "  MIME Type: " << attachment.mime_type << std::endl;
      }
    }

    return 0;

  } catch (const std::exception& e) {
    if (options.json) {
      std::cout << R"({"error": ")" << e.what() << R"(", "note_id": ")" << note_id_ << R"(", "file_path": ")" << file_path_ << R"("})" << std::endl;
    } else {
      std::cout << "Error: " << e.what() << std::endl;
    }
    return 1;
  }
}

void AttachCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("note_id", note_id_, "Note ID to attach file to (can be partial)")->required();
  cmd->add_option("file_path", file_path_, "Path to file to attach")->required();
  cmd->add_option("-d,--description", description_, "Optional description for the attachment");
}

} // namespace nx::cli