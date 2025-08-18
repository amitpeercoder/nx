#pragma once

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <chrono>

#include "nx/index/index.hpp"
#include "nx/core/note.hpp"
#include "nx/common.hpp"

namespace nx::index {

/**
 * @brief Ripgrep-based search index fallback for systems without SQLite FTS5
 * 
 * This implementation uses ripgrep (rg) for full-text search and maintains
 * a simple metadata cache for tag/notebook filtering and statistics.
 * It's designed as a fallback when SQLite FTS5 is not available.
 */
class RipgrepIndex : public Index {
public:
  explicit RipgrepIndex(std::filesystem::path notes_dir);
  ~RipgrepIndex() override = default;

  // Index lifecycle
  Result<void> initialize() override;
  
  // Note operations
  Result<void> addNote(const nx::core::Note& note) override;
  Result<void> updateNote(const nx::core::Note& note) override;
  Result<void> removeNote(const nx::core::NoteId& id) override;
  
  // Search operations
  Result<std::vector<SearchResult>> search(const SearchQuery& query) override;
  Result<std::vector<nx::core::NoteId>> searchIds(const SearchQuery& query) override;
  Result<size_t> searchCount(const SearchQuery& query) override;
  
  // Suggestions
  Result<std::vector<std::string>> suggestTags(const std::string& prefix, size_t limit) override;
  Result<std::vector<std::string>> suggestNotebooks(const std::string& prefix, size_t limit) override;
  
  // Statistics and maintenance
  Result<IndexStats> getStats() override;
  Result<bool> isHealthy() override;
  Result<void> validateIndex() override;
  Result<void> rebuild() override;
  Result<void> optimize() override;
  Result<void> vacuum() override;
  
  // Transaction support (no-ops for ripgrep)
  Result<void> beginTransaction() override;
  Result<void> commitTransaction() override;
  Result<void> rollbackTransaction() override;

private:
  struct NoteMeta {
    nx::core::NoteId id;
    std::string title;
    std::filesystem::path file_path;
    std::chrono::system_clock::time_point created;
    std::chrono::system_clock::time_point modified;
    std::vector<std::string> tags;
    std::optional<std::string> notebook;
    size_t word_count = 0;
  };
  
  // Core functionality
  Result<void> buildMetadataCache();
  Result<void> updateNoteMetadata(const nx::core::Note& note);
  Result<void> removeNoteMetadata(const nx::core::NoteId& id);
  
  // Search implementation
  Result<std::vector<std::filesystem::path>> ripgrepSearch(const std::string& query_text, 
                                                           size_t limit = 1000) const;
  Result<SearchResult> createSearchResult(const std::filesystem::path& file_path,
                                          const SearchQuery& query) const;
  std::vector<NoteMeta> filterByMetadata(const std::vector<NoteMeta>& candidates,
                                         const SearchQuery& query) const;
  
  // Utilities
  Result<NoteMeta> parseNoteFile(const std::filesystem::path& file_path) const;
  Result<std::string> extractSnippet(const std::filesystem::path& file_path,
                                     const std::string& query_text,
                                     bool highlight) const;
  bool isRipgrepAvailable() const;
  std::string escapeRipgrepQuery(const std::string& query) const;
  double calculateSimpleScore(const NoteMeta& meta, const SearchQuery& query) const;
  
  // Data members
  std::filesystem::path notes_dir_;
  mutable std::mutex cache_mutex_;
  std::unordered_map<std::string, NoteMeta> metadata_cache_; // ID -> metadata
  std::chrono::system_clock::time_point last_cache_update_;
  bool cache_dirty_ = true;
};

} // namespace nx::index