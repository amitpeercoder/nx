#include "nx/import_export/import_manager.hpp"

#include <fstream>
#include <regex>
#include <sstream>
#include <algorithm>
#include <expected>
#include <ctime>

#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>

#include "nx/store/note_store.hpp"
#include "nx/util/time.hpp"

namespace nx::import_export {

ImportManager::ImportManager(nx::store::NoteStore& note_store) : note_store_(note_store) {
}

Result<ImportManager::ImportResult> ImportManager::importDirectory(const ImportOptions& options) {
  ImportResult result;
  
  if (!std::filesystem::exists(options.source_dir)) {
    return std::unexpected(makeError(ErrorCode::kFileNotFound, 
                     "Source directory does not exist: " + options.source_dir.string()));
  }
  
  if (!std::filesystem::is_directory(options.source_dir)) {
    return std::unexpected(makeError(ErrorCode::kFileError, 
                     "Source path is not a directory: " + options.source_dir.string()));
  }
  
  try {
    // Iterate through directory
    if (options.recursive) {
      std::filesystem::recursive_directory_iterator iterator(options.source_dir);
      for (const auto& entry : iterator) {
        if (!entry.is_regular_file()) {
          continue;
        }
        
        const auto& file_path = entry.path();
        
        // Check if we should skip this file
        if (!shouldImportFile(file_path, options)) {
          result.files_skipped++;
          continue;
        }
        
        // Determine notebook for this file
        std::string notebook = inferNotebook(file_path, options.source_dir, options);
        
        // Import the file
        auto import_file_result = importFile(file_path, notebook);
        if (import_file_result.has_value()) {
          auto file_result = import_file_result.value();
          result.notes_imported += file_result.notes_imported;
          result.created_notes.insert(result.created_notes.end(), 
                                     file_result.created_notes.begin(),
                                     file_result.created_notes.end());
          
          // Track notebooks created
          if (!notebook.empty()) {
            result.notebooks_created[notebook]++;
          }
        } else {
          result.files_failed++;
          result.errors.push_back("Failed to import " + file_path.string() + ": " + 
                                 import_file_result.error().message());
        }
      }
    } else {
      std::filesystem::directory_iterator iterator(options.source_dir);
      for (const auto& entry : iterator) {
        if (!entry.is_regular_file()) {
          continue;
        }
        
        const auto& file_path = entry.path();
        
        // Check if we should skip this file
        if (!shouldImportFile(file_path, options)) {
          result.files_skipped++;
          continue;
        }
        
        // Determine notebook for this file
        std::string notebook = inferNotebook(file_path, options.source_dir, options);
        
        // Import the file
        auto import_file_result = importFile(file_path, notebook);
        if (import_file_result.has_value()) {
          auto file_result = import_file_result.value();
          result.notes_imported += file_result.notes_imported;
          result.created_notes.insert(result.created_notes.end(), 
                                     file_result.created_notes.begin(),
                                     file_result.created_notes.end());
          
          // Track notebooks created
          if (!notebook.empty()) {
            result.notebooks_created[notebook]++;
          }
        } else {
          result.files_failed++;
          result.errors.push_back("Failed to import " + file_path.string() + ": " + 
                                 import_file_result.error().message());
        }
      }
    }
    
    return result;
    
  } catch (const std::filesystem::filesystem_error& e) {
    return std::unexpected(makeError(ErrorCode::kFileError, 
                     "Filesystem error during import: " + std::string(e.what())));
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kUnknownError, 
                     "Unexpected error during import: " + std::string(e.what())));
  }
}

Result<ImportManager::ImportResult> ImportManager::importFile(
    const std::filesystem::path& file_path, 
    const std::string& notebook) {
  
  ImportResult result;
  
  if (!std::filesystem::exists(file_path)) {
    return std::unexpected(makeError(ErrorCode::kFileNotFound, 
                     "File does not exist: " + file_path.string()));
  }
  
  try {
    // Parse the file based on extension
    auto extension = file_path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    std::optional<Result<nx::core::Note>> note_result;
    
    if (extension == ".md" || extension == ".markdown") {
      note_result = parseMarkdownFile(file_path);
    } else if (extension == ".txt") {
      note_result = parseTextFile(file_path);
    } else {
      return std::unexpected(makeError(ErrorCode::kValidationError, 
                       "Unsupported file type: " + extension));
    }
    
    if (!note_result || !note_result->has_value()) {
      return std::unexpected(note_result ? note_result->error() : makeError(ErrorCode::kUnknownError, "Failed to parse file"));
    }
    
    auto note = note_result->value();
    
    // Set notebook if provided
    if (!notebook.empty()) {
      note.setNotebook(notebook);
    }
    
    // Store the note
    auto store_result = note_store_.store(note);
    if (!store_result.has_value()) {
      return std::unexpected(store_result.error());
    }
    
    result.notes_imported = 1;
    result.created_notes.push_back(note.id());
    
    return result;
    
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kUnknownError, 
                     "Error importing file: " + std::string(e.what())));
  }
}

Result<nx::core::Note> ImportManager::parseMarkdownFile(const std::filesystem::path& file) {
  std::ifstream stream(file);
  if (!stream.is_open()) {
    return std::unexpected(makeError(ErrorCode::kFileReadError, 
                     "Cannot open file: " + file.string()));
  }
  
  // Read entire file
  std::string content((std::istreambuf_iterator<char>(stream)),
                      std::istreambuf_iterator<char>());
  stream.close();
  
  // Parse YAML front-matter if present
  auto parsed = parseYamlFrontMatter(content);
  
  // Create new note
  auto note = nx::core::Note::create(
    parsed.metadata.count("title") ? parsed.metadata["title"] : filenameToTitle(file),
    parsed.content
  );
  
  // Set metadata from front-matter
  if (!parsed.tags.empty()) {
    note.setTags(parsed.tags);
  }
  
  if (parsed.metadata.count("notebook")) {
    note.setNotebook(parsed.metadata["notebook"]);
  }
  
  // Try to preserve file timestamps
  try {
    auto file_time = std::filesystem::last_write_time(file);
    auto system_time = fileTimeToSystemTime(file_time);
    note.metadata().setCreated(system_time);
    note.metadata().setUpdated(system_time);
  } catch (...) {
    // Ignore timestamp errors - use current time
  }
  
  return note;
}

Result<nx::core::Note> ImportManager::parseTextFile(const std::filesystem::path& file) {
  std::ifstream stream(file);
  if (!stream.is_open()) {
    return std::unexpected(makeError(ErrorCode::kFileReadError, 
                     "Cannot open file: " + file.string()));
  }
  
  // Read entire file
  std::string content((std::istreambuf_iterator<char>(stream)),
                      std::istreambuf_iterator<char>());
  stream.close();
  
  // Create note with filename as title
  auto note = nx::core::Note::create(filenameToTitle(file), content);
  
  // Try to preserve file timestamps
  try {
    auto file_time = std::filesystem::last_write_time(file);
    auto system_time = fileTimeToSystemTime(file_time);
    note.metadata().setCreated(system_time);
    note.metadata().setUpdated(system_time);
  } catch (...) {
    // Ignore timestamp errors
  }
  
  return note;
}

bool ImportManager::shouldImportFile(const std::filesystem::path& file, 
                                     const ImportOptions& options) {
  // Skip hidden files if requested
  if (options.skip_hidden && file.filename().string()[0] == '.') {
    return false;
  }
  
  // Check extension
  auto extension = file.extension().string();
  if (!extension.empty() && extension[0] == '.') {
    extension = extension.substr(1);  // Remove leading dot
  }
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
  
  return std::find(options.extensions.begin(), options.extensions.end(), extension) != 
         options.extensions.end();
}

std::string ImportManager::inferNotebook(const std::filesystem::path& file,
                                         const std::filesystem::path& root_dir,
                                         const ImportOptions& options) {
  if (!options.preserve_structure) {
    return options.target_notebook;
  }
  
  // Get relative path from root
  auto relative = std::filesystem::relative(file, root_dir);
  
  // If file is directly in root, use target notebook
  if (relative.parent_path().empty() || relative.parent_path() == ".") {
    return options.target_notebook;
  }
  
  // Use first directory as notebook name
  auto notebook = relative.begin()->string();
  
  // Sanitize notebook name
  return sanitizeFilename(notebook);
}

std::string ImportManager::filenameToTitle(const std::filesystem::path& filename) {
  auto stem = filename.stem().string();
  
  // Replace underscores and hyphens with spaces
  std::replace(stem.begin(), stem.end(), '_', ' ');
  std::replace(stem.begin(), stem.end(), '-', ' ');
  
  // Capitalize first letter
  if (!stem.empty()) {
    stem[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(stem[0])));
  }
  
  return stem;
}

std::string ImportManager::sanitizeFilename(const std::string& filename) {
  std::string result;
  result.reserve(filename.length());
  
  for (char c : filename) {
    if (std::isalnum(c) || c == '-' || c == '_') {
      result += c;
    } else if (std::isspace(c)) {
      result += '_';
    }
  }
  
  return result;
}

ImportManager::ParsedContent ImportManager::parseYamlFrontMatter(const std::string& raw_content) {
  ParsedContent result;
  result.content = raw_content;
  
  // Check for YAML front-matter (must start with ---)
  if (raw_content.size() < 4 || raw_content.substr(0, 3) != "---") {
    return result;
  }
  
  // Find end of front-matter
  auto end_pos = raw_content.find("\n---", 3);
  if (end_pos == std::string::npos) {
    // Try with \r\n
    end_pos = raw_content.find("\r\n---", 3);
    if (end_pos == std::string::npos) {
      return result;
    }
    end_pos += 1; // Account for \r
  }
  
  try {
    // Extract and parse YAML
    std::string yaml_str = raw_content.substr(4, end_pos - 4);  // Skip initial ---
    YAML::Node yaml = YAML::Load(yaml_str);
    
    result.has_frontmatter = true;
    
    // Extract common fields
    if (yaml["title"]) {
      result.metadata["title"] = yaml["title"].as<std::string>();
    }
    
    if (yaml["notebook"]) {
      result.metadata["notebook"] = yaml["notebook"].as<std::string>();
    }
    
    if (yaml["tags"]) {
      if (yaml["tags"].IsSequence()) {
        for (const auto& tag : yaml["tags"]) {
          result.tags.push_back(tag.as<std::string>());
        }
      }
    }
    
    // Extract content after front-matter
    auto content_start = end_pos + 4; // Skip \n---
    if (content_start < raw_content.length()) {
      result.content = raw_content.substr(content_start);
      // Trim leading newlines
      while (!result.content.empty() && (result.content[0] == '\n' || result.content[0] == '\r')) {
        result.content = result.content.substr(1);
      }
    } else {
      result.content.clear();
    }
    
  } catch (const YAML::Exception& e) {
    // If YAML parsing fails, treat as regular content
    result.has_frontmatter = false;
    result.metadata.clear();
    result.tags.clear();
  }
  
  return result;
}

std::chrono::system_clock::time_point ImportManager::fileTimeToSystemTime(
    const std::filesystem::file_time_type& file_time) {
  // Convert filesystem time to system time
  // This is a simplified conversion - may need adjustment for different systems
  auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
    file_time - std::filesystem::file_time_type::clock::now() + 
    std::chrono::system_clock::now()
  );
  return sctp;
}

// Specialized importers would be implemented here
// For now, providing basic stubs

ObsidianImporter::ObsidianImporter(nx::store::NoteStore& note_store) 
  : note_store_(note_store) {
}

Result<ImportManager::ImportResult> ObsidianImporter::importVault(
    const std::filesystem::path& vault_path) {
  if (!std::filesystem::exists(vault_path) || !std::filesystem::is_directory(vault_path)) {
    return std::unexpected(makeError(ErrorCode::kFileNotFound, "Obsidian vault not found: " + vault_path.string()));
  }

  ImportManager manager(note_store_);
  ImportManager::ImportOptions options;
  options.source_dir = vault_path;
  options.extensions = {"md", "canvas"}; // Include Obsidian canvas files
  options.target_notebook = "obsidian-" + vault_path.filename().string();
  options.preserve_structure = true;
  options.skip_hidden = false; // Obsidian may have important dotfiles
  
  // Import the vault using the standard directory import
  // This handles the basic file structure and YAML front-matter
  auto result = manager.importDirectory(options);
  if (!result.has_value()) {
    return result;
  }

  auto import_result = result.value();
  
  // Post-process for Obsidian-specific features
  // Convert [[wiki-links]] in imported notes
  if (!import_result.created_notes.empty()) {
    for (const auto& note_id : import_result.created_notes) {
      auto note_result = note_store_.load(note_id);
      if (note_result.has_value()) {
        auto note = note_result.value();
        auto converted_content = convertWikiLinks(note.content());
        if (converted_content.has_value() && converted_content.value() != note.content()) {
          note.setContent(converted_content.value());
          // Save the updated note (ignore errors to not fail the import)
          note_store_.store(note);
        }
      }
    }
  }
  
  // Future enhancements could include:
  // - Handle Obsidian templates and plugins
  // - Process .obsidian config directory for settings
  // - Import canvas files as structured notes
  // - Convert Obsidian-specific syntax (callouts, etc.)
  
  return import_result;
}

Result<std::string> ObsidianImporter::convertWikiLinks(const std::string& content) {
  std::string result = content;
  
  // Convert [[Page Name]] to [Page Name](Page Name.md)
  // This is a basic conversion - more sophisticated link resolution could be added
  std::regex wiki_link_regex(R"(\[\[([^\]]+)\]\])");
  
  result = std::regex_replace(result, wiki_link_regex, "[$1]($1.md)");
  
  // Handle [[Page Name|Display Text]] format
  std::regex wiki_link_alias_regex(R"(\[\[([^\|]+)\|([^\]]+)\]\])");
  result = std::regex_replace(result, wiki_link_alias_regex, "[$2]($1.md)");
  
  return result;
}

NotionImporter::NotionImporter(nx::store::NoteStore& note_store) 
  : note_store_(note_store) {
}

Result<ImportManager::ImportResult> NotionImporter::importExport(
    const std::filesystem::path& export_path) {
  if (!std::filesystem::exists(export_path)) {
    return std::unexpected(makeError(ErrorCode::kFileNotFound, "Notion export not found: " + export_path.string()));
  }

  ImportManager::ImportResult result;
  
  try {
    if (std::filesystem::is_directory(export_path)) {
      // Handle directory export (HTML + CSV format)
      ImportManager manager(note_store_);
      ImportManager::ImportOptions options;
      options.source_dir = export_path;
      options.extensions = {"md", "html", "txt"};
      options.target_notebook = "notion-" + export_path.filename().string();
      options.preserve_structure = true;
      
      auto dir_result = manager.importDirectory(options);
      if (dir_result.has_value()) {
        result = dir_result.value();
      } else {
        return dir_result;
      }
    } else {
      // Handle single file export
      auto extension = export_path.extension().string();
      std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
      
      if (extension == ".zip") {
        // Extract ZIP file to temporary directory and import
        auto temp_dir = std::filesystem::temp_directory_path() / ("nx_notion_extract_" + std::to_string(std::time(nullptr)));
        
        std::error_code ec;
        if (!std::filesystem::create_directories(temp_dir, ec) && ec) {
          result.files_failed = 1;
          result.errors.push_back("Failed to create temporary directory for ZIP extraction: " + ec.message());
        } else {
          // Use system unzip command for extraction
          std::ostringstream unzip_cmd;
          unzip_cmd << "unzip -q \"" << export_path.string() << "\" -d \"" << temp_dir.string() << "\"";
          
          int unzip_result = std::system(unzip_cmd.str().c_str());
          if (unzip_result != 0) {
            result.files_failed = 1;
            result.errors.push_back("Failed to extract ZIP file. Please ensure 'unzip' command is available.");
            std::filesystem::remove_all(temp_dir, ec); // Cleanup
          } else {
            // Recursively import the extracted contents
            ImportManager manager(note_store_);
            ImportManager::ImportOptions options;
            options.source_dir = temp_dir;
            options.extensions = {"md", "html", "txt", "csv"};
            options.target_notebook = "notion-" + export_path.stem().string();
            options.preserve_structure = true;
            
            auto extract_result = manager.importDirectory(options);
            if (extract_result.has_value()) {
              result = extract_result.value();
              result.notes_imported += 0; // Don't double-count
            } else {
              result.files_failed = 1;
              result.errors.push_back("Failed to import extracted ZIP contents: " + extract_result.error().message());
            }
            
            // Cleanup extracted files
            std::filesystem::remove_all(temp_dir, ec);
          }
        }
      } else if (extension == ".json") {
        // Parse JSON export file
        try {
          std::ifstream json_file(export_path);
          if (!json_file) {
            result.files_failed = 1;
            result.errors.push_back("Failed to open JSON export file");
          } else {
            nlohmann::json json_data;
            json_file >> json_data;
            
            // Handle different JSON export formats
            auto json_result = importFromJsonFormat(json_data, export_path);
            if (json_result.has_value()) {
              result = json_result.value();
            } else {
              result.files_failed = 1;
              result.errors.push_back("Failed to parse JSON export: " + json_result.error().message());
            }
          }
        } catch (const std::exception& e) {
          result.files_failed = 1;
          result.errors.push_back("JSON parsing error: " + std::string(e.what()));
        }
      } else {
        // Try to import as regular file
        ImportManager manager(note_store_);
        auto file_result = manager.importFile(export_path, "notion-import");
        if (file_result.has_value()) {
          result = file_result.value();
        } else {
          return file_result;
        }
      }
    }
    
    return result;
    
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kFileError, 
                         "Error importing Notion export: " + std::string(e.what())));
  }
}

Result<ImportManager::ImportResult> NotionImporter::importFromJsonFormat(
    const nlohmann::json& json_data, 
    const std::filesystem::path& source_path) {
  
  ImportManager::ImportResult result;
  
  try {
    // Handle different Notion JSON export formats
    if (json_data.is_array()) {
      // Array format - multiple notes/pages
      for (const auto& item : json_data) {
        auto item_result = processJsonItem(item, source_path);
        if (item_result.has_value()) {
          result.notes_imported += item_result.value().notes_imported;
          result.files_skipped += item_result.value().files_skipped;
          result.files_failed += item_result.value().files_failed;
          result.errors.insert(result.errors.end(), 
                              item_result.value().errors.begin(), 
                              item_result.value().errors.end());
        } else {
          result.files_failed++;
          result.errors.push_back("Failed to process JSON item: " + item_result.error().message());
        }
      }
    } else if (json_data.is_object()) {
      // Single object format
      auto item_result = processJsonItem(json_data, source_path);
      if (item_result.has_value()) {
        result = item_result.value();
      } else {
        result.files_failed = 1;
        result.errors.push_back("Failed to process JSON object: " + item_result.error().message());
      }
    } else {
      result.files_failed = 1;
      result.errors.push_back("Unsupported JSON format - expected object or array");
    }
    
  } catch (const std::exception& e) {
    result.files_failed = 1;
    result.errors.push_back("JSON processing error: " + std::string(e.what()));
  }
  
  return result;
}

Result<ImportManager::ImportResult> NotionImporter::processJsonItem(
    const nlohmann::json& item, 
    const std::filesystem::path& source_path) {
  
  ImportManager::ImportResult result;
  
  try {
    // Extract common Notion fields
    std::string title = "Untitled";
    std::string content = "";
    std::vector<std::string> tags;
    
    // Look for title in common Notion export fields
    if (item.contains("title")) {
      if (item["title"].is_string()) {
        title = item["title"];
      } else if (item["title"].is_array() && !item["title"].empty()) {
        // Notion rich text format
        if (item["title"][0].contains("plain_text")) {
          title = item["title"][0]["plain_text"];
        }
      }
    } else if (item.contains("Name") && item["Name"].is_string()) {
      title = item["Name"];
    }
    
    // Look for content in various fields
    if (item.contains("content")) {
      content = extractContentFromJson(item["content"]);
    } else if (item.contains("blocks")) {
      content = extractContentFromBlocks(item["blocks"]);
    } else if (item.contains("properties")) {
      content = extractContentFromProperties(item["properties"]);
    }
    
    // Extract tags if present
    if (item.contains("tags") && item["tags"].is_array()) {
      for (const auto& tag : item["tags"]) {
        if (tag.is_string()) {
          tags.push_back(tag);
        } else if (tag.contains("name") && tag["name"].is_string()) {
          tags.push_back(tag["name"]);
        }
      }
    }
    
    // Create the note
    if (!title.empty() && !content.empty()) {
      ImportManager manager(note_store_);
      
      // Prepare markdown content with frontmatter
      std::ostringstream note_content;
      note_content << "---\n";
      note_content << "title: \"" << title << "\"\n";
      note_content << "imported_from: notion_json\n";
      note_content << "source_file: \"" << source_path.filename().string() << "\"\n";
      if (!tags.empty()) {
        note_content << "tags: [";
        for (size_t i = 0; i < tags.size(); ++i) {
          note_content << "\"" << tags[i] << "\"";
          if (i < tags.size() - 1) note_content << ", ";
        }
        note_content << "]\n";
      }
      note_content << "---\n\n";
      note_content << "# " << title << "\n\n";
      note_content << content;
      
      // Create temporary file with the content
      auto temp_file = std::filesystem::temp_directory_path() / 
                       ("nx_notion_json_" + std::to_string(std::time(nullptr)) + ".md");
      
      std::ofstream temp_out(temp_file);
      if (!temp_out) {
        result.files_failed = 1;
        result.errors.push_back("Failed to create temporary file for note import");
        return result;
      }
      
      temp_out << note_content.str();
      temp_out.close();
      
      auto import_result = manager.importFile(temp_file, "notion-json-import");
      
      // Cleanup temporary file
      std::error_code ec;
      std::filesystem::remove(temp_file, ec);
      
      if (import_result.has_value()) {
        result.notes_imported = 1;
      } else {
        result.files_failed = 1;
        result.errors.push_back("Failed to create note: " + import_result.error().message());
      }
    } else {
      result.files_failed = 1;
      result.errors.push_back("Could not extract title or content from JSON item");
    }
    
  } catch (const std::exception& e) {
    result.files_failed = 1;
    result.errors.push_back("Error processing JSON item: " + std::string(e.what()));
  }
  
  return result;
}

std::string NotionImporter::extractContentFromJson(const nlohmann::json& content_json) {
  std::ostringstream content;
  
  if (content_json.is_string()) {
    content << content_json.get<std::string>();
  } else if (content_json.is_array()) {
    for (const auto& item : content_json) {
      if (item.contains("plain_text")) {
        content << item["plain_text"].get<std::string>() << "\n";
      } else if (item.is_string()) {
        content << item.get<std::string>() << "\n";
      }
    }
  } else if (content_json.is_object()) {
    if (content_json.contains("plain_text")) {
      content << content_json["plain_text"];
    } else if (content_json.contains("text")) {
      content << content_json["text"];
    }
  }
  
  return content.str();
}

std::string NotionImporter::extractContentFromBlocks(const nlohmann::json& blocks_json) {
  std::ostringstream content;
  
  if (blocks_json.is_array()) {
    for (const auto& block : blocks_json) {
      if (block.contains("paragraph") && block["paragraph"].contains("rich_text")) {
        for (const auto& text : block["paragraph"]["rich_text"]) {
          if (text.contains("plain_text")) {
            content << text["plain_text"];
          }
        }
        content << "\n\n";
      } else if (block.contains("heading_1") && block["heading_1"].contains("rich_text")) {
        content << "# ";
        for (const auto& text : block["heading_1"]["rich_text"]) {
          if (text.contains("plain_text")) {
            content << text["plain_text"];
          }
        }
        content << "\n\n";
      } else if (block.contains("heading_2") && block["heading_2"].contains("rich_text")) {
        content << "## ";
        for (const auto& text : block["heading_2"]["rich_text"]) {
          if (text.contains("plain_text")) {
            content << text["plain_text"];
          }
        }
        content << "\n\n";
      }
      // Add more block types as needed
    }
  }
  
  return content.str();
}

std::string NotionImporter::extractContentFromProperties(const nlohmann::json& properties_json) {
  std::ostringstream content;
  
  for (const auto& [key, value] : properties_json.items()) {
    if (value.is_string()) {
      content << "**" << key << "**: " << value.get<std::string>() << "\n";
    } else if (value.contains("rich_text") && value["rich_text"].is_array()) {
      content << "**" << key << "**: ";
      for (const auto& text : value["rich_text"]) {
        if (text.contains("plain_text")) {
          content << text["plain_text"];
        }
      }
      content << "\n";
    }
  }
  
  return content.str();
}

} // namespace nx::import_export