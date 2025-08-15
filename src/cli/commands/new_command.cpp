#include "nx/cli/commands/new_command.hpp"

#include <iostream>
#include <sstream>
#include <unistd.h>
#include <nlohmann/json.hpp>

#include "nx/core/note.hpp"
#include "nx/core/metadata.hpp"

namespace nx::cli {

NewCommand::NewCommand(Application& app) : app_(app) {
}

void NewCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("title", title_, "Note title (optional)");
  cmd->add_option("--tags,-t", tags_, "Tags for the note (comma-separated)");
  cmd->add_option("--notebook,--nb", notebook_, "Notebook name");
  cmd->add_option("--from", template_name_, "Create from template");
  cmd->add_flag("--edit,-e", edit_after_create_, "Edit after creation");
}

Result<int> NewCommand::execute(const GlobalOptions& options) {
  try {
    // Generate new note ID
    auto note_id = nx::core::NoteId::generate();
    
    // Create metadata
    nx::core::Metadata metadata(note_id, title_.empty() ? "Untitled" : title_);
    
    // Add tags
    for (const auto& tag : tags_) {
      metadata.addTag(tag);
    }
    
    // Set notebook
    if (!notebook_.empty()) {
      metadata.setNotebook(notebook_);
    }
    
    // Determine content based on priority:
    // 1. Template content (if --from specified)
    // 2. Stdin input (if available)
    // 3. Default content
    std::string content;
    std::string body_content;
    
    if (!template_name_.empty()) {
      // Simple template system - check for common templates
      if (template_name_ == "daily" || template_name_ == "journal") {
        body_content = "# Daily Journal\n\n## Tasks\n- [ ] \n\n## Notes\n\n## Reflection\n\n";
      } else if (template_name_ == "meeting") {
        body_content = "# Meeting Notes\n\n**Date:** \n**Attendees:** \n\n## Agenda\n- \n\n## Discussion\n\n## Action Items\n- [ ] \n\n";
      } else if (template_name_ == "project") {
        body_content = "# Project Notes\n\n## Overview\n\n## Goals\n- \n\n## Tasks\n- [ ] \n\n## Resources\n\n";
      } else {
        // Unknown template - use default with note about template
        body_content = "# " + title_ + "\n\n*Note: Template '" + template_name_ + "' not found, using default.*\n\n";
      }
    } else if (hasStdinInput()) {
      // Read content from stdin
      body_content = readStdinContent();
      
      // Remove trailing newline if present
      if (!body_content.empty() && body_content.back() == '\n') {
        body_content.pop_back();
      }
    }
    
    // Create final content with title header
    if (title_.empty()) {
      if (body_content.empty()) {
        content = "# Untitled\n\n";
      } else {
        content = "# Untitled\n\n" + body_content;
      }
    } else {
      if (body_content.empty()) {
        content = "# " + title_ + "\n\n";
      } else {
        content = "# " + title_ + "\n\n" + body_content;
      }
    }
    
    nx::core::Note note(std::move(metadata), content);
    
    // Store the note
    auto store_result = app_.noteStore().store(note);
    if (!store_result.has_value()) {
      return std::unexpected(store_result.error());
    }
    
    // Add to search index
    auto index_result = app_.searchIndex().addNote(note);
    if (!index_result.has_value()) {
      // Non-fatal error - warn but continue
      if (!options.quiet) {
        std::cerr << "Warning: Failed to add note to search index: " 
                  << index_result.error().message() << std::endl;
      }
    }
    
    // Output result
    if (options.json) {
      nlohmann::json result;
      result["id"] = note.id().toString();
      result["title"] = note.title();
      result["created"] = std::chrono::duration_cast<std::chrono::milliseconds>(
          note.metadata().created().time_since_epoch()).count();
      result["tags"] = note.metadata().tags();
      if (note.notebook().has_value()) {
        result["notebook"] = *note.notebook();
      }
      result["success"] = true;
      
      std::cout << result.dump(2) << std::endl;
    } else {
      std::cout << "Created note: " << note.id().toString();
      if (!note.title().empty()) {
        std::cout << " (" << note.title() << ")";
      }
      std::cout << std::endl;
      
      if (options.verbose) {
        std::cout << "  Tags: ";
        const auto& tags = note.metadata().tags();
        if (tags.empty()) {
          std::cout << "none";
        } else {
          for (size_t i = 0; i < tags.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << tags[i];
          }
        }
        std::cout << std::endl;
        
        if (note.notebook().has_value()) {
          std::cout << "  Notebook: " << *note.notebook() << std::endl;
        }
      }
    }
    
    return 0;
    
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kUnknownError, 
                                     "Failed to create note: " + std::string(e.what())));
  }
}

bool NewCommand::hasStdinInput() const {
  return !isatty(STDIN_FILENO);
}

std::string NewCommand::readStdinContent() const {
  std::ostringstream buffer;
  std::string line;
  
  while (std::getline(std::cin, line)) {
    buffer << line << '\n';
  }
  
  return buffer.str();
}

} // namespace nx::cli