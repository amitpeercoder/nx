#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "nx/common.hpp"
#include "nx/core/note_id.hpp"

namespace nx::core {

// Note metadata structure
class Metadata {
 public:
  // Default constructor (creates empty metadata)
  Metadata();
  
  // Constructor with required fields
  Metadata(NoteId id, std::string title);

  // Getters
  const NoteId& id() const noexcept { return id_; }
  const std::string& title() const noexcept { return title_; }
  const std::chrono::system_clock::time_point& created() const noexcept { return created_; }
  const std::chrono::system_clock::time_point& updated() const noexcept { return updated_; }
  const std::vector<std::string>& tags() const noexcept { return tags_; }
  const std::optional<std::string>& notebook() const noexcept { return notebook_; }
  const std::vector<NoteId>& links() const noexcept { return links_; }

  // Setters
  void setTitle(const std::string& title);
  void setCreated(std::chrono::system_clock::time_point time);
  void setUpdated(std::chrono::system_clock::time_point time);
  void setTags(const std::vector<std::string>& tags);
  void setNotebook(const std::string& notebook);
  void setNotebook(std::optional<std::string> notebook);
  void setLinks(const std::vector<NoteId>& links);

  // Tag operations
  void addTag(const std::string& tag);
  void removeTag(const std::string& tag);
  bool hasTag(const std::string& tag) const noexcept;

  // Link operations
  void addLink(const NoteId& link);
  void removeLink(const NoteId& link);
  bool hasLink(const NoteId& link) const noexcept;

  // Custom fields
  void setCustomField(const std::string& key, const std::string& value);
  std::optional<std::string> getCustomField(const std::string& key) const noexcept;
  void removeCustomField(const std::string& key);
  const std::unordered_map<std::string, std::string>& customFields() const noexcept { return custom_fields_; }

  // Update the modified timestamp to now
  void touch();

  // Validation
  Result<void> validate() const;

  // YAML serialization
  std::string toYaml() const;
  static Result<Metadata> fromYaml(const std::string& yaml);

 private:
  NoteId id_;
  std::string title_;
  std::chrono::system_clock::time_point created_;
  std::chrono::system_clock::time_point updated_;
  std::vector<std::string> tags_;
  std::optional<std::string> notebook_;
  std::vector<NoteId> links_;
  std::unordered_map<std::string, std::string> custom_fields_;
};

}  // namespace nx::core