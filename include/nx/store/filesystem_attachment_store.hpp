#pragma once

#include <filesystem>
#include <unordered_map>
#include <mutex>

#include "nx/store/attachment_store.hpp"

namespace nx::store {

// Filesystem-based attachment storage
class FilesystemAttachmentStore : public AttachmentStore {
 public:
  struct Config {
    std::filesystem::path attachments_dir;
    std::filesystem::path metadata_file;  // JSON file storing attachment metadata
    bool auto_create_dirs = true;
    std::uintmax_t max_file_size = 100 * 1024 * 1024;  // 100MB default limit
    std::vector<std::string> allowed_extensions = {
        ".jpg", ".jpeg", ".png", ".gif", ".svg", ".webp",  // Images
        ".pdf", ".txt", ".md", ".doc", ".docx",            // Documents
        ".mp3", ".wav", ".ogg", ".mp4", ".webm",           // Media
        ".zip", ".tar", ".gz", ".json", ".xml", ".csv"     // Data
    };
  };

  FilesystemAttachmentStore();
  explicit FilesystemAttachmentStore(Config config);
  ~FilesystemAttachmentStore() override = default;

  // AttachmentStore interface
  Result<AttachmentInfo> store(const nx::core::NoteId& parent_note,
                               const std::filesystem::path& source_file,
                               const std::string& description = "") override;

  Result<AttachmentInfo> storeData(const nx::core::NoteId& parent_note,
                                   const std::string& data,
                                   const std::string& filename,
                                   const std::string& mime_type = "",
                                   const std::string& description = "") override;

  Result<std::string> loadData(const nx::core::NoteId& attachment_id) override;
  Result<AttachmentInfo> getInfo(const nx::core::NoteId& attachment_id) override;
  Result<void> remove(const nx::core::NoteId& attachment_id) override;

  Result<std::vector<AttachmentInfo>> listForNote(const nx::core::NoteId& note_id) override;
  Result<std::vector<AttachmentInfo>> listAll() override;

  Result<std::filesystem::path> getPath(const nx::core::NoteId& attachment_id) override;
  Result<void> exportTo(const nx::core::NoteId& attachment_id,
                        const std::filesystem::path& target_path) override;

  Result<size_t> totalAttachments() override;
  Result<std::uintmax_t> totalSize() override;

  Result<void> cleanupOrphaned() override;
  Result<void> validate() override;

  // FilesystemAttachmentStore specific methods
  const Config& config() const { return config_; }
  
  // Set note store for orphan cleanup
  void setNoteStore(std::shared_ptr<class NoteStore> note_store);

 private:
  Config config_;
  std::shared_ptr<class NoteStore> note_store_;  // For orphan detection
  
  // Metadata cache (thread-safe)
  mutable std::mutex metadata_mutex_;
  mutable std::unordered_map<nx::core::NoteId, AttachmentInfo> metadata_cache_;
  mutable bool metadata_loaded_ = false;
  
  // Internal operations
  Result<void> ensureDirectories();
  Result<void> loadMetadata() const;
  Result<void> saveMetadata() const;
  
  Result<std::string> detectMimeType(const std::filesystem::path& file_path) const;
  Result<void> validateFile(const std::filesystem::path& file_path) const;
  
  std::filesystem::path getAttachmentPath(const nx::core::NoteId& attachment_id) const;
  
  // Cache operations
  void addToCache(const AttachmentInfo& info) const;
  void removeFromCache(const nx::core::NoteId& attachment_id) const;
  std::optional<AttachmentInfo> getFromCache(const nx::core::NoteId& attachment_id) const;
};

}  // namespace nx::store