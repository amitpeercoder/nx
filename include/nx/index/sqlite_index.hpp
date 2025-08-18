#pragma once

#include <sqlite3.h>
#include <memory>
#include <mutex>
#include <filesystem>

#include "nx/index/index.hpp"

namespace nx::index {

// SQLite FTS5-based search index implementation
class SqliteIndex : public Index {
public:
  explicit SqliteIndex(std::filesystem::path db_path);
  ~SqliteIndex() override;

  // Index management
  Result<void> initialize() override;
  Result<void> addNote(const nx::core::Note& note) override;
  Result<void> updateNote(const nx::core::Note& note) override;
  Result<void> removeNote(const nx::core::NoteId& id) override;
  Result<void> rebuild() override;
  Result<void> optimize() override;
  Result<void> vacuum() override;

  // Search operations
  Result<std::vector<SearchResult>> search(const SearchQuery& query) override;
  Result<std::vector<nx::core::NoteId>> searchIds(const SearchQuery& query) override;
  Result<size_t> searchCount(const SearchQuery& query) override;

  // Suggestions and autocompletion
  Result<std::vector<std::string>> suggestTags(const std::string& prefix, size_t limit = 10) override;
  Result<std::vector<std::string>> suggestNotebooks(const std::string& prefix, size_t limit = 10) override;
  
  // Statistics and health
  Result<IndexStats> getStats() override;
  Result<bool> isHealthy() override;
  Result<void> validateIndex() override;

  // Batch operations for performance
  Result<void> beginTransaction() override;
  Result<void> commitTransaction() override;
  Result<void> rollbackTransaction() override;

private:
  // Database management
  Result<void> createTables();
  Result<void> configureDatabase();
  Result<void> ensureCompatibility();
  
  // SQL statement preparation
  Result<void> prepareStatements();
  void finalizeStatements();
  
  // Query building
  std::string buildFtsQuery(const SearchQuery& query);
  std::string buildWhereClause(const SearchQuery& query, std::vector<std::string>& params);
  
  // Result processing
  Result<SearchResult> extractSearchResult(sqlite3_stmt* stmt, bool highlight);
  std::string generateSnippet(const std::string& content, const std::string& query, size_t max_length = 200);
  
  // Error handling
  Error makeSqliteError(const std::string& operation);
  Result<void> checkSqliteResult(int result, const std::string& operation);
  
  // Database path and connection
  std::filesystem::path db_path_;
  sqlite3* db_ = nullptr;
  std::mutex db_mutex_;
  
  // Prepared statements for common operations
  sqlite3_stmt* stmt_add_note_ = nullptr;
  sqlite3_stmt* stmt_update_note_ = nullptr;
  sqlite3_stmt* stmt_remove_note_ = nullptr;
  sqlite3_stmt* stmt_remove_fts_note_ = nullptr;
  sqlite3_stmt* stmt_search_ = nullptr;
  sqlite3_stmt* stmt_search_count_ = nullptr;
  sqlite3_stmt* stmt_suggest_tags_ = nullptr;
  sqlite3_stmt* stmt_suggest_notebooks_ = nullptr;
  sqlite3_stmt* stmt_stats_ = nullptr;
  
  // Transaction state
  bool in_transaction_ = false;
};

}  // namespace nx::index