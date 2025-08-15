#include "nx/index/ripgrep_index.hpp"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <regex>
#include <sstream>
#include <iterator>
#include <set>

#include "nx/util/time.hpp"
#include "nx/util/safe_process.hpp"

namespace nx::index {

RipgrepIndex::RipgrepIndex(std::filesystem::path notes_dir)
    : notes_dir_(std::move(notes_dir)) {
}

Result<void> RipgrepIndex::initialize() {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  
  // Check if notes directory exists
  if (!std::filesystem::exists(notes_dir_)) {
    return std::unexpected(makeError(ErrorCode::kDirectoryNotFound,
                                     "Notes directory not found: " + notes_dir_.string()));
  }
  
  // Check if ripgrep is available
  if (!isRipgrepAvailable()) {
    return std::unexpected(makeError(ErrorCode::kExternalToolError,
                                     "ripgrep (rg) not found in PATH"));
  }
  
  // Build initial metadata cache
  auto cache_result = buildMetadataCache();
  if (!cache_result.has_value()) {
    return cache_result;
  }
  
  cache_dirty_ = false;
  last_cache_update_ = std::chrono::system_clock::now();
  
  return {};
}

Result<void> RipgrepIndex::addNote(const nx::core::Note& note) {
  return updateNoteMetadata(note);
}

Result<void> RipgrepIndex::updateNote(const nx::core::Note& note) {
  return updateNoteMetadata(note);
}

Result<void> RipgrepIndex::removeNote(const nx::core::NoteId& id) {
  return removeNoteMetadata(id);
}

Result<std::vector<SearchResult>> RipgrepIndex::search(const SearchQuery& query) {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  
  // Refresh cache if needed
  if (cache_dirty_) {
    auto cache_result = buildMetadataCache();
    if (!cache_result.has_value()) {
      return std::unexpected(cache_result.error());
    }
    cache_dirty_ = false;
  }
  
  std::vector<SearchResult> results;
  
  // If we have text to search, use ripgrep
  if (!query.text.empty()) {
    auto file_paths_result = ripgrepSearch(query.text, query.limit + query.offset);
    if (!file_paths_result.has_value()) {
      return std::unexpected(file_paths_result.error());
    }
    
    // Convert file paths to search results
    for (const auto& file_path : *file_paths_result) {
      auto result = createSearchResult(file_path, query);
      if (result.has_value()) {
        results.push_back(*result);
      }
    }
  } else {
    // No text search - filter by metadata only
    std::vector<NoteMeta> candidates;
    for (const auto& [id, meta] : metadata_cache_) {
      candidates.push_back(meta);
    }
    
    auto filtered = filterByMetadata(candidates, query);
    
    // Convert to search results
    for (const auto& meta : filtered) {
      SearchResult result;
      result.id = meta.id;
      result.title = meta.title;
      result.modified = meta.modified;
      result.tags = meta.tags;
      result.notebook = meta.notebook;
      result.score = 1.0; // Default score for metadata-only results
      results.push_back(result);
    }
  }
  
  // Apply metadata filters
  std::vector<NoteMeta> meta_list;
  for (const auto& result : results) {
    auto it = metadata_cache_.find(result.id.toString());
    if (it != metadata_cache_.end()) {
      meta_list.push_back(it->second);
    }
  }
  
  auto filtered_meta = filterByMetadata(meta_list, query);
  
  // Rebuild results from filtered metadata
  std::vector<SearchResult> filtered_results;
  for (const auto& meta : filtered_meta) {
    auto it = std::find_if(results.begin(), results.end(),
                          [&meta](const SearchResult& r) { return r.id == meta.id; });
    if (it != results.end()) {
      filtered_results.push_back(*it);
    }
  }
  
  // Sort by score (descending)
  std::sort(filtered_results.begin(), filtered_results.end(),
            [](const SearchResult& a, const SearchResult& b) {
              return a.score > b.score;
            });
  
  // Apply pagination
  if (query.offset >= filtered_results.size()) {
    return std::vector<SearchResult>{};
  }
  
  size_t start = query.offset;
  size_t end = std::min(start + query.limit, filtered_results.size());
  
  return std::vector<SearchResult>(filtered_results.begin() + start,
                                   filtered_results.begin() + end);
}

Result<std::vector<nx::core::NoteId>> RipgrepIndex::searchIds(const SearchQuery& query) {
  auto results = search(query);
  if (!results.has_value()) {
    return std::unexpected(results.error());
  }
  
  std::vector<nx::core::NoteId> ids;
  ids.reserve(results->size());
  
  for (const auto& result : *results) {
    ids.push_back(result.id);
  }
  
  return ids;
}

Result<size_t> RipgrepIndex::searchCount(const SearchQuery& query) {
  // For count, we don't need pagination - set high limits
  SearchQuery count_query = query;
  count_query.limit = 10000;
  count_query.offset = 0;
  
  auto results = search(count_query);
  if (!results.has_value()) {
    return std::unexpected(results.error());
  }
  
  return results->size();
}

Result<std::vector<std::string>> RipgrepIndex::suggestTags(const std::string& prefix, size_t limit) {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  
  std::set<std::string> unique_tags;
  
  for (const auto& [id, meta] : metadata_cache_) {
    for (const auto& tag : meta.tags) {
      if (tag.starts_with(prefix)) {
        unique_tags.insert(tag);
      }
    }
  }
  
  std::vector<std::string> suggestions(unique_tags.begin(), unique_tags.end());
  
  if (suggestions.size() > limit) {
    suggestions.resize(limit);
  }
  
  return suggestions;
}

Result<std::vector<std::string>> RipgrepIndex::suggestNotebooks(const std::string& prefix, size_t limit) {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  
  std::set<std::string> unique_notebooks;
  
  for (const auto& [id, meta] : metadata_cache_) {
    if (meta.notebook.has_value() && meta.notebook->starts_with(prefix)) {
      unique_notebooks.insert(*meta.notebook);
    }
  }
  
  std::vector<std::string> suggestions(unique_notebooks.begin(), unique_notebooks.end());
  
  if (suggestions.size() > limit) {
    suggestions.resize(limit);
  }
  
  return suggestions;
}

Result<IndexStats> RipgrepIndex::getStats() {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  
  IndexStats stats;
  stats.total_notes = metadata_cache_.size();
  stats.total_words = 0;
  stats.last_updated = std::chrono::system_clock::time_point{};
  
  for (const auto& [id, meta] : metadata_cache_) {
    stats.total_words += meta.word_count;
    if (meta.modified > stats.last_updated) {
      stats.last_updated = meta.modified;
    }
  }
  
  // Calculate approximate size of notes directory
  std::error_code ec;
  stats.index_size_bytes = 0;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(notes_dir_, ec)) {
    if (!ec && entry.is_regular_file()) {
      auto size = std::filesystem::file_size(entry.path(), ec);
      if (!ec) {
        stats.index_size_bytes += size;
      }
    }
  }
  
  stats.last_optimized = last_cache_update_;
  
  return stats;
}

Result<bool> RipgrepIndex::isHealthy() {
  return isRipgrepAvailable() && std::filesystem::exists(notes_dir_);
}

Result<void> RipgrepIndex::validateIndex() {
  if (!isRipgrepAvailable()) {
    return std::unexpected(makeError(ErrorCode::kExternalToolError,
                                     "ripgrep (rg) not available"));
  }
  
  if (!std::filesystem::exists(notes_dir_)) {
    return std::unexpected(makeError(ErrorCode::kDirectoryNotFound,
                                     "Notes directory not found"));
  }
  
  return {};
}

Result<void> RipgrepIndex::rebuild() {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  
  // Clear cache and rebuild
  metadata_cache_.clear();
  auto result = buildMetadataCache();
  if (result.has_value()) {
    cache_dirty_ = false;
    last_cache_update_ = std::chrono::system_clock::now();
  }
  
  return result;
}

Result<void> RipgrepIndex::optimize() {
  // For ripgrep index, optimization is just rebuilding the cache
  return rebuild();
}

Result<void> RipgrepIndex::vacuum() {
  // No-op for ripgrep index - no database to vacuum
  return {};
}

Result<void> RipgrepIndex::beginTransaction() {
  // No-op for ripgrep index
  return {};
}

Result<void> RipgrepIndex::commitTransaction() {
  // No-op for ripgrep index
  return {};
}

Result<void> RipgrepIndex::rollbackTransaction() {
  // No-op for ripgrep index
  return {};
}

// Private methods

Result<void> RipgrepIndex::buildMetadataCache() {
  metadata_cache_.clear();
  
  std::error_code ec;
  for (const auto& entry : std::filesystem::recursive_directory_iterator(notes_dir_, ec)) {
    if (ec) continue;
    
    if (entry.is_regular_file() && entry.path().extension() == ".md") {
      auto meta_result = parseNoteFile(entry.path());
      if (meta_result.has_value()) {
        metadata_cache_[meta_result->id.toString()] = *meta_result;
      }
    }
  }
  
  return {};
}

Result<void> RipgrepIndex::updateNoteMetadata(const nx::core::Note& note) {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  
  NoteMeta meta;
  meta.id = note.id();
  meta.title = note.title();
  meta.created = note.metadata().created();
  meta.modified = note.metadata().updated();
  meta.tags = note.metadata().tags();
  meta.notebook = note.notebook();
  
  // Calculate word count
  std::istringstream iss(note.content());
  meta.word_count = std::distance(std::istream_iterator<std::string>(iss),
                                  std::istream_iterator<std::string>());
  
  // We don't know the file path here, so we'll have to find it or assume it
  // This is a limitation of the ripgrep fallback approach
  std::string filename = note.id().toString() + ".md";
  meta.file_path = notes_dir_ / filename;
  
  metadata_cache_[note.id().toString()] = meta;
  
  return {};
}

Result<void> RipgrepIndex::removeNoteMetadata(const nx::core::NoteId& id) {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  
  metadata_cache_.erase(id.toString());
  
  return {};
}

Result<std::vector<std::filesystem::path>> RipgrepIndex::ripgrepSearch(const std::string& query_text, size_t limit) const {
  if (query_text.empty()) {
    return std::vector<std::filesystem::path>{};
  }
  
  // Safely execute ripgrep using SafeProcess
  std::string escaped_query = escapeRipgrepQuery(query_text);
  std::vector<std::string> args = {
    "--files-with-matches",
    "--type", "md",
    "--max-count", "1",
    escaped_query,
    notes_dir_.string()
  };
  
  auto process_result = nx::util::SafeProcess::executeForOutput("rg", args);
  if (!process_result.has_value()) {
    return std::unexpected(makeError(ErrorCode::kExternalToolError,
                                     "Failed to execute ripgrep: " + process_result.error().message()));
  }
  
  std::vector<std::filesystem::path> results;
  std::istringstream output_stream(process_result.value());
  std::string line;
  
  while (std::getline(output_stream, line) && results.size() < limit) {
    if (!line.empty()) {
      results.emplace_back(line);
    }
  }
  
  return results;
}

Result<SearchResult> RipgrepIndex::createSearchResult(const std::filesystem::path& file_path,
                                                      const SearchQuery& query) const {
  auto meta_result = parseNoteFile(file_path);
  if (!meta_result.has_value()) {
    return std::unexpected(meta_result.error());
  }
  
  const auto& meta = *meta_result;
  
  SearchResult result;
  result.id = meta.id;
  result.title = meta.title;
  result.modified = meta.modified;
  result.tags = meta.tags;
  result.notebook = meta.notebook;
  result.score = calculateSimpleScore(meta, query);
  
  // Extract snippet if highlighting is enabled
  if (query.highlight && !query.text.empty()) {
    auto snippet_result = extractSnippet(file_path, query.text, true);
    if (snippet_result.has_value()) {
      result.snippet = *snippet_result;
    }
  }
  
  return result;
}

std::vector<RipgrepIndex::NoteMeta> RipgrepIndex::filterByMetadata(const std::vector<NoteMeta>& candidates,
                                                                   const SearchQuery& query) const {
  std::vector<NoteMeta> filtered;
  
  for (const auto& meta : candidates) {
    bool matches = true;
    
    // Filter by tags
    if (!query.tags.empty()) {
      bool has_all_tags = true;
      for (const auto& required_tag : query.tags) {
        bool has_tag = std::find(meta.tags.begin(), meta.tags.end(), required_tag) != meta.tags.end();
        if (!has_tag) {
          has_all_tags = false;
          break;
        }
      }
      if (!has_all_tags) {
        matches = false;
      }
    }
    
    // Filter by notebook
    if (matches && query.notebook.has_value()) {
      if (!meta.notebook.has_value() || *meta.notebook != *query.notebook) {
        matches = false;
      }
    }
    
    // Filter by date ranges
    if (matches && query.since.has_value()) {
      if (meta.modified < *query.since) {
        matches = false;
      }
    }
    
    if (matches && query.until.has_value()) {
      if (meta.modified > *query.until) {
        matches = false;
      }
    }
    
    if (matches) {
      filtered.push_back(meta);
    }
  }
  
  return filtered;
}

Result<RipgrepIndex::NoteMeta> RipgrepIndex::parseNoteFile(const std::filesystem::path& file_path) const {
  std::ifstream file(file_path);
  if (!file) {
    return std::unexpected(makeError(ErrorCode::kFileError,
                                     "Cannot read file: " + file_path.string()));
  }
  
  NoteMeta meta;
  meta.file_path = file_path;
  
  // Try to extract ID from filename
  std::string filename = file_path.stem().string();
  auto id_result = nx::core::NoteId::fromString(filename);
  if (id_result.has_value()) {
    meta.id = *id_result;
  } else {
    // Generate a new ID if we can't parse the filename
    meta.id = nx::core::NoteId::generate();
  }
  
  // Parse front matter and content
  std::string line;
  bool in_frontmatter = false;
  bool found_frontmatter = false;
  std::ostringstream content_stream;
  
  while (std::getline(file, line)) {
    if (line == "---") {
      if (!found_frontmatter) {
        in_frontmatter = true;
        found_frontmatter = true;
        continue;
      } else if (in_frontmatter) {
        in_frontmatter = false;
        continue;
      }
    }
    
    if (in_frontmatter) {
      // Parse YAML front matter
      if (line.starts_with("title:")) {
        meta.title = line.substr(6);
        // Trim whitespace and quotes
        meta.title = std::regex_replace(meta.title, std::regex("^\\s*[\"']?|[\"']?\\s*$"), "");
      } else if (line.starts_with("tags:")) {
        // Parse tags array - simplified parsing
        std::string tags_str = line.substr(5);
        std::regex tag_regex(R"([a-zA-Z_][a-zA-Z0-9_-]*)");
        std::sregex_iterator iter(tags_str.begin(), tags_str.end(), tag_regex);
        std::sregex_iterator end;
        
        for (; iter != end; ++iter) {
          meta.tags.push_back(iter->str());
        }
      } else if (line.starts_with("notebook:")) {
        std::string notebook = line.substr(9);
        notebook = std::regex_replace(notebook, std::regex("^\\s*[\"']?|[\"']?\\s*$"), "");
        if (!notebook.empty()) {
          meta.notebook = notebook;
        }
      }
    } else {
      content_stream << line << "\n";
    }
  }
  
  // If no title found, use first line of content or filename
  if (meta.title.empty()) {
    std::string content = content_stream.str();
    std::istringstream content_iss(content);
    if (std::getline(content_iss, line)) {
      // Remove markdown formatting from first line
      meta.title = std::regex_replace(line, std::regex("^#+\\s*"), "");
      if (meta.title.empty()) {
        meta.title = file_path.stem().string();
      }
    }
  }
  
  // Calculate word count
  std::string content = content_stream.str();
  std::istringstream iss(content);
  meta.word_count = std::distance(std::istream_iterator<std::string>(iss),
                                  std::istream_iterator<std::string>());
  
  // Get file timestamps
  std::error_code ec;
  auto file_time = std::filesystem::last_write_time(file_path, ec);
  if (!ec) {
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        file_time - std::filesystem::file_time_type::clock::now() + std::chrono::system_clock::now());
    meta.modified = sctp;
    meta.created = sctp; // We don't have creation time, use modification time
  } else {
    meta.created = meta.modified = std::chrono::system_clock::now();
  }
  
  return meta;
}

Result<std::string> RipgrepIndex::extractSnippet(const std::filesystem::path& file_path,
                                                 const std::string& query_text,
                                                 bool highlight) const {
  std::ifstream file(file_path);
  if (!file) {
    return std::unexpected(makeError(ErrorCode::kFileError,
                                     "Cannot read file for snippet"));
  }
  
  std::string content((std::istreambuf_iterator<char>(file)),
                      std::istreambuf_iterator<char>());
  
  // Find the first occurrence of query text (case insensitive)
  std::string lower_content = content;
  std::string lower_query = query_text;
  std::transform(lower_content.begin(), lower_content.end(), lower_content.begin(), ::tolower);
  std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
  
  size_t pos = lower_content.find(lower_query);
  if (pos == std::string::npos) {
    // Return first 150 characters if no match found
    if (content.length() > 150) {
      return content.substr(0, 150) + "...";
    }
    return content;
  }
  
  // Extract snippet around the match
  size_t snippet_length = 200;
  size_t start = (pos > snippet_length / 2) ? pos - snippet_length / 2 : 0;
  size_t end = std::min(start + snippet_length, content.length());
  
  std::string snippet = content.substr(start, end - start);
  
  // Add highlighting if requested
  if (highlight) {
    std::regex query_regex(query_text, std::regex_constants::icase);
    snippet = std::regex_replace(snippet, query_regex, "<mark>$&</mark>");
  }
  
  // Add ellipsis if needed
  if (start > 0) {
    snippet = "..." + snippet;
  }
  if (end < content.length()) {
    snippet += "...";
  }
  
  return snippet;
}

bool RipgrepIndex::isRipgrepAvailable() const {
  return nx::util::SafeProcess::commandExists("rg");
}

std::string RipgrepIndex::escapeRipgrepQuery(const std::string& query) const {
  // Escape special regex characters for ripgrep
  std::string escaped = query;
  
  // Characters that need escaping in regex
  std::vector<char> special_chars = {'.', '^', '$', '*', '+', '?', '(', ')', '[', ']', '{', '}', '|', '\\'};
  
  for (char c : special_chars) {
    std::string from(1, c);
    std::string to = "\\" + from;
    size_t pos = 0;
    while ((pos = escaped.find(from, pos)) != std::string::npos) {
      escaped.replace(pos, 1, to);
      pos += 2;
    }
  }
  
  return escaped;
}

double RipgrepIndex::calculateSimpleScore(const NoteMeta& meta, const SearchQuery& query) const {
  double score = 1.0;
  
  // Boost score for title matches
  if (!query.text.empty()) {
    std::string lower_title = meta.title;
    std::string lower_query = query.text;
    std::transform(lower_title.begin(), lower_title.end(), lower_title.begin(), ::tolower);
    std::transform(lower_query.begin(), lower_query.end(), lower_query.begin(), ::tolower);
    
    if (lower_title.find(lower_query) != std::string::npos) {
      score += 0.5;
    }
  }
  
  // Boost score for tag matches
  for (const auto& tag : query.tags) {
    if (std::find(meta.tags.begin(), meta.tags.end(), tag) != meta.tags.end()) {
      score += 0.2;
    }
  }
  
  // Boost score for recent notes
  auto now = std::chrono::system_clock::now();
  auto days_old = std::chrono::duration_cast<std::chrono::hours>(now - meta.modified).count() / 24;
  if (days_old < 30) {
    score += 0.1 * (30 - days_old) / 30.0;
  }
  
  return std::min(score, 1.0);
}

} // namespace nx::index