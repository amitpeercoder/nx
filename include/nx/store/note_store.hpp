#pragma once

#include <functional>
#include <string>
#include <vector>
#include <optional>

#include "nx/common.hpp"
#include "nx/core/note.hpp"
#include "nx/core/note_id.hpp"

namespace nx::store {

// Filter and sorting options for note queries
struct NoteQuery {
  std::optional<std::string> notebook;
  std::vector<std::string> tags;
  std::optional<std::chrono::system_clock::time_point> since;
  std::optional<std::chrono::system_clock::time_point> until;
  std::optional<std::string> title_contains;
  std::optional<std::string> content_contains;
  size_t limit = 0;  // 0 = no limit
  size_t offset = 0;
  
  enum class SortBy {
    kCreated,
    kUpdated,
    kTitle
  };
  SortBy sort_by = SortBy::kUpdated;
  
  enum class SortOrder {
    kAscending,
    kDescending
  };
  SortOrder sort_order = SortOrder::kDescending;
};

// Result of fuzzy search for note IDs
struct FuzzyMatch {
  nx::core::NoteId id;
  std::string display_text;  // Title or slug for display
  double score;  // Match confidence (0.0 - 1.0)
};

// Abstract interface for note storage
class NoteStore {
 public:
  virtual ~NoteStore() = default;

  // CRUD operations
  virtual Result<void> store(const nx::core::Note& note) = 0;
  virtual Result<nx::core::Note> load(const nx::core::NoteId& id) = 0;
  virtual Result<void> remove(const nx::core::NoteId& id, bool soft_delete = true) = 0;
  virtual Result<bool> exists(const nx::core::NoteId& id) = 0;

  // Batch operations
  virtual Result<void> storeBatch(const std::vector<nx::core::Note>& notes) = 0;
  virtual Result<std::vector<nx::core::Note>> loadBatch(const std::vector<nx::core::NoteId>& ids) = 0;

  // Query operations
  virtual Result<std::vector<nx::core::NoteId>> list(const NoteQuery& query = {}) = 0;
  virtual Result<std::vector<nx::core::Note>> search(const NoteQuery& query = {}) = 0;
  virtual Result<size_t> count(const NoteQuery& query = {}) = 0;

  // Fuzzy resolution
  virtual Result<std::vector<FuzzyMatch>> fuzzyResolve(const std::string& partial_id, 
                                                       size_t max_results = 10) = 0;
  virtual Result<nx::core::NoteId> resolveSingle(const std::string& partial_id) = 0;

  // Metadata operations
  virtual Result<std::vector<std::string>> getAllTags() = 0;
  virtual Result<std::vector<std::string>> getAllNotebooks() = 0;
  virtual Result<std::vector<nx::core::NoteId>> getBacklinks(const nx::core::NoteId& id) = 0;

  // Trash operations (soft delete)
  virtual Result<std::vector<nx::core::NoteId>> listTrashed() = 0;
  virtual Result<void> restore(const nx::core::NoteId& id) = 0;
  virtual Result<void> permanentlyDelete(const nx::core::NoteId& id) = 0;
  virtual Result<void> emptyTrash() = 0;

  // Statistics
  virtual Result<size_t> totalNotes() = 0;
  virtual Result<size_t> totalSize() = 0;
  virtual Result<std::chrono::system_clock::time_point> lastModified() = 0;

  // Maintenance
  virtual Result<void> rebuild() = 0;
  virtual Result<void> vacuum() = 0;
  virtual Result<void> validate() = 0;

  // Callbacks for external indexing
  using ChangeCallback = std::function<void(const nx::core::NoteId&, const std::string& operation)>;
  virtual void setChangeCallback(ChangeCallback callback) = 0;
};

}  // namespace nx::store