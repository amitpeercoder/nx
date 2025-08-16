#include "nx/import_export/exporter.hpp"

#include <fstream>
#include <chrono>

#include "nx/util/filesystem.hpp"

namespace nx::import_export {

Result<void> JsonExporter::exportNotes(const std::vector<nx::core::Note>& notes, 
                                       const ExportOptions& options) {
  if (notes.empty()) {
    return {};
  }

  // Create JSON structure
  nlohmann::json export_data;
  export_data["export_info"] = {
    {"format", "nx-notes-json"},
    {"version", "1.0"},
    {"exported_at", std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count()},
    {"note_count", notes.size()}
  };

  nlohmann::json notes_array = nlohmann::json::array();
  for (const auto& note : notes) {
    notes_array.push_back(noteToJson(note));
  }
  export_data["notes"] = notes_array;

  // Write to file
  std::filesystem::path output_file = options.output_path;
  if (std::filesystem::is_directory(output_file)) {
    output_file = output_file / "notes_export.json";
  }

  // Ensure directory exists
  auto parent_dir = output_file.parent_path();
  if (!parent_dir.empty() && !std::filesystem::exists(parent_dir)) {
    std::error_code ec;
    std::filesystem::create_directories(parent_dir, ec);
    if (ec) {
      return std::unexpected(makeError(ErrorCode::kDirectoryCreateError,
                                       "Failed to create output directory: " + ec.message()));
    }
  }

  // Write JSON file atomically
  auto write_result = nx::util::FileSystem::writeFileAtomic(output_file, export_data.dump(2));
  if (!write_result.has_value()) {
    return std::unexpected(makeError(ErrorCode::kFileWriteError,
                                     "Failed to write JSON file: " + write_result.error().message()));
  }

  return {};
}

nlohmann::json JsonExporter::noteToJson(const nx::core::Note& note) const {
  nlohmann::json note_json;
  
  // Basic note information
  note_json["id"] = note.id().toString();
  note_json["title"] = note.title();
  note_json["content"] = note.content();
  
  // Metadata
  const auto& metadata = note.metadata();
  note_json["metadata"] = {
    {"created", std::chrono::duration_cast<std::chrono::milliseconds>(
        metadata.created().time_since_epoch()).count()},
    {"modified", std::chrono::duration_cast<std::chrono::milliseconds>(
        metadata.updated().time_since_epoch()).count()},
    {"tags", metadata.tags()}
  };
  
  // Optional fields
  if (note.notebook().has_value()) {
    note_json["notebook"] = note.notebook().value();
  }
  
  return note_json;
}

} // namespace nx::import_export