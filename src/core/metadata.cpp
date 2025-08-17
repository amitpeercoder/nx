#include "nx/core/metadata.hpp"

#include <algorithm>
#include <set>
#include <unordered_set>
#include <yaml-cpp/yaml.h>

#include "nx/util/time.hpp"

namespace nx::core {

Metadata::Metadata() : id_(NoteId::generate()), title_("") {
  auto now = std::chrono::system_clock::now();
  created_ = now;
  updated_ = now;
}

Metadata::Metadata(NoteId id, std::string title)
    : id_(std::move(id)), title_(std::move(title)) {
  auto now = std::chrono::system_clock::now();
  created_ = now;
  updated_ = now;
}

void Metadata::setTitle(const std::string& title) {
  title_ = title;
  touch();
}

void Metadata::setCreated(std::chrono::system_clock::time_point time) {
  created_ = time;
}

void Metadata::setUpdated(std::chrono::system_clock::time_point time) {
  updated_ = time;
}

void Metadata::setTags(const std::vector<std::string>& tags) {
  tags_ = tags;
  // Remove duplicates and sort
  std::sort(tags_.begin(), tags_.end());
  tags_.erase(std::unique(tags_.begin(), tags_.end()), tags_.end());
  touch();
}

void Metadata::setNotebook(const std::string& notebook) {
  notebook_ = notebook.empty() ? std::nullopt : std::make_optional(notebook);
  touch();
}

void Metadata::setNotebook(std::optional<std::string> notebook) {
  notebook_ = std::move(notebook);
  touch();
}

void Metadata::setLinks(const std::vector<NoteId>& links) {
  links_ = links;
  // Remove duplicates and sort
  std::sort(links_.begin(), links_.end());
  links_.erase(std::unique(links_.begin(), links_.end()), links_.end());
  touch();
}

void Metadata::addTag(const std::string& tag) {
  if (!hasTag(tag)) {
    tags_.push_back(tag);
    std::sort(tags_.begin(), tags_.end());
    touch();
  }
}

void Metadata::removeTag(const std::string& tag) {
  auto it = std::find(tags_.begin(), tags_.end(), tag);
  if (it != tags_.end()) {
    tags_.erase(it);
    touch();
  }
}

bool Metadata::hasTag(const std::string& tag) const noexcept {
  return std::find(tags_.begin(), tags_.end(), tag) != tags_.end();
}

void Metadata::addLink(const NoteId& link) {
  if (!hasLink(link)) {
    links_.push_back(link);
    std::sort(links_.begin(), links_.end());
    touch();
  }
}

void Metadata::removeLink(const NoteId& link) {
  auto it = std::find(links_.begin(), links_.end(), link);
  if (it != links_.end()) {
    links_.erase(it);
    touch();
  }
}

bool Metadata::hasLink(const NoteId& link) const noexcept {
  return std::find(links_.begin(), links_.end(), link) != links_.end();
}

void Metadata::setCustomField(const std::string& key, const std::string& value) {
  custom_fields_[key] = value;
  touch();
}

std::optional<std::string> Metadata::getCustomField(const std::string& key) const noexcept {
  auto it = custom_fields_.find(key);
  return it != custom_fields_.end() ? std::make_optional(it->second) : std::nullopt;
}

void Metadata::removeCustomField(const std::string& key) {
  custom_fields_.erase(key);
  touch();
}

void Metadata::touch() {
  updated_ = std::chrono::system_clock::now();
}

Result<void> Metadata::validate() const {
  // Validate ID
  if (!id_.isValid()) {
    return std::unexpected(makeError(ErrorCode::kValidationError, "Invalid note ID"));
  }
  
  // Validate title
  if (title_.empty()) {
    return std::unexpected(makeError(ErrorCode::kValidationError, "Title cannot be empty"));
  }
  
  if (title_.size() > 200) {
    return std::unexpected(makeError(ErrorCode::kValidationError, "Title too long (max 200 characters)"));
  }
  
  // Validate tags
  for (const auto& tag : tags_) {
    if (tag.empty()) {
      return std::unexpected(makeError(ErrorCode::kValidationError, "Tag cannot be empty"));
    }
    if (tag.size() > 50) {
      return std::unexpected(makeError(ErrorCode::kValidationError, "Tag too long (max 50 characters)"));
    }
    if (tag.find(' ') != std::string::npos) {
      return std::unexpected(makeError(ErrorCode::kValidationError, "Tag cannot contain spaces: " + tag));
    }
  }
  
  // Validate notebook
  if (notebook_.has_value() && notebook_->size() > 50) {
    return std::unexpected(makeError(ErrorCode::kValidationError, "Notebook name too long (max 50 characters)"));
  }
  
  // Validate custom fields
  for (const auto& [key, value] : custom_fields_) {
    if (key.empty()) {
      return std::unexpected(makeError(ErrorCode::kValidationError, "Custom field key cannot be empty"));
    }
    if (key.size() > 50) {
      return std::unexpected(makeError(ErrorCode::kValidationError, "Custom field key too long (max 50 characters)"));
    }
    if (value.size() > 1000) {
      return std::unexpected(makeError(ErrorCode::kValidationError, "Custom field value too long (max 1000 characters)"));
    }
  }
  
  return {};
}

std::string Metadata::toYaml() const {
  YAML::Node node;
  
  node["id"] = id_.toString();
  node["title"] = title_;
  node["created"] = nx::util::Time::toRfc3339(created_);
  node["updated"] = nx::util::Time::toRfc3339(updated_);
  
  if (!tags_.empty()) {
    node["tags"] = tags_;
  }
  
  if (notebook_.has_value()) {
    node["notebook"] = *notebook_;
  }
  
  if (!links_.empty()) {
    std::vector<std::string> link_strings;
    for (const auto& link : links_) {
      link_strings.push_back(link.toString());
    }
    node["links"] = link_strings;
  }
  
  for (const auto& [key, value] : custom_fields_) {
    node[key] = value;
  }
  
  YAML::Emitter emitter;
  emitter << node;
  return emitter.c_str();
}

Result<Metadata> Metadata::fromYaml(const std::string& yaml) {
  try {
    YAML::Node node = YAML::Load(yaml);
    
    // Parse required fields
    if (!node["id"] || !node["title"]) {
      return std::unexpected(makeError(ErrorCode::kParseError, "Missing required fields: id or title"));
    }
    
    auto id_result = NoteId::fromString(node["id"].as<std::string>());
    if (!id_result.has_value()) {
      return std::unexpected(id_result.error());
    }
    
    Metadata metadata(*id_result, node["title"].as<std::string>());
    
    // Parse timestamps
    if (node["created"]) {
      auto created_result = nx::util::Time::fromRfc3339(node["created"].as<std::string>());
      if (created_result.has_value()) {
        metadata.created_ = *created_result;
      }
    }
    
    if (node["updated"]) {
      auto updated_result = nx::util::Time::fromRfc3339(node["updated"].as<std::string>());
      if (updated_result.has_value()) {
        metadata.updated_ = *updated_result;
      }
    }
    
    // Parse tags
    if (node["tags"] && node["tags"].IsSequence()) {
      std::vector<std::string> tags;
      tags.reserve(node["tags"].size());  // Pre-allocate to avoid reallocations
      for (const auto& tag_node : node["tags"]) {
        tags.emplace_back(tag_node.as<std::string>());  // Use emplace_back for efficiency
      }
      metadata.setTags(std::move(tags));  // Move to avoid copy
    }
    
    // Parse notebook
    if (node["notebook"]) {
      metadata.setNotebook(node["notebook"].as<std::string>());
    }
    
    // Parse links
    if (node["links"] && node["links"].IsSequence()) {
      std::vector<NoteId> links;
      links.reserve(node["links"].size());  // Pre-allocate to avoid reallocations
      for (const auto& link_node : node["links"]) {
        auto link_result = NoteId::fromString(link_node.as<std::string>());
        if (link_result.has_value()) {
          links.emplace_back(std::move(*link_result));  // Move the NoteId
        }
      }
      metadata.setLinks(std::move(links));  // Move to avoid copy
    }
    
    // Parse custom fields (skip standard fields)
    // Use static set to avoid recreation on each call and unordered_set for O(1) lookup
    static const std::unordered_set<std::string> standard_fields = {"id", "title", "created", "updated", "tags", "notebook", "links"};
    for (const auto& pair : node) {
      std::string key = pair.first.as<std::string>();
      if (standard_fields.find(key) == standard_fields.end()) {
        metadata.setCustomField(std::move(key), pair.second.as<std::string>());  // Move key to avoid copy
      }
    }
    
    // Don't update the timestamp when loading from YAML
    metadata.updated_ = metadata.updated_;
    
    auto validation_result = metadata.validate();
    if (!validation_result.has_value()) {
      return std::unexpected(validation_result.error());
    }
    
    return metadata;
    
  } catch (const YAML::Exception& e) {
    return std::unexpected(makeError(ErrorCode::kParseError, "YAML parse error: " + std::string(e.what())));
  }
}

}  // namespace nx::core