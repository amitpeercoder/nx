#include "nx/store/filesystem_attachment_store.hpp"

#include <fstream>
#include <regex>

#include <nlohmann/json.hpp>

#include "nx/store/note_store.hpp"
#include "nx/util/filesystem.hpp"
#include "nx/util/time.hpp"
#include "nx/util/xdg.hpp"

namespace nx::store {

FilesystemAttachmentStore::FilesystemAttachmentStore() : FilesystemAttachmentStore(Config{}) {
}

FilesystemAttachmentStore::FilesystemAttachmentStore(Config config) : config_(std::move(config)) {
  // Set default paths if not provided
  if (config_.attachments_dir.empty()) {
    config_.attachments_dir = nx::util::Xdg::attachmentsDir();
  }
  if (config_.metadata_file.empty()) {
    config_.metadata_file = nx::util::Xdg::nxDir() / "attachments.json";
  }
  
  if (config_.auto_create_dirs) {
    ensureDirectories();
  }
}

Result<AttachmentInfo> FilesystemAttachmentStore::store(const nx::core::NoteId& parent_note,
                                                        const std::filesystem::path& source_file,
                                                        const std::string& description) {
  // Validate source file
  auto validation_result = validateFile(source_file);
  if (!validation_result.has_value()) {
    return std::unexpected(validation_result.error());
  }
  
  // Get file info
  auto size_result = nx::util::FileSystem::fileSize(source_file);
  if (!size_result.has_value()) {
    return std::unexpected(size_result.error());
  }
  
  // Create attachment info
  AttachmentInfo info;
  info.id = nx::core::NoteId::generate();
  info.parent_note = parent_note;
  info.original_name = source_file.filename().string();
  info.size = *size_result;
  info.created = std::chrono::system_clock::now();
  info.description = description;
  
  // Detect MIME type
  auto mime_result = detectMimeType(source_file);
  if (mime_result.has_value()) {
    info.mime_type = *mime_result;
  }
  
  // Copy file to attachments directory
  auto target_path = getAttachmentPath(info.id);
  auto copy_result = nx::util::FileSystem::copyFile(source_file, target_path);
  if (!copy_result.has_value()) {
    return std::unexpected(copy_result.error());
  }
  
  // Update metadata
  addToCache(info);
  auto save_result = saveMetadata();
  if (!save_result.has_value()) {
    // Clean up file on metadata save failure
    nx::util::FileSystem::removeFile(target_path);
    return std::unexpected(save_result.error());
  }
  
  return info;
}

Result<AttachmentInfo> FilesystemAttachmentStore::storeData(const nx::core::NoteId& parent_note,
                                                            const std::string& data,
                                                            const std::string& filename,
                                                            const std::string& mime_type,
                                                            const std::string& description) {
  // Check size limit
  if (data.size() > config_.max_file_size) {
    return std::unexpected(makeError(ErrorCode::kValidationError, 
                                     "File too large: " + std::to_string(data.size()) + " bytes"));
  }
  
  // Create attachment info
  AttachmentInfo info;
  info.id = nx::core::NoteId::generate();
  info.parent_note = parent_note;
  info.original_name = filename;
  info.mime_type = mime_type;
  info.size = data.size();
  info.created = std::chrono::system_clock::now();
  info.description = description;
  
  // Write data to file
  auto target_path = getAttachmentPath(info.id);
  auto write_result = nx::util::FileSystem::writeFileAtomic(target_path, data);
  if (!write_result.has_value()) {
    return std::unexpected(write_result.error());
  }
  
  // Update metadata
  addToCache(info);
  auto save_result = saveMetadata();
  if (!save_result.has_value()) {
    // Clean up file on metadata save failure
    nx::util::FileSystem::removeFile(target_path);
    return std::unexpected(save_result.error());
  }
  
  return info;
}

Result<std::string> FilesystemAttachmentStore::loadData(const nx::core::NoteId& attachment_id) {
  // Get attachment path
  auto path_result = getPath(attachment_id);
  if (!path_result.has_value()) {
    return std::unexpected(path_result.error());
  }
  
  // Read file
  return nx::util::FileSystem::readFile(*path_result);
}

Result<AttachmentInfo> FilesystemAttachmentStore::getInfo(const nx::core::NoteId& attachment_id) {
  auto load_result = loadMetadata();
  if (!load_result.has_value()) {
    return std::unexpected(load_result.error());
  }
  
  auto cached = getFromCache(attachment_id);
  if (cached.has_value()) {
    return *cached;
  }
  
  return std::unexpected(makeError(ErrorCode::kFileNotFound, 
                                   "Attachment not found: " + attachment_id.toString()));
}

Result<void> FilesystemAttachmentStore::remove(const nx::core::NoteId& attachment_id) {
  // Get file path
  auto path_result = getPath(attachment_id);
  if (!path_result.has_value()) {
    return std::unexpected(path_result.error());
  }
  
  // Remove file
  auto remove_result = nx::util::FileSystem::removeFile(*path_result);
  if (!remove_result.has_value()) {
    return std::unexpected(remove_result.error());
  }
  
  // Update metadata
  removeFromCache(attachment_id);
  return saveMetadata();
}

Result<std::vector<AttachmentInfo>> FilesystemAttachmentStore::listForNote(const nx::core::NoteId& note_id) {
  auto load_result = loadMetadata();
  if (!load_result.has_value()) {
    return std::unexpected(load_result.error());
  }
  
  std::vector<AttachmentInfo> attachments;
  
  std::lock_guard<std::mutex> lock(metadata_mutex_);
  for (const auto& [id, info] : metadata_cache_) {
    if (info.parent_note == note_id) {
      attachments.push_back(info);
    }
  }
  
  // Sort by creation time
  std::sort(attachments.begin(), attachments.end(),
            [](const AttachmentInfo& a, const AttachmentInfo& b) {
              return a.created < b.created;
            });
  
  return attachments;
}

Result<std::vector<AttachmentInfo>> FilesystemAttachmentStore::listAll() {
  auto load_result = loadMetadata();
  if (!load_result.has_value()) {
    return std::unexpected(load_result.error());
  }
  
  std::vector<AttachmentInfo> attachments;
  
  std::lock_guard<std::mutex> lock(metadata_mutex_);
  for (const auto& [id, info] : metadata_cache_) {
    attachments.push_back(info);
  }
  
  return attachments;
}

Result<std::filesystem::path> FilesystemAttachmentStore::getPath(const nx::core::NoteId& attachment_id) {
  auto info_result = getInfo(attachment_id);
  if (!info_result.has_value()) {
    return std::unexpected(info_result.error());
  }
  
  auto path = getAttachmentPath(attachment_id);
  if (!std::filesystem::exists(path)) {
    return std::unexpected(makeError(ErrorCode::kFileNotFound, 
                                     "Attachment file not found: " + path.string()));
  }
  
  return path;
}

Result<void> FilesystemAttachmentStore::exportTo(const nx::core::NoteId& attachment_id,
                                                 const std::filesystem::path& target_path) {
  auto source_result = getPath(attachment_id);
  if (!source_result.has_value()) {
    return std::unexpected(source_result.error());
  }
  
  return nx::util::FileSystem::copyFile(*source_result, target_path);
}

Result<size_t> FilesystemAttachmentStore::totalAttachments() {
  auto all_result = listAll();
  if (!all_result.has_value()) {
    return std::unexpected(all_result.error());
  }
  
  return all_result->size();
}

Result<std::uintmax_t> FilesystemAttachmentStore::totalSize() {
  auto all_result = listAll();
  if (!all_result.has_value()) {
    return std::unexpected(all_result.error());
  }
  
  std::uintmax_t total = 0;
  for (const auto& info : *all_result) {
    total += info.size;
  }
  
  return total;
}

Result<void> FilesystemAttachmentStore::cleanupOrphaned() {
  if (!note_store_) {
    return std::unexpected(makeError(ErrorCode::kConfigError, 
                                     "Note store not set for orphan cleanup"));
  }
  
  auto all_result = listAll();
  if (!all_result.has_value()) {
    return std::unexpected(all_result.error());
  }
  
  std::vector<nx::core::NoteId> to_remove;
  
  for (const auto& info : *all_result) {
    auto exists_result = note_store_->exists(info.parent_note);
    if (exists_result.has_value() && !*exists_result) {
      to_remove.push_back(info.id);
    }
  }
  
  for (const auto& id : to_remove) {
    remove(id);  // Ignore errors for individual removals
  }
  
  return {};
}

Result<void> FilesystemAttachmentStore::validate() {
  auto load_result = loadMetadata();
  if (!load_result.has_value()) {
    return std::unexpected(load_result.error());
  }
  
  std::lock_guard<std::mutex> lock(metadata_mutex_);
  for (const auto& [id, info] : metadata_cache_) {
    auto path = getAttachmentPath(id);
    if (!std::filesystem::exists(path)) {
      return std::unexpected(makeError(ErrorCode::kValidationError, 
                                       "Attachment file missing: " + id.toString()));
    }
    
    // Verify file size matches metadata
    auto size_result = nx::util::FileSystem::fileSize(path);
    if (size_result.has_value() && *size_result != info.size) {
      return std::unexpected(makeError(ErrorCode::kValidationError, 
                                       "Size mismatch for attachment: " + id.toString()));
    }
  }
  
  return {};
}

void FilesystemAttachmentStore::setNoteStore(std::shared_ptr<NoteStore> note_store) {
  note_store_ = std::move(note_store);
}

// Private methods

Result<void> FilesystemAttachmentStore::ensureDirectories() {
  auto ensure_attachments = nx::util::FileSystem::ensureXdgDirectory(config_.attachments_dir);
  if (!ensure_attachments.has_value()) {
    return ensure_attachments;
  }
  
  // Ensure metadata file directory exists
  auto metadata_dir = config_.metadata_file.parent_path();
  return nx::util::FileSystem::ensureXdgDirectory(metadata_dir);
}

Result<void> FilesystemAttachmentStore::loadMetadata() const {
  std::lock_guard<std::mutex> lock(metadata_mutex_);
  
  if (metadata_loaded_) {
    return {};
  }
  
  if (!std::filesystem::exists(config_.metadata_file)) {
    metadata_loaded_ = true;
    return {};  // No metadata file yet, empty cache is valid
  }
  
  auto content_result = nx::util::FileSystem::readFile(config_.metadata_file);
  if (!content_result.has_value()) {
    return std::unexpected(content_result.error());
  }
  
  try {
    auto json = nlohmann::json::parse(*content_result);
    
    metadata_cache_.clear();
    
    for (const auto& item : json["attachments"]) {
      AttachmentInfo info;
      
      auto id_result = nx::core::NoteId::fromString(item["id"].get<std::string>());
      if (!id_result.has_value()) continue;
      info.id = *id_result;
      
      auto parent_result = nx::core::NoteId::fromString(item["parent_note"].get<std::string>());
      if (!parent_result.has_value()) continue;
      info.parent_note = *parent_result;
      
      info.original_name = item["original_name"].get<std::string>();
      info.mime_type = item.value("mime_type", "");
      info.size = item["size"].get<size_t>();
      info.description = item.value("description", "");
      
      auto created_result = nx::util::Time::fromRfc3339(item["created"].get<std::string>());
      if (created_result.has_value()) {
        info.created = *created_result;
      }
      
      metadata_cache_[info.id] = info;
    }
    
    metadata_loaded_ = true;
    return {};
    
  } catch (const nlohmann::json::exception& e) {
    return std::unexpected(makeError(ErrorCode::kParseError, 
                                     "Invalid attachment metadata: " + std::string(e.what())));
  }
}

Result<void> FilesystemAttachmentStore::saveMetadata() const {
  std::lock_guard<std::mutex> lock(metadata_mutex_);
  
  nlohmann::json json;
  json["version"] = 1;
  json["attachments"] = nlohmann::json::array();
  
  for (const auto& [id, info] : metadata_cache_) {
    nlohmann::json item;
    item["id"] = info.id.toString();
    item["parent_note"] = info.parent_note.toString();
    item["original_name"] = info.original_name;
    item["mime_type"] = info.mime_type;
    item["size"] = info.size;
    item["created"] = nx::util::Time::toRfc3339(info.created);
    item["description"] = info.description;
    
    json["attachments"].push_back(item);
  }
  
  std::string content = json.dump(2);
  return nx::util::FileSystem::writeFileAtomic(config_.metadata_file, content);
}

Result<std::string> FilesystemAttachmentStore::detectMimeType(const std::filesystem::path& file_path) const {
  std::string extension = file_path.extension().string();
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
  
  // Simple MIME type detection based on extension
  static const std::unordered_map<std::string, std::string> mime_types = {
    {".jpg", "image/jpeg"}, {".jpeg", "image/jpeg"},
    {".png", "image/png"}, {".gif", "image/gif"}, {".svg", "image/svg+xml"},
    {".webp", "image/webp"},
    {".pdf", "application/pdf"},
    {".txt", "text/plain"}, {".md", "text/markdown"},
    {".json", "application/json"}, {".xml", "application/xml"},
    {".csv", "text/csv"},
    {".mp3", "audio/mpeg"}, {".wav", "audio/wav"}, {".ogg", "audio/ogg"},
    {".mp4", "video/mp4"}, {".webm", "video/webm"},
    {".zip", "application/zip"}, {".tar", "application/x-tar"},
    {".gz", "application/gzip"}
  };
  
  auto it = mime_types.find(extension);
  if (it != mime_types.end()) {
    return it->second;
  }
  
  return "application/octet-stream";
}

Result<void> FilesystemAttachmentStore::validateFile(const std::filesystem::path& file_path) const {
  // Check if file exists
  if (!std::filesystem::exists(file_path)) {
    return std::unexpected(makeError(ErrorCode::kFileNotFound, 
                                     "Source file not found: " + file_path.string()));
  }
  
  // Check file size
  auto size_result = nx::util::FileSystem::fileSize(file_path);
  if (!size_result.has_value()) {
    return std::unexpected(size_result.error());
  }
  
  if (*size_result > config_.max_file_size) {
    return std::unexpected(makeError(ErrorCode::kValidationError, 
                                     "File too large: " + std::to_string(*size_result) + " bytes"));
  }
  
  // Check extension if restrictions are in place
  if (!config_.allowed_extensions.empty()) {
    std::string extension = file_path.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    
    auto it = std::find(config_.allowed_extensions.begin(), 
                        config_.allowed_extensions.end(), extension);
    if (it == config_.allowed_extensions.end()) {
      return std::unexpected(makeError(ErrorCode::kValidationError, 
                                       "File type not allowed: " + extension));
    }
  }
  
  return {};
}

std::filesystem::path FilesystemAttachmentStore::getAttachmentPath(const nx::core::NoteId& attachment_id) const {
  // Use attachment ULID as filename
  return config_.attachments_dir / (attachment_id.toString());
}

void FilesystemAttachmentStore::addToCache(const AttachmentInfo& info) const {
  std::lock_guard<std::mutex> lock(metadata_mutex_);
  metadata_cache_[info.id] = info;
}

void FilesystemAttachmentStore::removeFromCache(const nx::core::NoteId& attachment_id) const {
  std::lock_guard<std::mutex> lock(metadata_mutex_);
  metadata_cache_.erase(attachment_id);
}

std::optional<AttachmentInfo> FilesystemAttachmentStore::getFromCache(const nx::core::NoteId& attachment_id) const {
  std::lock_guard<std::mutex> lock(metadata_mutex_);
  auto it = metadata_cache_.find(attachment_id);
  return it != metadata_cache_.end() ? std::make_optional(it->second) : std::nullopt;
}

}  // namespace nx::store