#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <map>

#include "nx/common.hpp"
#include "nx/core/note.hpp"

namespace nx::template_system {

// Template metadata structure
struct TemplateInfo {
  std::string name;
  std::string description;
  std::string category;
  std::vector<std::string> variables;  // Template variables like {{title}}, {{date}}
  std::chrono::system_clock::time_point created;
  std::chrono::system_clock::time_point modified;
  std::filesystem::path file_path;
};

// Template processing result
struct TemplateResult {
  std::string content;
  std::string title;
  std::vector<std::string> tags;
  std::optional<std::string> notebook;
  std::map<std::string, std::string> metadata;
};

// Template variable substitution
using VariableMap = std::map<std::string, std::string>;

class TemplateManager {
public:
  struct Config {
    std::filesystem::path templates_dir;
    std::filesystem::path metadata_file;
  };

  explicit TemplateManager(const Config& config);

  // Template CRUD operations
  Result<void> createTemplate(const std::string& name, 
                             const std::string& content,
                             const std::string& description = "",
                             const std::string& category = "default");
  
  Result<std::string> getTemplate(const std::string& name);
  Result<TemplateInfo> getTemplateInfo(const std::string& name);
  Result<std::vector<TemplateInfo>> listTemplates(const std::string& category = "");
  Result<void> updateTemplate(const std::string& name, const std::string& content);
  Result<void> deleteTemplate(const std::string& name);

  // Template processing
  Result<TemplateResult> processTemplate(const std::string& name, 
                                        const VariableMap& variables = {});
  
  Result<nx::core::Note> createNoteFromTemplate(const std::string& template_name,
                                               const VariableMap& variables = {});

  // Category management
  Result<std::vector<std::string>> listCategories();
  Result<void> setTemplateCategory(const std::string& name, const std::string& category);

  // Built-in templates
  Result<void> installBuiltinTemplates();
  
  // Template validation
  Result<void> validateTemplate(const std::string& content);
  std::vector<std::string> extractVariables(const std::string& content);

  // Template search
  Result<std::vector<TemplateInfo>> searchTemplates(const std::string& query);

private:
  Config config_;
  mutable std::map<std::string, TemplateInfo> template_cache_;
  mutable bool cache_valid_ = false;

  // Internal operations
  Result<void> loadTemplateCache();
  Result<void> saveMetadata();
  Result<TemplateInfo> loadTemplateInfo(const std::filesystem::path& template_file);
  std::filesystem::path getTemplatePath(const std::string& name) const;
  
  // Variable processing
  std::string processVariables(const std::string& content, const VariableMap& variables);
  VariableMap getDefaultVariables();
  
  // Built-in template definitions
  struct BuiltinTemplate {
    std::string name;
    std::string content;
    std::string description;
    std::string category;
  };
  
  std::vector<BuiltinTemplate> getBuiltinTemplates() const;
  
  // File operations
  Result<void> ensureDirectoryExists();
  std::string sanitizeTemplateName(const std::string& name) const;
};

} // namespace nx::template_system