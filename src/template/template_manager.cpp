#include "nx/template/template_manager.hpp"

#include <fstream>
#include <regex>
#include <sstream>
#include <algorithm>
#include <iomanip>

#include <yaml-cpp/yaml.h>
#include <nlohmann/json.hpp>

#include "nx/util/time.hpp"
#include "nx/util/filesystem.hpp"

namespace nx::template_system {

TemplateManager::TemplateManager(const Config& config) : config_(config) {
}

Result<void> TemplateManager::createTemplate(const std::string& name, 
                                           const std::string& content,
                                           const std::string& description,
                                           const std::string& category) {
  if (name.empty()) {
    return std::unexpected(makeError(ErrorCode::kValidationError, "Template name cannot be empty"));
  }

  auto sanitized_name = sanitizeTemplateName(name);
  auto template_path = getTemplatePath(sanitized_name);
  
  // Check if template already exists
  if (std::filesystem::exists(template_path)) {
    return std::unexpected(makeError(ErrorCode::kFileError, "Template already exists: " + name));
  }

  auto ensure_result = ensureDirectoryExists();
  if (!ensure_result.has_value()) {
    return ensure_result;
  }

  // Validate template content
  auto validate_result = validateTemplate(content);
  if (!validate_result.has_value()) {
    return validate_result;
  }

  try {
    // Write template file atomically
    auto write_result = nx::util::FileSystem::writeFileAtomic(template_path, content);
    if (!write_result.has_value()) {
      return std::unexpected(makeError(ErrorCode::kFileWriteError, 
                           "Cannot create template file: " + write_result.error().message()));
    }

    // Update template info
    TemplateInfo info;
    info.name = sanitized_name;
    info.description = description;
    info.category = category.empty() ? "default" : category;
    info.variables = extractVariables(content);
    info.created = std::chrono::system_clock::now();
    info.modified = info.created;
    info.file_path = template_path;

    // Add to cache
    template_cache_[sanitized_name] = info;
    cache_valid_ = true;

    // Save metadata
    return saveMetadata();

  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kFileWriteError, 
                         "Error creating template: " + std::string(e.what())));
  }
}

Result<std::string> TemplateManager::getTemplate(const std::string& name) {
  auto template_path = getTemplatePath(name);
  
  if (!std::filesystem::exists(template_path)) {
    return std::unexpected(makeError(ErrorCode::kFileNotFound, "Template not found: " + name));
  }

  try {
    std::ifstream file(template_path);
    if (!file.is_open()) {
      return std::unexpected(makeError(ErrorCode::kFileReadError, 
                           "Cannot read template file: " + template_path.string()));
    }

    std::string content((std::istreambuf_iterator<char>(file)),
                        std::istreambuf_iterator<char>());
    return content;

  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kFileReadError, 
                         "Error reading template: " + std::string(e.what())));
  }
}

Result<TemplateInfo> TemplateManager::getTemplateInfo(const std::string& name) {
  if (!cache_valid_) {
    auto load_result = loadTemplateCache();
    if (!load_result.has_value()) {
      return std::unexpected(load_result.error());
    }
  }

  auto it = template_cache_.find(name);
  if (it == template_cache_.end()) {
    return std::unexpected(makeError(ErrorCode::kNotFound, "Template not found: " + name));
  }

  return it->second;
}

Result<std::vector<TemplateInfo>> TemplateManager::listTemplates(const std::string& category) {
  if (!cache_valid_) {
    auto load_result = loadTemplateCache();
    if (!load_result.has_value()) {
      return std::unexpected(load_result.error());
    }
  }

  std::vector<TemplateInfo> templates;
  
  for (const auto& [name, info] : template_cache_) {
    if (category.empty() || info.category == category) {
      templates.push_back(info);
    }
  }

  // Sort by name
  std::sort(templates.begin(), templates.end(), 
           [](const TemplateInfo& a, const TemplateInfo& b) {
             return a.name < b.name;
           });

  return templates;
}

Result<void> TemplateManager::updateTemplate(const std::string& name, const std::string& content) {
  auto template_path = getTemplatePath(name);
  
  if (!std::filesystem::exists(template_path)) {
    return std::unexpected(makeError(ErrorCode::kFileNotFound, "Template not found: " + name));
  }

  // Validate template content
  auto validate_result = validateTemplate(content);
  if (!validate_result.has_value()) {
    return validate_result;
  }

  try {
    // Write updated template atomically
    auto write_result = nx::util::FileSystem::writeFileAtomic(template_path, content);
    if (!write_result.has_value()) {
      return std::unexpected(makeError(ErrorCode::kFileWriteError, 
                           "Cannot update template file: " + write_result.error().message()));
    }

    // Update cache
    auto it = template_cache_.find(name);
    if (it != template_cache_.end()) {
      it->second.variables = extractVariables(content);
      it->second.modified = std::chrono::system_clock::now();
    }

    return saveMetadata();

  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kFileWriteError, 
                         "Error updating template: " + std::string(e.what())));
  }
}

Result<void> TemplateManager::deleteTemplate(const std::string& name) {
  auto template_path = getTemplatePath(name);
  
  if (!std::filesystem::exists(template_path)) {
    return std::unexpected(makeError(ErrorCode::kFileNotFound, "Template not found: " + name));
  }

  try {
    std::filesystem::remove(template_path);
    
    // Remove from cache
    template_cache_.erase(name);
    
    return saveMetadata();

  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kFileError, 
                         "Error deleting template: " + std::string(e.what())));
  }
}

Result<TemplateResult> TemplateManager::processTemplate(const std::string& name, 
                                                       const VariableMap& variables) {
  auto template_result = getTemplate(name);
  if (!template_result.has_value()) {
    return std::unexpected(template_result.error());
  }

  // Merge with default variables
  VariableMap merged_vars = getDefaultVariables();
  for (const auto& [key, value] : variables) {
    merged_vars[key] = value;
  }

  // Process template content
  std::string processed_content = processVariables(template_result.value(), merged_vars);
  
  TemplateResult result;
  result.content = processed_content;
  
  // Extract title from variables or content
  if (merged_vars.count("title")) {
    result.title = merged_vars["title"];
  } else {
    // Try to extract title from first heading
    std::regex heading_regex(R"(^#\s+(.+)$)");
    std::smatch match;
    if (std::regex_search(processed_content, match, heading_regex)) {
      result.title = match[1].str();
    } else {
      result.title = "New Note from Template";
    }
  }
  
  // Extract tags from variables
  if (merged_vars.count("tags")) {
    std::string tags_str = merged_vars["tags"];
    std::regex tag_regex(R"(\w+)");
    std::sregex_iterator iter(tags_str.begin(), tags_str.end(), tag_regex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
      result.tags.push_back(iter->str());
    }
  }
  
  // Extract notebook from variables
  if (merged_vars.count("notebook")) {
    result.notebook = merged_vars["notebook"];
  }
  
  // Store all variables as metadata
  result.metadata = merged_vars;
  
  return result;
}

Result<nx::core::Note> TemplateManager::createNoteFromTemplate(const std::string& template_name,
                                                              const VariableMap& variables) {
  auto result = processTemplate(template_name, variables);
  if (!result.has_value()) {
    return std::unexpected(result.error());
  }

  auto template_result = result.value();
  
  // Create note
  auto note = nx::core::Note::create(template_result.title, template_result.content);
  
  // Set tags if provided
  if (!template_result.tags.empty()) {
    note.setTags(template_result.tags);
  }
  
  // Set notebook if provided
  if (template_result.notebook.has_value()) {
    note.setNotebook(template_result.notebook.value());
  }
  
  return note;
}

Result<std::vector<std::string>> TemplateManager::listCategories() {
  if (!cache_valid_) {
    auto load_result = loadTemplateCache();
    if (!load_result.has_value()) {
      return std::unexpected(load_result.error());
    }
  }

  std::set<std::string> categories;
  for (const auto& [name, info] : template_cache_) {
    categories.insert(info.category);
  }

  return std::vector<std::string>(categories.begin(), categories.end());
}

Result<void> TemplateManager::setTemplateCategory(const std::string& name, const std::string& category) {
  auto it = template_cache_.find(name);
  if (it == template_cache_.end()) {
    return std::unexpected(makeError(ErrorCode::kNotFound, "Template not found: " + name));
  }

  it->second.category = category;
  return saveMetadata();
}

Result<void> TemplateManager::installBuiltinTemplates() {
  auto builtin_templates = getBuiltinTemplates();
  
  for (const auto& tpl : builtin_templates) {
    // Only create if doesn't exist
    auto template_path = getTemplatePath(tpl.name);
    if (!std::filesystem::exists(template_path)) {
      auto create_result = createTemplate(tpl.name, tpl.content, tpl.description, tpl.category);
      if (!create_result.has_value()) {
        // Continue with other templates even if one fails
        continue;
      }
    }
  }

  return {};
}

Result<void> TemplateManager::validateTemplate(const std::string& content) {
  if (content.empty()) {
    return std::unexpected(makeError(ErrorCode::kValidationError, "Template content cannot be empty"));
  }

  // Check for unbalanced template variable braces
  int open_count = 0;
  for (size_t i = 0; i < content.length() - 1; ++i) {
    if (content[i] == '{' && content[i + 1] == '{') {
      open_count++;
      i++; // Skip next character
    } else if (content[i] == '}' && content[i + 1] == '}') {
      open_count--;
      i++; // Skip next character
      if (open_count < 0) {
        return std::unexpected(makeError(ErrorCode::kValidationError, 
                             "Template contains unmatched closing braces '}}'"));
      }
    }
  }
  
  if (open_count > 0) {
    return std::unexpected(makeError(ErrorCode::kValidationError, 
                         "Template contains unmatched opening braces '{{'"));
  }
  
  return {};
}

std::vector<std::string> TemplateManager::extractVariables(const std::string& content) {
  std::vector<std::string> variables;
  std::regex var_regex(R"(\{\{([^}]+)\}\})");
  
  std::sregex_iterator iter(content.begin(), content.end(), var_regex);
  std::sregex_iterator end;
  
  for (; iter != end; ++iter) {
    std::string var_name = iter->str(1);
    // Remove whitespace
    var_name.erase(std::remove_if(var_name.begin(), var_name.end(), ::isspace), var_name.end());
    
    if (std::find(variables.begin(), variables.end(), var_name) == variables.end()) {
      variables.push_back(var_name);
    }
  }
  
  return variables;
}

Result<std::vector<TemplateInfo>> TemplateManager::searchTemplates(const std::string& query) {
  auto list_result = listTemplates();
  if (!list_result.has_value()) {
    return std::unexpected(list_result.error());
  }

  std::vector<TemplateInfo> matches;
  std::string lower_query = query;
  std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);

  for (const auto& template_info : list_result.value()) {
    std::string lower_name = template_info.name;
    std::string lower_desc = template_info.description;
    std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    std::transform(lower_desc.begin(), lower_desc.end(), lower_desc.begin(), ::tolower);

    if (lower_name.find(lower_query) != std::string::npos ||
        lower_desc.find(lower_query) != std::string::npos) {
      matches.push_back(template_info);
    }
  }

  return matches;
}

// Private methods implementation

Result<void> TemplateManager::loadTemplateCache() {
  template_cache_.clear();

  if (!std::filesystem::exists(config_.templates_dir)) {
    // No templates directory yet - that's fine
    cache_valid_ = true;
    return {};
  }

  try {
    // Load all template files
    for (const auto& entry : std::filesystem::directory_iterator(config_.templates_dir)) {
      if (entry.is_regular_file() && entry.path().extension() == ".md") {
        auto info_result = loadTemplateInfo(entry.path());
        if (info_result.has_value()) {
          auto info = info_result.value();
          template_cache_[info.name] = info;
        }
      }
    }

    // Load metadata if exists
    if (std::filesystem::exists(config_.metadata_file)) {
      std::ifstream file(config_.metadata_file);
      if (file.is_open()) {
        nlohmann::json metadata_json;
        file >> metadata_json;

        for (auto& [name, template_json] : metadata_json.items()) {
          auto it = template_cache_.find(name);
          if (it != template_cache_.end()) {
            // Update with metadata
            if (template_json.contains("description")) {
              it->second.description = template_json["description"];
            }
            if (template_json.contains("category")) {
              it->second.category = template_json["category"];
            }
          }
        }
      }
    }

    cache_valid_ = true;
    return {};

  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kFileError, 
                         "Error loading template cache: " + std::string(e.what())));
  }
}

Result<void> TemplateManager::saveMetadata() {
  auto ensure_result = ensureDirectoryExists();
  if (!ensure_result.has_value()) {
    return ensure_result;
  }

  try {
    nlohmann::json metadata_json;
    
    for (const auto& [name, info] : template_cache_) {
      nlohmann::json template_json;
      template_json["description"] = info.description;
      template_json["category"] = info.category;
      template_json["variables"] = info.variables;
      template_json["created"] = std::chrono::duration_cast<std::chrono::seconds>(
        info.created.time_since_epoch()).count();
      template_json["modified"] = std::chrono::duration_cast<std::chrono::seconds>(
        info.modified.time_since_epoch()).count();
      
      metadata_json[name] = template_json;
    }

    // Write metadata atomically
    auto write_result = nx::util::FileSystem::writeFileAtomic(config_.metadata_file, metadata_json.dump(2));
    if (!write_result.has_value()) {
      return std::unexpected(makeError(ErrorCode::kFileWriteError, 
                           "Cannot write metadata file: " + write_result.error().message()));
    }
    return {};

  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kFileWriteError, 
                         "Error saving metadata: " + std::string(e.what())));
  }
}

Result<TemplateInfo> TemplateManager::loadTemplateInfo(const std::filesystem::path& template_file) {
  TemplateInfo info;
  info.file_path = template_file;
  info.name = template_file.stem().string();
  
  try {
    // Get file timestamps
    auto file_time = std::filesystem::last_write_time(template_file);
    // Convert to system_clock time (simplified)
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
      file_time - std::filesystem::file_time_type::clock::now() + 
      std::chrono::system_clock::now()
    );
    info.created = sctp;
    info.modified = sctp;

    // Read template content to extract variables
    std::ifstream file(template_file);
    if (file.is_open()) {
      std::string content((std::istreambuf_iterator<char>(file)),
                          std::istreambuf_iterator<char>());
      info.variables = extractVariables(content);
    }

    info.category = "default";
    info.description = "";

    return info;

  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kFileError, 
                         "Error loading template info: " + std::string(e.what())));
  }
}

std::filesystem::path TemplateManager::getTemplatePath(const std::string& name) const {
  return config_.templates_dir / (name + ".md");
}

std::string TemplateManager::processVariables(const std::string& content, const VariableMap& variables) {
  std::string result = content;
  
  for (const auto& [var_name, var_value] : variables) {
    std::string pattern = "{{" + var_name + "}}";
    size_t pos = 0;
    while ((pos = result.find(pattern, pos)) != std::string::npos) {
      result.replace(pos, pattern.length(), var_value);
      pos += var_value.length();
    }
    
    // Also handle with spaces
    std::string spaced_pattern = "{{ " + var_name + " }}";
    pos = 0;
    while ((pos = result.find(spaced_pattern, pos)) != std::string::npos) {
      result.replace(pos, spaced_pattern.length(), var_value);
      pos += var_value.length();
    }
  }
  
  return result;
}

VariableMap TemplateManager::getDefaultVariables() {
  VariableMap defaults;
  
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  
  // Date variables
  std::stringstream ss;
  ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d");
  defaults["date"] = ss.str();
  
  ss.str("");
  ss << std::put_time(std::localtime(&time_t), "%Y-%m-%d %H:%M");
  defaults["datetime"] = ss.str();
  
  ss.str("");
  ss << std::put_time(std::localtime(&time_t), "%H:%M");
  defaults["time"] = ss.str();
  
  ss.str("");
  ss << std::put_time(std::localtime(&time_t), "%A, %B %d, %Y");
  defaults["date_full"] = ss.str();
  
  // System variables
  defaults["year"] = std::to_string(1900 + std::localtime(&time_t)->tm_year);
  defaults["month"] = std::to_string(1 + std::localtime(&time_t)->tm_mon);
  defaults["day"] = std::to_string(std::localtime(&time_t)->tm_mday);
  
  return defaults;
}

std::vector<TemplateManager::BuiltinTemplate> TemplateManager::getBuiltinTemplates() const {
  return {
    {
      "basic",
      R"(# {{title}}

{{content}}

Created: {{date}}
Tags: {{tags}})",
      "Basic note template with title and content",
      "basic"
    },
    {
      "meeting",
      R"(# Meeting: {{title}}

**Date:** {{date}}  
**Time:** {{time}}  
**Attendees:** {{attendees}}

## Agenda
{{agenda}}

## Discussion

## Action Items
- [ ] 

## Next Steps

---
Tags: {{tags}}, meeting)",
      "Meeting notes template",
      "work"
    },
    {
      "daily",
      R"(# Daily Note - {{date_full}}

## Today's Goals
- 

## Completed Tasks
- 

## Notes


## Tomorrow's Priorities
- 

---
Tags: daily, {{date}})",
      "Daily journal template",
      "journal"
    },
    {
      "project",
      R"(# Project: {{title}}

## Overview
{{description}}

## Goals
- 

## Timeline
- **Start Date:** {{start_date}}
- **Target Date:** {{target_date}}

## Resources
- 

## Progress Log

### {{date}}
- Project created

## Notes


---
Tags: {{tags}}, project)",
      "Project planning template",
      "work"
    },
    {
      "book-review",
      R"(# Book Review: {{title}}

**Author:** {{author}}  
**Genre:** {{genre}}  
**Rating:** {{rating}}/5  
**Date Finished:** {{date}}

## Summary


## Key Takeaways
- 

## Quotes


## My Thoughts


---
Tags: {{tags}}, book-review, reading)",
      "Book review template",
      "review"
    }
  };
}

Result<void> TemplateManager::ensureDirectoryExists() {
  try {
    if (!std::filesystem::exists(config_.templates_dir)) {
      std::filesystem::create_directories(config_.templates_dir);
    }
    
    // Ensure parent directory for metadata file exists
    auto metadata_parent = config_.metadata_file.parent_path();
    if (!std::filesystem::exists(metadata_parent)) {
      std::filesystem::create_directories(metadata_parent);
    }
    
    return {};
    
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kDirectoryCreateError, 
                         "Cannot create templates directory: " + std::string(e.what())));
  }
}

std::string TemplateManager::sanitizeTemplateName(const std::string& name) const {
  std::string result = name;
  
  // Replace invalid characters with underscores
  std::replace_if(result.begin(), result.end(), 
                 [](char c) { return !std::isalnum(c) && c != '-' && c != '_'; }, 
                 '_');
  
  // Convert to lowercase
  std::transform(result.begin(), result.end(), result.begin(), ::tolower);
  
  return result;
}

} // namespace nx::template_system