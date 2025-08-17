#include "nx/core/note.hpp"

#include <algorithm>
#include <regex>
#include <sstream>

namespace nx::core {

Note::Note(Metadata metadata, std::string content)
    : metadata_(std::move(metadata)), content_(std::move(content)) {}

Note Note::create(const std::string& title, const std::string& content) {
  auto id = NoteId::generate();
  Metadata metadata(id, title);
  return Note(std::move(metadata), content);
}

void Note::setContent(const std::string& content) {
  content_ = content;
  metadata_.touch();
}

void Note::setMetadata(const Metadata& metadata) {
  metadata_ = metadata;
}

void Note::appendContent(const std::string& content) {
  if (!content_.empty() && content_.back() != '\n') {
    content_ += "\n";
  }
  content_ += content;
  metadata_.touch();
}

void Note::prependContent(const std::string& content) {
  std::string new_content = content;
  if (!new_content.empty() && new_content.back() != '\n') {
    new_content += "\n";
  }
  content_ = new_content + content_;
  metadata_.touch();
}

void Note::setTitle(const std::string& title) {
  metadata_.setTitle(title);
}

void Note::setTags(const std::vector<std::string>& tags) {
  metadata_.setTags(tags);
}

void Note::addTag(const std::string& tag) {
  metadata_.addTag(tag);
}

void Note::setNotebook(const std::string& notebook) {
  metadata_.setNotebook(notebook);
}

void Note::touch() {
  metadata_.touch();
}

Result<void> Note::validate() const {
  // Validate metadata
  auto metadata_result = metadata_.validate();
  if (!metadata_result.has_value()) {
    return metadata_result;
  }
  
  // Validate content size (reasonable limits)
  if (content_.size() > 10 * 1024 * 1024) {  // 10MB limit
    return std::unexpected(makeError(ErrorCode::kValidationError, "Content too large (max 10MB)"));
  }
  
  return {};
}

std::string Note::toFileFormat() const {
  std::ostringstream oss;
  
  // YAML front-matter
  oss << "---\n";
  oss << metadata_.toYaml();
  oss << "\n---\n\n";
  
  // Content
  oss << content_;
  
  return oss.str();
}

Result<Note> Note::fromFileFormat(const std::string& content) {
  // Look for YAML front-matter delimiters
  const std::string yaml_start = "---\n";
  const std::string yaml_end = "\n---\n";
  
  if (content.substr(0, yaml_start.length()) != yaml_start) {
    return std::unexpected(makeError(ErrorCode::kParseError, "Missing YAML front-matter start delimiter"));
  }
  
  size_t yaml_end_pos = content.find(yaml_end, yaml_start.length());
  if (yaml_end_pos == std::string::npos) {
    return std::unexpected(makeError(ErrorCode::kParseError, "Missing YAML front-matter end delimiter"));
  }
  
  // Extract YAML and content
  std::string yaml_content = content.substr(
      yaml_start.length(), 
      yaml_end_pos - yaml_start.length()
  );
  
  std::string note_content = content.substr(yaml_end_pos + yaml_end.length());
  
  // Remove leading newlines from content efficiently
  auto first_non_newline = note_content.find_first_not_of('\n');
  if (first_non_newline != std::string::npos) {
    note_content = note_content.substr(first_non_newline);
  } else if (!note_content.empty()) {
    note_content.clear(); // All newlines
  }
  
  // Parse metadata
  auto metadata_result = Metadata::fromYaml(yaml_content);
  if (!metadata_result.has_value()) {
    return std::unexpected(metadata_result.error());
  }
  
  Note note(*metadata_result, note_content);
  
  // Validate the complete note
  auto validation_result = note.validate();
  if (!validation_result.has_value()) {
    return std::unexpected(validation_result.error());
  }
  
  return note;
}

std::string Note::filename() const {
  std::string slug = generateSlug(metadata_.title());
  return metadata_.id().toString() + "-" + slug + ".md";
}

std::vector<NoteId> Note::extractContentLinks() const {
  std::vector<NoteId> links;
  
  // Match markdown links with ULID patterns: [text](01ABC...)
  std::regex ulid_link_regex(R"(\[([^\]]*)\]\(([0-9A-HJKMNP-TV-Z]{26})\))");
  std::sregex_iterator iter(content_.begin(), content_.end(), ulid_link_regex);
  std::sregex_iterator end;
  
  for (; iter != end; ++iter) {
    std::string ulid_str = iter->str(2);
    auto id_result = NoteId::fromString(ulid_str);
    if (id_result.has_value()) {
      links.push_back(*id_result);
    }
  }
  
  // Remove duplicates
  std::sort(links.begin(), links.end());
  links.erase(std::unique(links.begin(), links.end()), links.end());
  
  return links;
}

void Note::updateLinksFromContent() {
  auto content_links = extractContentLinks();
  metadata_.setLinks(content_links);
}

bool Note::containsText(std::string_view text, bool case_sensitive) const noexcept {
  if (case_sensitive) {
    return content_.find(text) != std::string::npos ||
           metadata_.title().find(text) != std::string::npos;
  } else {
    std::string lower_content = content_;
    std::string lower_title = metadata_.title();
    std::string lower_text(text);
    
    std::transform(lower_content.begin(), lower_content.end(), lower_content.begin(), ::tolower);
    std::transform(lower_title.begin(), lower_title.end(), lower_title.begin(), ::tolower);
    std::transform(lower_text.begin(), lower_text.end(), lower_text.begin(), ::tolower);
    
    return lower_content.find(lower_text) != std::string::npos ||
           lower_title.find(lower_text) != std::string::npos;
  }
}

std::vector<size_t> Note::findTextPositions(std::string_view text, bool case_sensitive) const {
  std::vector<size_t> positions;
  
  std::string search_content = content_;
  std::string search_text(text);
  
  if (!case_sensitive) {
    std::transform(search_content.begin(), search_content.end(), search_content.begin(), ::tolower);
    std::transform(search_text.begin(), search_text.end(), search_text.begin(), ::tolower);
  }
  
  size_t pos = 0;
  while ((pos = search_content.find(search_text, pos)) != std::string::npos) {
    positions.push_back(pos);
    pos += search_text.length();
  }
  
  return positions;
}

std::string Note::generateSlug(const std::string& title) noexcept {
  std::string slug = title;
  
  // Convert to lowercase
  std::transform(slug.begin(), slug.end(), slug.begin(), ::tolower);
  
  // Replace spaces and special characters with hyphens
  std::regex special_chars(R"([^a-z0-9]+)");
  slug = std::regex_replace(slug, special_chars, "-");
  
  // Remove leading/trailing hyphens
  while (!slug.empty() && slug.front() == '-') {
    slug = slug.substr(1);
  }
  while (!slug.empty() && slug.back() == '-') {
    slug.pop_back();
  }
  
  // Limit length
  if (slug.length() > 50) {
    slug = slug.substr(0, 50);
    // Remove trailing hyphen if present
    while (!slug.empty() && slug.back() == '-') {
      slug.pop_back();
    }
  }
  
  // Ensure not empty
  if (slug.empty()) {
    slug = "untitled";
  }
  
  return slug;
}

}  // namespace nx::core