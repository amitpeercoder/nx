#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "nx/common.hpp"
#include "nx/core/note_id.hpp"

namespace nx::store {

// Attachment metadata
struct AttachmentInfo {
  nx::core::NoteId id;          // ULID for the attachment
  nx::core::NoteId parent_note; // Note this is attached to
  std::string original_name;    // Original filename
  std::string mime_type;        // MIME type
  std::uintmax_t size;          // File size in bytes
  std::chrono::system_clock::time_point created;
  std::string description;      // Optional description
  
  // Generate filename for storage (ULID-original_name)
  std::string storageFilename() const;
  
  // Get relative path for use in notes
  std::string relativePath() const;
};

// Attachment storage interface
class AttachmentStore {
 public:
  virtual ~AttachmentStore() = default;

  // Store attachment file
  virtual Result<AttachmentInfo> store(const nx::core::NoteId& parent_note,
                                       const std::filesystem::path& source_file,
                                       const std::string& description = "") = 0;

  // Store attachment from data
  virtual Result<AttachmentInfo> storeData(const nx::core::NoteId& parent_note,
                                           const std::string& data,
                                           const std::string& filename,
                                           const std::string& mime_type = "",
                                           const std::string& description = "") = 0;

  // Load attachment data
  virtual Result<std::string> loadData(const nx::core::NoteId& attachment_id) = 0;

  // Get attachment info
  virtual Result<AttachmentInfo> getInfo(const nx::core::NoteId& attachment_id) = 0;

  // Remove attachment
  virtual Result<void> remove(const nx::core::NoteId& attachment_id) = 0;

  // List attachments for a note
  virtual Result<std::vector<AttachmentInfo>> listForNote(const nx::core::NoteId& note_id) = 0;

  // List all attachments
  virtual Result<std::vector<AttachmentInfo>> listAll() = 0;

  // Get attachment file path
  virtual Result<std::filesystem::path> getPath(const nx::core::NoteId& attachment_id) = 0;

  // Copy attachment to external location
  virtual Result<void> exportTo(const nx::core::NoteId& attachment_id,
                                const std::filesystem::path& target_path) = 0;

  // Statistics
  virtual Result<size_t> totalAttachments() = 0;
  virtual Result<std::uintmax_t> totalSize() = 0;

  // Maintenance
  virtual Result<void> cleanupOrphaned() = 0;  // Remove attachments with no parent note
  virtual Result<void> validate() = 0;
};

}  // namespace nx::store