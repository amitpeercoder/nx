#pragma once

#include <string>
#include <string_view>

#include "nx/common.hpp"
#include "nx/core/metadata.hpp"
#include "nx/core/note_id.hpp"

namespace nx::core {

// Note class representing a complete note with metadata and content
class Note {
 public:
  // Constructor with metadata and content
  Note(Metadata metadata, std::string content);

  // Create new note with minimal information
  static Note create(const std::string& title, const std::string& content = "");

  // Getters
  const Metadata& metadata() const noexcept { return metadata_; }
  Metadata& metadata() noexcept { return metadata_; }
  const std::string& content() const noexcept { return content_; }
  const NoteId& id() const noexcept { return metadata_.id(); }

  // Setters
  void setContent(const std::string& content);
  void setMetadata(const Metadata& metadata);

  // Content operations
  void appendContent(const std::string& content);
  void prependContent(const std::string& content);

  // Convenience metadata accessors
  const std::string& title() const noexcept { return metadata_.title(); }
  void setTitle(const std::string& title);

  const std::vector<std::string>& tags() const noexcept { return metadata_.tags(); }
  void setTags(const std::vector<std::string>& tags);
  void addTag(const std::string& tag);

  const std::optional<std::string>& notebook() const noexcept { return metadata_.notebook(); }
  void setNotebook(const std::string& notebook);

  // Update the modified timestamp
  void touch();

  // Validation
  Result<void> validate() const;

  // File format serialization (YAML front-matter + Markdown)
  std::string toFileFormat() const;
  static Result<Note> fromFileFormat(const std::string& content);

  // Get filename for this note (ULID-slug.md)
  std::string filename() const;

  // Extract links from content (markdown links to other notes)
  std::vector<NoteId> extractContentLinks() const;

  // Update metadata links based on content
  void updateLinksFromContent();

  // Content search
  bool containsText(std::string_view text, bool case_sensitive = false) const noexcept;
  std::vector<size_t> findTextPositions(std::string_view text, bool case_sensitive = false) const;

 private:
  Metadata metadata_;
  std::string content_;

  // Generate filename slug from title
  static std::string generateSlug(const std::string& title) noexcept;
};

}  // namespace nx::core