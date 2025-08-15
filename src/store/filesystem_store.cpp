#include "nx/store/filesystem_store.hpp"

#include <algorithm>
#include <regex>
#include <set>

#include "nx/util/filesystem.hpp"

namespace nx::store {

FilesystemStore::FilesystemStore() : FilesystemStore(Config{}) {
}

FilesystemStore::FilesystemStore(Config config) : config_(std::move(config)) {
  // Set default paths if not provided
  if (config_.notes_dir.empty()) {
    config_.notes_dir = nx::util::Xdg::notesDir();
  }
  if (config_.attachments_dir.empty()) {
    config_.attachments_dir = nx::util::Xdg::attachmentsDir();
  }
  if (config_.trash_dir.empty()) {
    config_.trash_dir = nx::util::Xdg::trashDir();
  }
  
  if (config_.auto_create_dirs) {
    ensureDirectories();
  }
}

Result<void> FilesystemStore::store(const nx::core::Note& note) {
  // Validate note
  auto validation_result = note.validate();
  if (!validation_result.has_value()) {
    return validation_result;
  }
  
  // Get file path
  auto note_path = getNotePath(note.id());
  
  // Validate path if required
  if (config_.validate_paths) {
    auto path_validation = nx::util::FileSystem::validatePath(note_path);
    if (!path_validation.has_value()) {
      return path_validation;
    }
  }
  
  // Serialize note to file format
  std::string file_content = note.toFileFormat();
  
  // Atomic write
  auto write_result = nx::util::FileSystem::writeFileAtomic(note_path, file_content);
  if (!write_result.has_value()) {
    return write_result;
  }
  
  // Update cache
  updateMetadataCache(note);
  
  // Notify change
  notifyChange(note.id(), "store");
  
  return {};
}

Result<nx::core::Note> FilesystemStore::load(const nx::core::NoteId& id) {
  // Find note file
  auto file_path_result = findNoteFile(id);
  if (!file_path_result.has_value()) {
    return std::unexpected(file_path_result.error());
  }
  
  // Read file
  auto content_result = nx::util::FileSystem::readFile(*file_path_result);
  if (!content_result.has_value()) {
    return std::unexpected(content_result.error());
  }
  
  // Parse note
  auto note_result = nx::core::Note::fromFileFormat(*content_result);
  if (!note_result.has_value()) {
    return std::unexpected(note_result.error());
  }
  
  // Update cache
  updateMetadataCache(*note_result);
  
  return *note_result;
}

Result<void> FilesystemStore::remove(const nx::core::NoteId& id, bool soft_delete) {
  if (soft_delete) {
    return moveToTrash(id);
  } else {
    return permanentlyDelete(id);
  }
}

Result<bool> FilesystemStore::exists(const nx::core::NoteId& id) {
  auto file_path_result = findNoteFile(id);
  return file_path_result.has_value();
}

Result<void> FilesystemStore::storeBatch(const std::vector<nx::core::Note>& notes) {
  // Store each note individually (could be optimized with transactions)
  for (const auto& note : notes) {
    auto result = store(note);
    if (!result.has_value()) {
      return result;
    }
  }
  return {};
}

Result<std::vector<nx::core::Note>> FilesystemStore::loadBatch(const std::vector<nx::core::NoteId>& ids) {
  std::vector<nx::core::Note> notes;
  notes.reserve(ids.size());
  
  for (const auto& id : ids) {
    auto note_result = load(id);
    if (note_result.has_value()) {
      notes.push_back(*note_result);
    }
    // Continue loading other notes even if one fails
  }
  
  return notes;
}

Result<std::vector<nx::core::NoteId>> FilesystemStore::list(const NoteQuery& query) {
  auto files_result = getAllNoteFiles();
  if (!files_result.has_value()) {
    return std::unexpected(files_result.error());
  }
  
  std::vector<nx::core::NoteId> ids;
  
  for (const auto& file_path : *files_result) {
    // Extract ID from filename
    std::string filename = file_path.filename().string();
    if (filename.length() >= 26) {
      auto id_result = nx::core::NoteId::fromString(filename.substr(0, 26));
      if (id_result.has_value()) {
        ids.push_back(*id_result);
      }
    }
  }
  
  // Apply query filters by loading minimal metadata
  if (query.notebook.has_value() || !query.tags.empty() || 
      query.since.has_value() || query.until.has_value() ||
      query.title_contains.has_value()) {
    
    std::vector<nx::core::NoteId> filtered_ids;
    
    for (const auto& id : ids) {
      // Use cached metadata if available, otherwise load minimal metadata
      auto cached_metadata = getCachedMetadata(id);
      if (cached_metadata.has_value()) {
        nx::core::Note temp_note(*cached_metadata, "");
        if (matchesQuery(temp_note, query)) {
          filtered_ids.push_back(id);
        }
      } else {
        // Load full note for filtering (could be optimized)
        auto note_result = load(id);
        if (note_result.has_value() && matchesQuery(*note_result, query)) {
          filtered_ids.push_back(id);
        }
      }
    }
    
    ids = std::move(filtered_ids);
  }
  
  // Apply sorting
  if (query.sort_by != NoteQuery::SortBy::kCreated || 
      query.sort_order != NoteQuery::SortOrder::kDescending) {
    // Sort by ULID (which includes timestamp as prefix)
    std::sort(ids.begin(), ids.end());
    if (query.sort_order == NoteQuery::SortOrder::kDescending) {
      std::reverse(ids.begin(), ids.end());
    }
  }
  
  // Apply offset and limit
  if (query.offset > 0 || query.limit > 0) {
    size_t start = std::min(query.offset, ids.size());
    size_t end = ids.size();
    
    if (query.limit > 0) {
      end = std::min(start + query.limit, ids.size());
    }
    
    if (start < end) {
      ids = std::vector<nx::core::NoteId>(ids.begin() + start, ids.begin() + end);
    } else {
      ids.clear();
    }
  }
  
  return ids;
}

Result<std::vector<nx::core::Note>> FilesystemStore::search(const NoteQuery& query) {
  auto ids_result = list(query);
  if (!ids_result.has_value()) {
    return std::unexpected(ids_result.error());
  }
  
  return loadBatch(*ids_result);
}

Result<size_t> FilesystemStore::count(const NoteQuery& query) {
  auto ids_result = list(query);
  if (!ids_result.has_value()) {
    return std::unexpected(ids_result.error());
  }
  
  return ids_result->size();
}

Result<std::vector<FuzzyMatch>> FilesystemStore::fuzzyResolve(const std::string& partial_id, 
                                                              size_t max_results) {
  auto all_ids_result = list();
  if (!all_ids_result.has_value()) {
    return std::unexpected(all_ids_result.error());
  }
  
  return performFuzzyMatch(partial_id, *all_ids_result, max_results);
}

Result<nx::core::NoteId> FilesystemStore::resolveSingle(const std::string& partial_id) {
  auto matches_result = fuzzyResolve(partial_id, 1);
  if (!matches_result.has_value()) {
    return std::unexpected(matches_result.error());
  }
  
  if (matches_result->empty()) {
    return std::unexpected(makeError(ErrorCode::kFileNotFound, 
                                     "No notes match: " + partial_id));
  }
  
  return matches_result->front().id;
}

Result<std::vector<std::string>> FilesystemStore::getAllTags() {
  refreshMetadataCache();
  
  std::set<std::string> unique_tags;
  
  std::lock_guard<std::mutex> lock(cache_mutex_);
  for (const auto& [id, metadata] : metadata_cache_) {
    for (const auto& tag : metadata.tags()) {
      unique_tags.insert(tag);
    }
  }
  
  return std::vector<std::string>(unique_tags.begin(), unique_tags.end());
}

Result<std::vector<std::string>> FilesystemStore::getAllNotebooks() {
  refreshMetadataCache();
  
  std::set<std::string> unique_notebooks;
  
  std::lock_guard<std::mutex> lock(cache_mutex_);
  for (const auto& [id, metadata] : metadata_cache_) {
    if (metadata.notebook().has_value()) {
      unique_notebooks.insert(*metadata.notebook());
    }
  }
  
  return std::vector<std::string>(unique_notebooks.begin(), unique_notebooks.end());
}

Result<std::vector<nx::core::NoteId>> FilesystemStore::getBacklinks(const nx::core::NoteId& target_id) {
  refreshMetadataCache();
  
  std::vector<nx::core::NoteId> backlinks;
  
  std::lock_guard<std::mutex> lock(cache_mutex_);
  for (const auto& [id, metadata] : metadata_cache_) {
    if (metadata.hasLink(target_id)) {
      backlinks.push_back(id);
    }
  }
  
  return backlinks;
}

Result<std::vector<nx::core::NoteId>> FilesystemStore::listTrashed() {
  auto files_result = getAllTrashFiles();
  if (!files_result.has_value()) {
    return std::unexpected(files_result.error());
  }
  
  std::vector<nx::core::NoteId> ids;
  
  for (const auto& file_path : *files_result) {
    std::string filename = file_path.filename().string();
    if (filename.length() >= 26) {
      auto id_result = nx::core::NoteId::fromString(filename.substr(0, 26));
      if (id_result.has_value()) {
        ids.push_back(*id_result);
      }
    }
  }
  
  return ids;
}

Result<void> FilesystemStore::restore(const nx::core::NoteId& id) {
  return restoreFromTrash(id);
}

Result<void> FilesystemStore::permanentlyDelete(const nx::core::NoteId& id) {
  // Try to delete from main directory first
  auto note_path = getNotePath(id);
  if (std::filesystem::exists(note_path)) {
    auto result = nx::util::FileSystem::removeFile(note_path);
    if (!result.has_value()) {
      return result;
    }
  }
  
  // Try to delete from trash
  auto trash_path = getTrashPath(id);
  if (std::filesystem::exists(trash_path)) {
    auto result = nx::util::FileSystem::removeFile(trash_path);
    if (!result.has_value()) {
      return result;
    }
  }
  
  // Remove from cache
  invalidateCache(id);
  
  // Notify change
  notifyChange(id, "delete");
  
  return {};
}

Result<void> FilesystemStore::emptyTrash() {
  auto trashed_ids_result = listTrashed();
  if (!trashed_ids_result.has_value()) {
    return std::unexpected(trashed_ids_result.error());
  }
  
  for (const auto& id : *trashed_ids_result) {
    auto result = permanentlyDelete(id);
    if (!result.has_value()) {
      return result;
    }
  }
  
  return {};
}

Result<size_t> FilesystemStore::totalNotes() {
  auto count_result = count();
  return count_result;
}

Result<size_t> FilesystemStore::totalSize() {
  auto files_result = getAllNoteFiles();
  if (!files_result.has_value()) {
    return std::unexpected(files_result.error());
  }
  
  size_t total = 0;
  for (const auto& file_path : *files_result) {
    auto size_result = nx::util::FileSystem::fileSize(file_path);
    if (size_result.has_value()) {
      total += *size_result;
    }
  }
  
  return total;
}

Result<std::chrono::system_clock::time_point> FilesystemStore::lastModified() {
  auto files_result = getAllNoteFiles();
  if (!files_result.has_value()) {
    return std::unexpected(files_result.error());
  }
  
  std::chrono::system_clock::time_point latest{};
  
  for (const auto& file_path : *files_result) {
    auto time_result = nx::util::FileSystem::lastModified(file_path);
    if (time_result.has_value()) {
      auto file_time = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
          std::chrono::system_clock::now() + 
          std::chrono::duration_cast<std::chrono::system_clock::duration>(
              time_result->time_since_epoch()));
      if (file_time > latest) {
        latest = file_time;
      }
    }
  }
  
  return latest;
}

Result<void> FilesystemStore::rebuild() {
  clearCache();
  refreshMetadataCache();
  return {};
}

Result<void> FilesystemStore::vacuum() {
  // For filesystem store, vacuum means cleaning up orphaned files
  // and ensuring directory structure is correct
  return ensureDirectories();
}

Result<void> FilesystemStore::validate() {
  auto files_result = getAllNoteFiles();
  if (!files_result.has_value()) {
    return std::unexpected(files_result.error());
  }
  
  for (const auto& file_path : *files_result) {
    auto validation_result = validateNoteFile(file_path);
    if (!validation_result.has_value()) {
      return validation_result;
    }
  }
  
  return {};
}

void FilesystemStore::setChangeCallback(ChangeCallback callback) {
  change_callback_ = std::move(callback);
}

std::filesystem::path FilesystemStore::getNotePath(const nx::core::NoteId& id) const {
  // Use ULID + .md extension as filename
  return config_.notes_dir / (id.toString() + ".md");
}

std::filesystem::path FilesystemStore::getTrashPath(const nx::core::NoteId& id) const {
  return config_.trash_dir / (id.toString() + ".md");
}

Result<void> FilesystemStore::ensureDirectories() {
  auto ensure_notes = nx::util::FileSystem::ensureXdgDirectory(config_.notes_dir);
  if (!ensure_notes.has_value()) {
    return ensure_notes;
  }
  
  auto ensure_attachments = nx::util::FileSystem::ensureXdgDirectory(config_.attachments_dir);
  if (!ensure_attachments.has_value()) {
    return ensure_attachments;
  }
  
  auto ensure_trash = nx::util::FileSystem::ensureXdgDirectory(config_.trash_dir);
  if (!ensure_trash.has_value()) {
    return ensure_trash;
  }
  
  return {};
}

void FilesystemStore::clearCache() {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  metadata_cache_.clear();
  cache_refresh_time_.reset();
}

void FilesystemStore::invalidateCache(const nx::core::NoteId& id) {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  metadata_cache_.erase(id);
}

// [Implementation continues with remaining private methods...]

Result<std::filesystem::path> FilesystemStore::findNoteFile(const nx::core::NoteId& id) const {
  auto note_path = getNotePath(id);
  if (std::filesystem::exists(note_path)) {
    return note_path;
  }
  
  return std::unexpected(makeError(ErrorCode::kFileNotFound, 
                                   "Note not found: " + id.toString()));
}

Result<std::vector<std::filesystem::path>> FilesystemStore::getAllNoteFiles() const {
  return nx::util::FileSystem::listDirectory(config_.notes_dir, ".md");
}

Result<std::vector<std::filesystem::path>> FilesystemStore::getAllTrashFiles() const {
  return nx::util::FileSystem::listDirectory(config_.trash_dir, ".md");
}

std::vector<FuzzyMatch> FilesystemStore::performFuzzyMatch(const std::string& partial_id, 
                                                           const std::vector<nx::core::NoteId>& candidates,
                                                           size_t max_results) const {
  std::vector<FuzzyMatch> matches;
  
  for (const auto& id : candidates) {
    // Get metadata for title
    auto cached_metadata = getCachedMetadata(id);
    std::string title = cached_metadata.has_value() ? cached_metadata->title() : "";
    
    double score = calculateMatchScore(partial_id, id, title);
    if (score > 0.0) {
      matches.push_back({id, title, score});
    }
  }
  
  // Sort by score (descending)
  std::sort(matches.begin(), matches.end(), 
            [](const FuzzyMatch& a, const FuzzyMatch& b) {
              return a.score > b.score;
            });
  
  // Limit results
  if (matches.size() > max_results) {
    matches.resize(max_results);
  }
  
  return matches;
}

double FilesystemStore::calculateMatchScore(const std::string& partial_id, 
                                           const nx::core::NoteId& id,
                                           const std::string& title) const {
  std::string id_str = id.toString();
  
  // Exact prefix match gets highest score
  if (id_str.substr(0, partial_id.length()) == partial_id) {
    return 1.0;
  }
  
  // Case-insensitive title match
  std::string lower_partial = partial_id;
  std::string lower_title = title;
  std::transform(lower_partial.begin(), lower_partial.end(), lower_partial.begin(), ::tolower);
  std::transform(lower_title.begin(), lower_title.end(), lower_title.begin(), ::tolower);
  
  if (lower_title.find(lower_partial) != std::string::npos) {
    return 0.8;
  }
  
  // Partial ID match anywhere in the ID
  if (id_str.find(partial_id) != std::string::npos) {
    return 0.5;
  }
  
  return 0.0;
}

bool FilesystemStore::matchesQuery(const nx::core::Note& note, const NoteQuery& query) const {
  // Check notebook filter
  if (query.notebook.has_value()) {
    if (!note.notebook().has_value() || *note.notebook() != *query.notebook) {
      return false;
    }
  }
  
  // Check tag filters
  if (!query.tags.empty()) {
    for (const auto& required_tag : query.tags) {
      if (!note.metadata().hasTag(required_tag)) {
        return false;
      }
    }
  }
  
  // Check time filters
  if (query.since.has_value() && note.metadata().created() < *query.since) {
    return false;
  }
  
  if (query.until.has_value() && note.metadata().created() > *query.until) {
    return false;
  }
  
  // Check title filter
  if (query.title_contains.has_value()) {
    if (note.title().find(*query.title_contains) == std::string::npos) {
      return false;
    }
  }
  
  // Check content filter
  if (query.content_contains.has_value()) {
    if (note.content().find(*query.content_contains) == std::string::npos) {
      return false;
    }
  }
  
  return true;
}

void FilesystemStore::updateMetadataCache(const nx::core::Note& note) const {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  metadata_cache_[note.id()] = note.metadata();
}

std::optional<nx::core::Metadata> FilesystemStore::getCachedMetadata(const nx::core::NoteId& id) const {
  std::lock_guard<std::mutex> lock(cache_mutex_);
  auto it = metadata_cache_.find(id);
  return it != metadata_cache_.end() ? std::make_optional(it->second) : std::nullopt;
}

void FilesystemStore::refreshMetadataCache() const {
  // Only refresh if cache is stale (older than 5 minutes)
  std::lock_guard<std::mutex> lock(cache_mutex_);
  
  auto now = std::chrono::system_clock::now();
  if (cache_refresh_time_.has_value() && 
      (now - *cache_refresh_time_) < std::chrono::minutes(5)) {
    return;
  }
  
  // Refresh metadata cache by loading all note files
  auto files_result = getAllNoteFiles();
  if (files_result.has_value()) {
    metadata_cache_.clear();
    for (const auto& file_path : *files_result) {
      // Extract ID from filename and load metadata
      std::string filename = file_path.filename().string();
      if (filename.length() >= 26) {
        auto id_result = nx::core::NoteId::fromString(filename.substr(0, 26));
        if (id_result.has_value()) {
          // Load just the metadata efficiently by parsing the file header
          auto content_result = nx::util::FileSystem::readFile(file_path);
          if (content_result.has_value()) {
            auto note_result = nx::core::Note::fromFileFormat(*content_result);
            if (note_result.has_value()) {
              metadata_cache_[note_result->id()] = note_result->metadata();
            }
          }
        }
      }
    }
  }
  cache_refresh_time_ = now;
}

void FilesystemStore::notifyChange(const nx::core::NoteId& id, const std::string& operation) {
  if (change_callback_) {
    change_callback_(id, operation);
  }
}

Result<void> FilesystemStore::validateNoteFile(const std::filesystem::path& path) const {
  // Read and parse the file to ensure it's valid
  auto content_result = nx::util::FileSystem::readFile(path);
  if (!content_result.has_value()) {
    return std::unexpected(content_result.error());
  }
  
  auto note_result = nx::core::Note::fromFileFormat(*content_result);
  if (!note_result.has_value()) {
    return std::unexpected(note_result.error());
  }
  
  return note_result->validate();
}

Result<void> FilesystemStore::moveToTrash(const nx::core::NoteId& id) {
  auto note_path = getNotePath(id);
  auto trash_path = getTrashPath(id);
  
  if (!std::filesystem::exists(note_path)) {
    return std::unexpected(makeError(ErrorCode::kFileNotFound, 
                                     "Note not found: " + id.toString()));
  }
  
  auto result = nx::util::FileSystem::moveFile(note_path, trash_path);
  if (!result.has_value()) {
    return result;
  }
  
  // Keep in cache but mark as trashed somehow
  notifyChange(id, "trash");
  
  return {};
}

Result<void> FilesystemStore::restoreFromTrash(const nx::core::NoteId& id) {
  auto trash_path = getTrashPath(id);
  auto note_path = getNotePath(id);
  
  if (!std::filesystem::exists(trash_path)) {
    return std::unexpected(makeError(ErrorCode::kFileNotFound, 
                                     "Note not found in trash: " + id.toString()));
  }
  
  auto result = nx::util::FileSystem::moveFile(trash_path, note_path);
  if (!result.has_value()) {
    return result;
  }
  
  notifyChange(id, "restore");
  
  return {};
}

}  // namespace nx::store