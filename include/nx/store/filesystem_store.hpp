#pragma once

#include <filesystem>
#include <unordered_map>
#include <mutex>

#include "nx/store/note_store.hpp"
#include "nx/util/xdg.hpp"

namespace nx::store {

// Filesystem-based note storage implementation
class FilesystemStore : public NoteStore {
 public:
  struct Config {
    std::filesystem::path notes_dir;
    std::filesystem::path attachments_dir;
    std::filesystem::path trash_dir;
    bool auto_create_dirs = true;
    bool validate_paths = true;
  };

  FilesystemStore();
  explicit FilesystemStore(Config config);
  ~FilesystemStore() override = default;

  // NoteStore interface implementation
  Result<void> store(const nx::core::Note& note) override;
  Result<nx::core::Note> load(const nx::core::NoteId& id) override;
  Result<void> remove(const nx::core::NoteId& id, bool soft_delete = true) override;
  Result<bool> exists(const nx::core::NoteId& id) override;

  Result<void> storeBatch(const std::vector<nx::core::Note>& notes) override;
  Result<std::vector<nx::core::Note>> loadBatch(const std::vector<nx::core::NoteId>& ids) override;

  Result<std::vector<nx::core::NoteId>> list(const NoteQuery& query = {}) override;
  Result<std::vector<nx::core::Note>> search(const NoteQuery& query = {}) override;
  Result<size_t> count(const NoteQuery& query = {}) override;

  Result<std::vector<FuzzyMatch>> fuzzyResolve(const std::string& partial_id, 
                                               size_t max_results = 10) override;
  Result<nx::core::NoteId> resolveSingle(const std::string& partial_id) override;

  Result<std::vector<std::string>> getAllTags() override;
  Result<std::vector<std::string>> getAllNotebooks() override;
  Result<std::vector<nx::core::NoteId>> getBacklinks(const nx::core::NoteId& id) override;

  Result<std::vector<nx::core::NoteId>> listTrashed() override;
  Result<void> restore(const nx::core::NoteId& id) override;
  Result<void> permanentlyDelete(const nx::core::NoteId& id) override;
  Result<void> emptyTrash() override;

  Result<size_t> totalNotes() override;
  Result<size_t> totalSize() override;
  Result<std::chrono::system_clock::time_point> lastModified() override;

  Result<void> rebuild() override;
  Result<void> vacuum() override;
  Result<void> validate() override;

  void setChangeCallback(ChangeCallback callback) override;

  // FilesystemStore specific methods
  const Config& config() const { return config_; }
  
  // Get file path for note
  std::filesystem::path getNotePath(const nx::core::NoteId& id) const;
  std::filesystem::path getTrashPath(const nx::core::NoteId& id) const;
  
  // Directory operations
  Result<void> ensureDirectories();
  
  // Cache management
  void clearCache();
  void invalidateCache(const nx::core::NoteId& id);

 private:
  Config config_;
  ChangeCallback change_callback_;
  
  // Cache for metadata (thread-safe)
  mutable std::mutex cache_mutex_;
  mutable std::unordered_map<nx::core::NoteId, nx::core::Metadata> metadata_cache_;
  mutable std::optional<std::chrono::system_clock::time_point> cache_refresh_time_;
  
  // Internal operations
  Result<std::filesystem::path> findNoteFile(const nx::core::NoteId& id) const;
  Result<std::vector<std::filesystem::path>> getAllNoteFiles() const;
  Result<std::vector<std::filesystem::path>> getAllTrashFiles() const;
  
  // Fuzzy matching helpers
  std::vector<FuzzyMatch> performFuzzyMatch(const std::string& partial_id, 
                                            const std::vector<nx::core::NoteId>& candidates,
                                            size_t max_results) const;
  double calculateMatchScore(const std::string& partial_id, const nx::core::NoteId& id,
                            const std::string& title) const;
  
  // Query filtering
  bool matchesQuery(const nx::core::Note& note, const NoteQuery& query) const;
  std::vector<nx::core::Note> applyQueryFilters(std::vector<nx::core::Note> notes, 
                                                const NoteQuery& query) const;
  
  // Cache operations
  void updateMetadataCache(const nx::core::Note& note) const;
  std::optional<nx::core::Metadata> getCachedMetadata(const nx::core::NoteId& id) const;
  void refreshMetadataCache() const;
  
  // Notification helpers
  void notifyChange(const nx::core::NoteId& id, const std::string& operation);
  
  // Validation helpers
  Result<void> validateNoteFile(const std::filesystem::path& path) const;
  
  // File operations with error handling
  Result<void> moveToTrash(const nx::core::NoteId& id);
  Result<void> restoreFromTrash(const nx::core::NoteId& id);
};

}  // namespace nx::store