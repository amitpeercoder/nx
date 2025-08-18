#pragma once

#include <string>
#include <vector>
#include <optional>
#include <chrono>
#include <filesystem>

#include "nx/common.hpp"
#include "nx/core/note_id.hpp"
#include "nx/core/note.hpp"

namespace nx::index {

// Search result with ranking and metadata
struct SearchResult {
  nx::core::NoteId id;
  std::string title;
  std::string snippet;  // Highlighted excerpt
  double score;         // Relevance score (0.0 - 1.0)
  std::chrono::system_clock::time_point modified;
  std::vector<std::string> tags;
  std::optional<std::string> notebook;
};

// Search query configuration
struct SearchQuery {
  std::string text;                    // FTS query text
  std::vector<std::string> tags;       // Must have all these tags
  std::optional<std::string> notebook; // Must be in this notebook
  std::optional<std::chrono::system_clock::time_point> since;
  std::optional<std::chrono::system_clock::time_point> until;
  size_t limit = 50;                   // Max results
  size_t offset = 0;                   // Pagination offset
  bool highlight = true;               // Include snippet highlighting
};

// Index statistics
struct IndexStats {
  size_t total_notes = 0;
  size_t total_words = 0;
  size_t index_size_bytes = 0;
  std::chrono::system_clock::time_point last_updated;
  std::chrono::system_clock::time_point last_optimized;
};

// Abstract index interface
class Index {
public:
  virtual ~Index() = default;

  // Index management
  virtual Result<void> initialize() = 0;
  virtual Result<void> addNote(const nx::core::Note& note) = 0;
  virtual Result<void> updateNote(const nx::core::Note& note) = 0;
  virtual Result<void> removeNote(const nx::core::NoteId& id) = 0;
  virtual Result<void> rebuild() = 0;
  virtual Result<void> optimize() = 0;
  virtual Result<void> vacuum() = 0;

  // Search operations
  virtual Result<std::vector<SearchResult>> search(const SearchQuery& query) = 0;
  virtual Result<std::vector<nx::core::NoteId>> searchIds(const SearchQuery& query) = 0;
  virtual Result<size_t> searchCount(const SearchQuery& query) = 0;

  // Suggestions and autocompletion
  virtual Result<std::vector<std::string>> suggestTags(const std::string& prefix, size_t limit = 10) = 0;
  virtual Result<std::vector<std::string>> suggestNotebooks(const std::string& prefix, size_t limit = 10) = 0;
  
  // Statistics and health
  virtual Result<IndexStats> getStats() = 0;
  virtual Result<bool> isHealthy() = 0;
  virtual Result<void> validateIndex() = 0;

  // Batch operations for performance
  virtual Result<void> beginTransaction() = 0;
  virtual Result<void> commitTransaction() = 0;
  virtual Result<void> rollbackTransaction() = 0;
};

// Index factory
class IndexFactory {
public:
  static std::unique_ptr<Index> createSqliteIndex(const std::filesystem::path& db_path);
  static std::unique_ptr<Index> createRipgrepIndex(const std::filesystem::path& notes_dir);
};

}  // namespace nx::index