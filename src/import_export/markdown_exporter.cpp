#include "nx/import_export/exporter.hpp"

#include <fstream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <regex>

#include "nx/util/filesystem.hpp"

namespace nx::import_export {

Result<void> MarkdownExporter::exportNotes(const std::vector<nx::core::Note>& notes, 
                                           const ExportOptions& options) {
  if (notes.empty()) {
    return {};
  }

  // Create output directory
  if (!std::filesystem::exists(options.output_path)) {
    std::error_code ec;
    std::filesystem::create_directories(options.output_path, ec);
    if (ec) {
      return std::unexpected(makeError(ErrorCode::kDirectoryCreateError,
                                       "Failed to create output directory: " + ec.message()));
    }
  }

  // Export each note as a separate markdown file
  for (const auto& note : notes) {
    std::string filename = generateFilename(note, ".md");
    std::filesystem::path note_path = options.output_path / filename;

    // Format note content
    std::string content = formatNoteContent(note, options.include_metadata);

    // Write to file
    std::ofstream file(note_path);
    if (!file) {
      return std::unexpected(makeError(ErrorCode::kFileWriteError,
                                       "Failed to create file: " + note_path.string()));
    }

    file << content;
    if (!file) {
      return std::unexpected(makeError(ErrorCode::kFileWriteError,
                                       "Failed to write to file: " + note_path.string()));
    }
  }

  // Create index file if multiple notes
  if (notes.size() > 1) {
    std::filesystem::path index_path = options.output_path / "index.md";
    std::ofstream index_file(index_path);
    if (index_file) {
      index_file << "# Notes Index\n\n";
      index_file << "This directory contains " << notes.size() << " exported notes.\n\n";
      
      for (const auto& note : notes) {
        std::string filename = generateFilename(note, ".md");
        index_file << "- [" << note.title() << "](./" << filename << ")\n";
      }
    }
  }

  return {};
}

std::string MarkdownExporter::generateFilename(const nx::core::Note& note, 
                                               const std::string& extension) const {
  std::string title = note.title();
  
  // Replace invalid filename characters
  std::regex invalid_chars(R"([<>:"/\\|?*])");
  title = std::regex_replace(title, invalid_chars, "_");
  
  // Limit length
  if (title.length() > 100) {
    title = title.substr(0, 100);
  }
  
  // Add note ID for uniqueness
  std::string id_suffix = "_" + note.id().toString().substr(0, 8);
  
  return title + id_suffix + extension;
}

std::string MarkdownExporter::formatNoteContent(const nx::core::Note& note, 
                                                bool include_metadata) const {
  std::ostringstream content;
  
  if (include_metadata) {
    content << "---\n";
    content << "id: " << note.id().toString() << "\n";
    content << "title: \"" << note.title() << "\"\n";
    
    // Format creation and modification times
    auto created_time = std::chrono::system_clock::to_time_t(note.metadata().created());
    auto modified_time = std::chrono::system_clock::to_time_t(note.metadata().updated());
    
    content << "created: " << std::put_time(std::gmtime(&created_time), "%Y-%m-%dT%H:%M:%SZ") << "\n";
    content << "modified: " << std::put_time(std::gmtime(&modified_time), "%Y-%m-%dT%H:%M:%SZ") << "\n";
    
    // Add tags
    const auto& tags = note.metadata().tags();
    if (!tags.empty()) {
      content << "tags:\n";
      for (const auto& tag : tags) {
        content << "  - " << tag << "\n";
      }
    }
    
    // Add notebook
    if (note.notebook().has_value()) {
      content << "notebook: \"" << note.notebook().value() << "\"\n";
    }
    
    content << "---\n\n";
  }
  
  // Add note content
  content << note.content();
  
  return content.str();
}

} // namespace nx::import_export