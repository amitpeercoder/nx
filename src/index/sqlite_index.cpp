#include "nx/index/sqlite_index.hpp"

#include <sstream>
#include <algorithm>
#include <regex>
#include <iterator>
#include <numeric>

#include "nx/util/time.hpp"

namespace nx::index {

// SQL schemas and queries
namespace sql {

// Main notes table for metadata
constexpr const char* kCreateNotesTable = R"(
CREATE TABLE IF NOT EXISTS notes (
  id TEXT PRIMARY KEY,
  title TEXT NOT NULL,
  created INTEGER NOT NULL,
  modified INTEGER NOT NULL,
  tags TEXT,  -- JSON array
  notebook TEXT,
  content_length INTEGER DEFAULT 0,
  word_count INTEGER DEFAULT 0
)
)";

// FTS5 table for full-text search
constexpr const char* kCreateFtsTable = R"(
CREATE VIRTUAL TABLE IF NOT EXISTS notes_fts USING fts5(
  id UNINDEXED,
  title,
  content,
  tags,
  notebook
)
)";


// Index for common queries
constexpr const char* kCreateIndexes = R"(
CREATE INDEX IF NOT EXISTS idx_notes_created ON notes(created);
CREATE INDEX IF NOT EXISTS idx_notes_modified ON notes(modified);
CREATE INDEX IF NOT EXISTS idx_notes_notebook ON notes(notebook);
)";

// Performance pragmas
constexpr const char* kPerformancePragmas = R"(
PRAGMA journal_mode = WAL;
PRAGMA synchronous = NORMAL;
PRAGMA cache_size = -64000;  -- 64MB cache
PRAGMA temp_store = MEMORY;
PRAGMA mmap_size = 268435456;  -- 256MB mmap
)";

} // namespace sql

SqliteIndex::SqliteIndex(std::filesystem::path db_path) 
    : db_path_(std::move(db_path)) {
}

SqliteIndex::~SqliteIndex() {
  finalizeStatements();
  if (db_) {
    sqlite3_close(db_);
  }
}

Result<void> SqliteIndex::initialize() {
  std::lock_guard<std::mutex> lock(db_mutex_);
  
  // Ensure parent directory exists
  auto parent = db_path_.parent_path();
  if (!parent.empty() && !std::filesystem::exists(parent)) {
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) {
      return std::unexpected(makeError(ErrorCode::kDirectoryCreateError,
                                       "Failed to create index directory: " + ec.message()));
    }
  }
  
  // Open database
  int result = sqlite3_open(db_path_.c_str(), &db_);
  if (result != SQLITE_OK) {
    return std::unexpected(makeSqliteError("Failed to open database"));
  }
  
  // Configure database for performance
  auto config_result = configureDatabase();
  if (!config_result.has_value()) {
    return config_result;
  }
  
  // Create tables and indexes
  auto tables_result = createTables();
  if (!tables_result.has_value()) {
    return tables_result;
  }
  
  // Prepare statements
  auto prepare_result = prepareStatements();
  if (!prepare_result.has_value()) {
    return prepare_result;
  }
  
  return {};
}

Result<void> SqliteIndex::configureDatabase() {
  // Execute performance pragmas
  auto result = checkSqliteResult(
      sqlite3_exec(db_, sql::kPerformancePragmas, nullptr, nullptr, nullptr),
      "Configure database pragmas");
  if (!result.has_value()) {
    return result;
  }
  
  // Check FTS5 availability by testing virtual table creation
  sqlite3_stmt* stmt;
  int prepare_result = sqlite3_prepare_v2(db_, 
      "CREATE VIRTUAL TABLE fts5_test USING fts5(content)", -1, &stmt, nullptr);
  if (prepare_result != SQLITE_OK) {
    return std::unexpected(makeError(ErrorCode::kDatabaseError,
                                     "FTS5 extension not available"));
  }
  sqlite3_finalize(stmt);
  
  // Clean up test table
  sqlite3_exec(db_, "DROP TABLE IF EXISTS fts5_test", nullptr, nullptr, nullptr);
  
  return {};
}

Result<void> SqliteIndex::createTables() {
  const char* schemas[] = {
    sql::kCreateNotesTable,
    sql::kCreateFtsTable,
    sql::kCreateIndexes
  };
  
  for (const char* schema : schemas) {
    auto result = checkSqliteResult(
        sqlite3_exec(db_, schema, nullptr, nullptr, nullptr),
        "Create database schema");
    if (!result.has_value()) {
      return result;
    }
  }
  
  return {};
}

Result<void> SqliteIndex::prepareStatements() {
  struct Statement {
    const char* sql;
    sqlite3_stmt** stmt;
  };
  
  Statement statements[] = {
    {
      R"(INSERT OR REPLACE INTO notes 
         (id, title, created, modified, tags, notebook, content_length, word_count) 
         VALUES (?, ?, ?, ?, ?, ?, ?, ?))",
      &stmt_add_note_
    },
    {
      R"(INSERT OR REPLACE INTO notes_fts 
         (id, title, content, tags, notebook) 
         VALUES (?, ?, ?, ?, ?))",
      &stmt_update_note_
    },
    {
      R"(DELETE FROM notes WHERE id = ?)",
      &stmt_remove_note_
    },
    {
      R"(DELETE FROM notes_fts WHERE id = ?)",
      &stmt_remove_fts_note_
    },
    {
      R"(SELECT id, title, '', '', tags, notebook,
               snippet(notes_fts, 2, '<mark>', '</mark>', '...', 32) as snippet,
               bm25(notes_fts) as score
         FROM notes_fts
         WHERE notes_fts MATCH ?
         ORDER BY bm25(notes_fts)
         LIMIT ? OFFSET ?)",
      &stmt_search_
    },
    {
      R"(SELECT COUNT(*) FROM notes_fts
         WHERE notes_fts MATCH ?)",
      &stmt_search_count_
    },
    {
      R"(SELECT DISTINCT value as tag 
         FROM notes, json_each(notes.tags)
         WHERE value LIKE ? || '%'
         ORDER BY tag
         LIMIT ?)",
      &stmt_suggest_tags_
    },
    {
      R"(SELECT DISTINCT notebook 
         FROM notes 
         WHERE notebook IS NOT NULL AND notebook LIKE ? || '%'
         ORDER BY notebook
         LIMIT ?)",
      &stmt_suggest_notebooks_
    },
    {
      R"(SELECT COUNT(*) as total_notes,
               SUM(word_count) as total_words,
               MAX(modified) as last_updated
         FROM notes)",
      &stmt_stats_
    }
  };
  
  for (const auto& stmt_def : statements) {
    int result = sqlite3_prepare_v2(db_, stmt_def.sql, -1, stmt_def.stmt, nullptr);
    if (result != SQLITE_OK) {
      return std::unexpected(makeSqliteError("Failed to prepare statement"));
    }
  }
  
  return {};
}

void SqliteIndex::finalizeStatements() {
  sqlite3_stmt* statements[] = {
    stmt_add_note_, stmt_update_note_, stmt_remove_note_, stmt_remove_fts_note_,
    stmt_search_, stmt_search_count_, stmt_suggest_tags_,
    stmt_suggest_notebooks_, stmt_stats_
  };
  
  for (auto stmt : statements) {
    if (stmt) {
      sqlite3_finalize(stmt);
    }
  }
}

Result<void> SqliteIndex::addNote(const nx::core::Note& note) {
  std::lock_guard<std::mutex> lock(db_mutex_);
  
  if (!stmt_add_note_) {
    return std::unexpected(makeError(ErrorCode::kDatabaseError, "Statement not prepared"));
  }
  
  // Bind parameters to notes table
  sqlite3_reset(stmt_add_note_);
  std::string note_id_str = note.id().toString();
  sqlite3_bind_text(stmt_add_note_, 1, note_id_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt_add_note_, 2, note.title().c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt_add_note_, 3, 
      std::chrono::duration_cast<std::chrono::milliseconds>(
          note.metadata().created().time_since_epoch()).count());
  sqlite3_bind_int64(stmt_add_note_, 4,
      std::chrono::duration_cast<std::chrono::milliseconds>(
          note.metadata().updated().time_since_epoch()).count());
  
  // Serialize tags as JSON
  std::ostringstream tags_json;
  tags_json << "[";
  bool first = true;
  for (const auto& tag : note.metadata().tags()) {
    if (!first) tags_json << ",";
    tags_json << "\"" << tag << "\"";
    first = false;
  }
  tags_json << "]";
  
  sqlite3_bind_text(stmt_add_note_, 5, tags_json.str().c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt_add_note_, 6, 
      note.notebook().has_value() ? note.notebook()->c_str() : nullptr, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt_add_note_, 7, static_cast<int>(note.content().length()));
  
  // Simple word count
  std::istringstream iss(note.content());
  auto word_count = static_cast<size_t>(std::distance(std::istream_iterator<std::string>(iss),
                                                      std::istream_iterator<std::string>()));
  sqlite3_bind_int(stmt_add_note_, 8, static_cast<int>(word_count));
  
  // Execute
  int result = sqlite3_step(stmt_add_note_);
  if (result != SQLITE_DONE) {
    return std::unexpected(makeSqliteError("Failed to insert note"));
  }
  
  // Update FTS content
  sqlite3_reset(stmt_update_note_);
  sqlite3_bind_text(stmt_update_note_, 1, note_id_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt_update_note_, 2, note.title().c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt_update_note_, 3, note.content().c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt_update_note_, 4, tags_json.str().c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt_update_note_, 5, 
      note.notebook().has_value() ? note.notebook()->c_str() : nullptr, -1, SQLITE_STATIC);
  
  result = sqlite3_step(stmt_update_note_);
  if (result != SQLITE_DONE) {
    return std::unexpected(makeSqliteError("Failed to update FTS content"));
  }
  
  return {};
}

Result<void> SqliteIndex::updateNote(const nx::core::Note& note) {
  std::lock_guard<std::mutex> lock(db_mutex_);
  
  if (!stmt_remove_note_ || !stmt_remove_fts_note_ || !stmt_add_note_ || !stmt_update_note_) {
    return std::unexpected(makeError(ErrorCode::kDatabaseError, "Statement not prepared"));
  }
  
  std::string note_id_str = note.id().toString();
  
  // For FTS5, we need explicit DELETE + INSERT since REPLACE doesn't work as expected
  // First remove existing FTS data
  sqlite3_reset(stmt_remove_fts_note_);
  sqlite3_bind_text(stmt_remove_fts_note_, 1, note_id_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_step(stmt_remove_fts_note_); // Don't check result - might not exist
  
  // Update notes table (this supports REPLACE properly)
  sqlite3_reset(stmt_add_note_);
  sqlite3_bind_text(stmt_add_note_, 1, note_id_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt_add_note_, 2, note.title().c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int64(stmt_add_note_, 3, 
      std::chrono::duration_cast<std::chrono::milliseconds>(
          note.metadata().created().time_since_epoch()).count());
  sqlite3_bind_int64(stmt_add_note_, 4,
      std::chrono::duration_cast<std::chrono::milliseconds>(
          note.metadata().updated().time_since_epoch()).count());
  
  // Serialize tags as JSON
  std::ostringstream tags_json;
  tags_json << "[";
  bool first = true;
  for (const auto& tag : note.metadata().tags()) {
    if (!first) tags_json << ",";
    tags_json << "\"" << tag << "\"";
    first = false;
  }
  tags_json << "]";
  
  sqlite3_bind_text(stmt_add_note_, 5, tags_json.str().c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt_add_note_, 6, 
      note.notebook().has_value() ? note.notebook()->c_str() : nullptr, -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt_add_note_, 7, static_cast<int>(note.content().length()));
  
  // Simple word count
  std::istringstream iss(note.content());
  auto word_count = static_cast<size_t>(std::distance(std::istream_iterator<std::string>(iss),
                                                      std::istream_iterator<std::string>()));
  sqlite3_bind_int(stmt_add_note_, 8, static_cast<int>(word_count));
  
  // Execute notes table update
  int result = sqlite3_step(stmt_add_note_);
  if (result != SQLITE_DONE) {
    return std::unexpected(makeSqliteError("Failed to update note"));
  }
  
  // Insert new FTS content
  sqlite3_reset(stmt_update_note_);
  sqlite3_bind_text(stmt_update_note_, 1, note_id_str.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt_update_note_, 2, note.title().c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt_update_note_, 3, note.content().c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_text(stmt_update_note_, 4, tags_json.str().c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_text(stmt_update_note_, 5, 
      note.notebook().has_value() ? note.notebook()->c_str() : nullptr, -1, SQLITE_STATIC);
  
  result = sqlite3_step(stmt_update_note_);
  if (result != SQLITE_DONE) {
    return std::unexpected(makeSqliteError("Failed to update FTS content"));
  }
  
  return {};
}

Result<void> SqliteIndex::removeNote(const nx::core::NoteId& id) {
  std::lock_guard<std::mutex> lock(db_mutex_);
  
  if (!stmt_remove_note_ || !stmt_remove_fts_note_) {
    return std::unexpected(makeError(ErrorCode::kDatabaseError, "Statement not prepared"));
  }
  
  std::string id_str = id.toString();
  
  // Remove from notes table
  sqlite3_reset(stmt_remove_note_);
  sqlite3_bind_text(stmt_remove_note_, 1, id_str.c_str(), -1, SQLITE_TRANSIENT);
  
  int result = sqlite3_step(stmt_remove_note_);
  if (result != SQLITE_DONE) {
    return std::unexpected(makeSqliteError("Failed to remove note"));
  }
  
  // Remove from FTS table
  sqlite3_reset(stmt_remove_fts_note_);
  sqlite3_bind_text(stmt_remove_fts_note_, 1, id_str.c_str(), -1, SQLITE_TRANSIENT);
  
  result = sqlite3_step(stmt_remove_fts_note_);
  if (result != SQLITE_DONE) {
    return std::unexpected(makeSqliteError("Failed to remove FTS note"));
  }
  
  return {};
}

Result<std::vector<SearchResult>> SqliteIndex::search(const SearchQuery& query) {
  std::lock_guard<std::mutex> lock(db_mutex_);
  
  if (!stmt_search_) {
    return std::unexpected(makeError(ErrorCode::kDatabaseError, "Statement not prepared"));
  }
  
  // Build FTS query
  std::string fts_query = buildFtsQuery(query);
  if (fts_query.empty()) {
    return std::vector<SearchResult>{}; // Empty query returns no results
  }
  
  sqlite3_reset(stmt_search_);
  sqlite3_bind_text(stmt_search_, 1, fts_query.c_str(), -1, SQLITE_TRANSIENT);
  sqlite3_bind_int(stmt_search_, 2, static_cast<int>(query.limit));
  sqlite3_bind_int(stmt_search_, 3, static_cast<int>(query.offset));
  
  std::vector<SearchResult> results;
  
  while (true) {
    int result = sqlite3_step(stmt_search_);
    if (result == SQLITE_DONE) {
      break;
    } else if (result != SQLITE_ROW) {
      return std::unexpected(makeSqliteError("Search query failed"));
    }
    
    auto search_result = extractSearchResult(stmt_search_, query.highlight);
    if (search_result.has_value()) {
      results.push_back(*search_result);
    }
  }
  
  return results;
}

Result<std::vector<nx::core::NoteId>> SqliteIndex::searchIds(const SearchQuery& query) {
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

Result<size_t> SqliteIndex::searchCount(const SearchQuery& query) {
  std::lock_guard<std::mutex> lock(db_mutex_);
  
  if (!stmt_search_count_) {
    return std::unexpected(makeError(ErrorCode::kDatabaseError, "Statement not prepared"));
  }
  
  std::string fts_query = buildFtsQuery(query);
  if (fts_query.empty()) {
    return 0;
  }
  
  sqlite3_reset(stmt_search_count_);
  sqlite3_bind_text(stmt_search_count_, 1, fts_query.c_str(), -1, SQLITE_TRANSIENT);
  
  int result = sqlite3_step(stmt_search_count_);
  if (result != SQLITE_ROW) {
    return std::unexpected(makeSqliteError("Count query failed"));
  }
  
  return static_cast<size_t>(sqlite3_column_int64(stmt_search_count_, 0));
}

std::string SqliteIndex::buildFtsQuery(const SearchQuery& query) {
  if (query.text.empty()) {
    return "";
  }
  
  // Use the query text directly - FTS5 handles basic escaping
  // FTS5 query syntax is well-defined and safe to use directly
  std::string fts_query = query.text;
  
  // Add column filters if needed
  std::vector<std::string> conditions;
  
  if (!query.tags.empty()) {
    for (const auto& tag : query.tags) {
      conditions.push_back("tags:\"" + tag + "\"");
    }
  }
  
  if (query.notebook.has_value()) {
    conditions.push_back("notebook:\"" + *query.notebook + "\"");
  }
  
  if (!conditions.empty()) {
    fts_query += " AND " + std::accumulate(
        conditions.begin() + 1, conditions.end(), conditions[0],
        [](const std::string& a, const std::string& b) {
          return a + " AND " + b;
        });
  }
  
  return fts_query;
}

Result<SearchResult> SqliteIndex::extractSearchResult(sqlite3_stmt* stmt, bool highlight) {
  SearchResult result;
  
  // Extract basic fields
  const char* id_str = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
  if (!id_str) {
    return std::unexpected(makeError(ErrorCode::kDatabaseError, "Invalid note ID"));
  }
  
  auto id_result = nx::core::NoteId::fromString(id_str);
  if (!id_result.has_value()) {
    return std::unexpected(id_result.error());
  }
  result.id = *id_result;
  
  const char* title = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 1));
  result.title = title ? title : "";
  
  // Use current time as modified timestamp for FTS-only results
  // This is a design trade-off for search performance vs timestamp accuracy
  result.modified = std::chrono::system_clock::now();
  
  // Extract tags (JSON array) - column 4
  const char* tags_json = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 4));
  if (tags_json) {
    // Parse JSON tags using regex for simplicity and performance
    std::string tags_str(tags_json);
    std::regex tag_regex("\"([^\"]+)\"");
    std::sregex_iterator iter(tags_str.begin(), tags_str.end(), tag_regex);
    std::sregex_iterator end;
    
    for (; iter != end; ++iter) {
      result.tags.push_back((*iter)[1].str());
    }
  }
  
  // Extract notebook - column 5
  const char* notebook = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 5));
  if (notebook) {
    result.notebook = std::string(notebook);
  }
  
  // Extract snippet and score
  if (highlight) {
    const char* snippet = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 6));
    result.snippet = snippet ? snippet : "";
  }
  
  double score = sqlite3_column_double(stmt, 7);
  result.score = std::max(0.0, std::min(1.0, -score / 10.0)); // Normalize BM25 score
  
  return result;
}

Result<std::vector<std::string>> SqliteIndex::suggestTags(const std::string& prefix, size_t limit) {
  std::lock_guard<std::mutex> lock(db_mutex_);
  
  if (!stmt_suggest_tags_) {
    return std::unexpected(makeError(ErrorCode::kDatabaseError, "Statement not prepared"));
  }
  
  sqlite3_reset(stmt_suggest_tags_);
  sqlite3_bind_text(stmt_suggest_tags_, 1, prefix.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt_suggest_tags_, 2, static_cast<int>(limit));
  
  std::vector<std::string> suggestions;
  
  while (true) {
    int result = sqlite3_step(stmt_suggest_tags_);
    if (result == SQLITE_DONE) {
      break;
    } else if (result != SQLITE_ROW) {
      return std::unexpected(makeSqliteError("Tag suggestion query failed"));
    }
    
    const char* tag = reinterpret_cast<const char*>(sqlite3_column_text(stmt_suggest_tags_, 0));
    if (tag) {
      suggestions.emplace_back(tag);
    }
  }
  
  return suggestions;
}

Result<std::vector<std::string>> SqliteIndex::suggestNotebooks(const std::string& prefix, size_t limit) {
  std::lock_guard<std::mutex> lock(db_mutex_);
  
  if (!stmt_suggest_notebooks_) {
    return std::unexpected(makeError(ErrorCode::kDatabaseError, "Statement not prepared"));
  }
  
  sqlite3_reset(stmt_suggest_notebooks_);
  sqlite3_bind_text(stmt_suggest_notebooks_, 1, prefix.c_str(), -1, SQLITE_STATIC);
  sqlite3_bind_int(stmt_suggest_notebooks_, 2, static_cast<int>(limit));
  
  std::vector<std::string> suggestions;
  
  while (true) {
    int result = sqlite3_step(stmt_suggest_notebooks_);
    if (result == SQLITE_DONE) {
      break;
    } else if (result != SQLITE_ROW) {
      return std::unexpected(makeSqliteError("Notebook suggestion query failed"));
    }
    
    const char* notebook = reinterpret_cast<const char*>(sqlite3_column_text(stmt_suggest_notebooks_, 0));
    if (notebook) {
      suggestions.emplace_back(notebook);
    }
  }
  
  return suggestions;
}

Result<IndexStats> SqliteIndex::getStats() {
  std::lock_guard<std::mutex> lock(db_mutex_);
  
  if (!stmt_stats_) {
    return std::unexpected(makeError(ErrorCode::kDatabaseError, "Statement not prepared"));
  }
  
  sqlite3_reset(stmt_stats_);
  
  int result = sqlite3_step(stmt_stats_);
  if (result != SQLITE_ROW) {
    return std::unexpected(makeSqliteError("Stats query failed"));
  }
  
  IndexStats stats;
  stats.total_notes = static_cast<size_t>(sqlite3_column_int64(stmt_stats_, 0));
  stats.total_words = static_cast<size_t>(sqlite3_column_int64(stmt_stats_, 1));
  
  int64_t last_updated_ms = sqlite3_column_int64(stmt_stats_, 2);
  stats.last_updated = std::chrono::system_clock::time_point{
      std::chrono::milliseconds(last_updated_ms)};
  
  // Get database file size
  std::error_code ec;
  auto file_size = std::filesystem::file_size(db_path_, ec);
  if (!ec) {
    stats.index_size_bytes = file_size;
  }
  
  // Set last_optimized to current time since we don't track optimization history
  stats.last_optimized = std::chrono::system_clock::now();
  
  return stats;
}

Result<bool> SqliteIndex::isHealthy() {
  std::lock_guard<std::mutex> lock(db_mutex_);
  
  if (!db_) {
    return false;
  }
  
  // Simple health check - execute a basic query
  sqlite3_stmt* stmt;
  int result = sqlite3_prepare_v2(db_, "SELECT 1", -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    return false;
  }
  
  result = sqlite3_step(stmt);
  sqlite3_finalize(stmt);
  
  return result == SQLITE_ROW;
}

Result<void> SqliteIndex::validateIndex() {
  std::lock_guard<std::mutex> lock(db_mutex_);
  
  // Run PRAGMA integrity_check
  sqlite3_stmt* stmt;
  int result = sqlite3_prepare_v2(db_, "PRAGMA integrity_check", -1, &stmt, nullptr);
  if (result != SQLITE_OK) {
    return std::unexpected(makeSqliteError("Failed to prepare integrity check"));
  }
  
  result = sqlite3_step(stmt);
  if (result == SQLITE_ROW) {
    const char* check_result = reinterpret_cast<const char*>(sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);
    
    if (check_result && std::string(check_result) != "ok") {
      return std::unexpected(makeError(ErrorCode::kDatabaseError, 
                                       "Database integrity check failed: " + std::string(check_result)));
    }
  } else {
    sqlite3_finalize(stmt);
    return std::unexpected(makeSqliteError("Integrity check failed"));
  }
  
  return {};
}

Result<void> SqliteIndex::rebuild() {
  std::lock_guard<std::mutex> lock(db_mutex_);
  
  // Rebuild FTS index
  auto result = checkSqliteResult(
      sqlite3_exec(db_, "INSERT INTO notes_fts(notes_fts) VALUES('rebuild')", 
                   nullptr, nullptr, nullptr),
      "Rebuild FTS index");
  
  return result;
}

Result<void> SqliteIndex::optimize() {
  std::lock_guard<std::mutex> lock(db_mutex_);
  
  // Optimize FTS index
  auto result = checkSqliteResult(
      sqlite3_exec(db_, "INSERT INTO notes_fts(notes_fts) VALUES('optimize')", 
                   nullptr, nullptr, nullptr),
      "Optimize FTS index");
  if (!result.has_value()) {
    return result;
  }
  
  // VACUUM to reclaim space
  result = checkSqliteResult(
      sqlite3_exec(db_, "VACUUM", nullptr, nullptr, nullptr),
      "VACUUM database");
  
  return result;
}

Result<void> SqliteIndex::beginTransaction() {
  std::lock_guard<std::mutex> lock(db_mutex_);
  
  if (in_transaction_) {
    return std::unexpected(makeError(ErrorCode::kDatabaseError, "Transaction already active"));
  }
  
  auto result = checkSqliteResult(
      sqlite3_exec(db_, "BEGIN IMMEDIATE", nullptr, nullptr, nullptr),
      "Begin transaction");
  
  if (result.has_value()) {
    in_transaction_ = true;
  }
  
  return result;
}

Result<void> SqliteIndex::commitTransaction() {
  std::lock_guard<std::mutex> lock(db_mutex_);
  
  if (!in_transaction_) {
    return std::unexpected(makeError(ErrorCode::kDatabaseError, "No active transaction"));
  }
  
  auto result = checkSqliteResult(
      sqlite3_exec(db_, "COMMIT", nullptr, nullptr, nullptr),
      "Commit transaction");
  
  in_transaction_ = false;
  return result;
}

Result<void> SqliteIndex::rollbackTransaction() {
  std::lock_guard<std::mutex> lock(db_mutex_);
  
  if (!in_transaction_) {
    return std::unexpected(makeError(ErrorCode::kDatabaseError, "No active transaction"));
  }
  
  auto result = checkSqliteResult(
      sqlite3_exec(db_, "ROLLBACK", nullptr, nullptr, nullptr),
      "Rollback transaction");
  
  in_transaction_ = false;
  return result;
}

Error SqliteIndex::makeSqliteError(const std::string& operation) {
  std::string message = operation;
  if (db_) {
    message += ": " + std::string(sqlite3_errmsg(db_));
  }
  return makeError(ErrorCode::kDatabaseError, message);
}

Result<void> SqliteIndex::checkSqliteResult(int result, const std::string& operation) {
  if (result == SQLITE_OK || result == SQLITE_DONE || result == SQLITE_ROW) {
    return {};
  }
  return std::unexpected(makeSqliteError(operation));
}

}  // namespace nx::index