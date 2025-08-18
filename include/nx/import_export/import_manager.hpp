#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <map>

#include <nlohmann/json.hpp>

#include "nx/common.hpp"
#include "nx/core/note.hpp"
#include "nx/core/note_id.hpp"
#include "nx/store/note_store.hpp"

namespace nx::import_export {

class ImportManager {
public:
  struct ImportOptions {
    std::filesystem::path source_dir;
    std::string target_notebook = "imported";
    bool recursive = true;
    std::vector<std::string> extensions = {"md", "txt", "markdown"};
    bool preserve_structure = true;  // Create notebooks from directory structure
    bool overwrite_existing = false;
    bool skip_hidden = true;
  };
  
  struct ImportResult {
    size_t notes_imported = 0;
    size_t files_skipped = 0;
    size_t files_failed = 0;
    std::vector<std::string> errors;
    std::vector<nx::core::NoteId> created_notes;
    std::map<std::string, size_t> notebooks_created; // notebook -> note count
  };
  
  explicit ImportManager(nx::store::NoteStore& note_store);
  
  // Main import methods
  Result<ImportResult> importDirectory(const ImportOptions& options);
  Result<ImportResult> importFile(const std::filesystem::path& file_path, 
                                  const std::string& notebook = "");
  
  // Specialized importers
  Result<ImportResult> importObsidianVault(const std::filesystem::path& vault_path);
  Result<ImportResult> importNotionExport(const std::filesystem::path& export_path);

private:
  nx::store::NoteStore& note_store_;
  
  // File processing
  Result<nx::core::Note> parseMarkdownFile(const std::filesystem::path& file);
  Result<nx::core::Note> parseTextFile(const std::filesystem::path& file);
  Result<nx::core::Note> parseObsidianNote(const std::filesystem::path& file);
  
  // Helper methods
  bool shouldImportFile(const std::filesystem::path& file, const ImportOptions& options);
  std::string inferNotebook(const std::filesystem::path& file, 
                           const std::filesystem::path& root_dir,
                           const ImportOptions& options);
  std::string filenameToTitle(const std::filesystem::path& filename);
  std::string sanitizeFilename(const std::string& filename);
  
  // Front-matter parsing
  struct ParsedContent {
    std::map<std::string, std::string> metadata;
    std::vector<std::string> tags;
    std::string content;
    bool has_frontmatter = false;
  };
  
  ParsedContent parseYamlFrontMatter(const std::string& raw_content);
  
  // Time utilities
  std::chrono::system_clock::time_point fileTimeToSystemTime(
    const std::filesystem::file_time_type& file_time);
};

// Specialized importer classes
class ObsidianImporter {
public:
  explicit ObsidianImporter(nx::store::NoteStore& note_store);
  
  Result<ImportManager::ImportResult> importVault(const std::filesystem::path& vault_path);
  
private:
  nx::store::NoteStore& note_store_;
  
  Result<std::string> convertWikiLinks(const std::string& content);
};

class NotionImporter {
public:
  explicit NotionImporter(nx::store::NoteStore& note_store);
  
  Result<ImportManager::ImportResult> importExport(const std::filesystem::path& export_path);
  
private:
  nx::store::NoteStore& note_store_;
  
  // JSON format parsing methods
  Result<ImportManager::ImportResult> importFromJsonFormat(
      const nlohmann::json& json_data, 
      const std::filesystem::path& source_path);
  
  Result<ImportManager::ImportResult> processJsonItem(
      const nlohmann::json& item,
      const std::filesystem::path& source_path);
  
  std::string extractContentFromJson(const nlohmann::json& content_json);
  std::string extractContentFromBlocks(const nlohmann::json& blocks_json);
  std::string extractContentFromProperties(const nlohmann::json& properties_json);
};

} // namespace nx::import_export