#include "nx/store/notebook_manager.hpp"

#include <algorithm>
#include <regex>
#include <set>

#include "nx/store/note_store.hpp"
#include "nx/core/note.hpp"
#include "nx/common.hpp"

namespace nx::store {

// NotebookInfo implementation
NotebookInfo::NotebookInfo(const std::string& notebook_name) 
  : name(notebook_name) {
  // Initialize timestamps to current time
  auto now = std::chrono::system_clock::now();
  created = now;
  last_modified = now;
}

// NotebookManager implementation
NotebookManager::NotebookManager(NoteStore& note_store) 
  : note_store_(note_store) {
}

Result<void> NotebookManager::createNotebook(const std::string& name) {
  // Validate notebook name
  auto validation_result = validateNotebookName(name);
  if (!validation_result.has_value()) {
    return std::unexpected(validation_result.error());
  }
  
  // Check if notebook already exists
  auto exists_result = notebookExists(name);
  if (!exists_result.has_value()) {
    return std::unexpected(exists_result.error());
  }
  
  if (exists_result.value()) {
    return std::unexpected(makeError(ErrorCode::kValidationError, 
      "Notebook '" + name + "' already exists"));
  }
  
  // Create a placeholder note to ensure the notebook exists in the system
  // This note will be automatically created with the notebook metadata
  auto now = std::chrono::system_clock::now();
  auto time_t = std::chrono::system_clock::to_time_t(now);
  
  auto placeholder_note = nx::core::Note::create(
    ".notebook_" + name,
    "# " + name + "\n\nNotebook created on " + std::ctime(&time_t)
  );
  placeholder_note.setNotebook(name);
  
  // Store the placeholder note
  auto store_result = note_store_.store(placeholder_note);
  if (!store_result.has_value()) {
    return std::unexpected(store_result.error());
  }
  
  return {};
}

Result<void> NotebookManager::deleteNotebook(const std::string& name, bool force) {
  // Check if notebook exists
  auto exists_result = notebookExists(name);
  if (!exists_result.has_value()) {
    return std::unexpected(exists_result.error());
  }
  
  if (!exists_result.value()) {
    return std::unexpected(makeError(ErrorCode::kNotFound, 
      "Notebook '" + name + "' not found"));
  }
  
  // Get all notes in the notebook
  auto notes_result = getNotesInNotebook(name);
  if (!notes_result.has_value()) {
    return std::unexpected(notes_result.error());
  }
  
  auto note_ids = notes_result.value();
  
  // Filter out placeholder notes (notes starting with .notebook_)
  std::vector<nx::core::NoteId> user_notes;
  for (const auto& id : note_ids) {
    auto note_result = note_store_.load(id);
    if (note_result.has_value()) {
      const auto& note = note_result.value();
      if (!note.title().starts_with(".notebook_")) {
        user_notes.push_back(id);
      }
    }
  }
  
  // Safety check: prevent deletion of non-empty notebooks without --force
  if (!user_notes.empty() && !force) {
    return std::unexpected(makeError(ErrorCode::kValidationError, 
      "Notebook '" + name + "' contains " + std::to_string(user_notes.size()) + " notes. " +
      "Use --force to delete anyway, or move notes to another notebook first"));
  }
  
  // Delete all notes in the notebook
  for (const auto& id : note_ids) {
    auto delete_result = note_store_.remove(id, false);  // Hard delete
    if (!delete_result.has_value()) {
      // Note: Failed to delete note during notebook cleanup
      // We continue to avoid partial cleanup state, but this could be improved with proper logging
      continue;
    }
  }
  
  return {};
}

Result<void> NotebookManager::renameNotebook(const std::string& old_name, const std::string& new_name) {
  // Validate new notebook name
  auto validation_result = validateNotebookName(new_name);
  if (!validation_result.has_value()) {
    return std::unexpected(validation_result.error());
  }
  
  // Check if old notebook exists
  auto old_exists_result = notebookExists(old_name);
  if (!old_exists_result.has_value()) {
    return std::unexpected(old_exists_result.error());
  }
  
  if (!old_exists_result.value()) {
    return std::unexpected(makeError(ErrorCode::kNotFound, 
      "Notebook '" + old_name + "' not found"));
  }
  
  // Check if new notebook name already exists
  auto new_exists_result = notebookExists(new_name);
  if (!new_exists_result.has_value()) {
    return std::unexpected(new_exists_result.error());
  }
  
  if (new_exists_result.value()) {
    return std::unexpected(makeError(ErrorCode::kValidationError, 
      "Notebook '" + new_name + "' already exists"));
  }
  
  // Get all notes in the old notebook
  auto notes_result = getNotesInNotebook(old_name);
  if (!notes_result.has_value()) {
    return std::unexpected(notes_result.error());
  }
  
  // Update notebook field for all notes
  for (const auto& id : notes_result.value()) {
    auto note_result = note_store_.load(id);
    if (!note_result.has_value()) {
      continue;  // Skip notes that can't be loaded
    }
    
    auto note = note_result.value();
    note.setNotebook(new_name);
    note.touch();  // Update modified timestamp
    
    // Update placeholder note title if it exists
    if (note.title().starts_with(".notebook_")) {
      note.setTitle(".notebook_" + new_name);
    }
    
    auto store_result = note_store_.store(note);
    if (!store_result.has_value()) {
      return std::unexpected(store_result.error());
    }
  }
  
  return {};
}

Result<std::vector<NotebookInfo>> NotebookManager::listNotebooks(bool include_stats) {
  // Get all notebooks from the note store
  auto notebooks_result = note_store_.getAllNotebooks();
  if (!notebooks_result.has_value()) {
    return std::unexpected(notebooks_result.error());
  }
  
  std::vector<NotebookInfo> notebook_infos;
  
  for (const auto& name : notebooks_result.value()) {
    if (include_stats) {
      auto info_result = calculateNotebookStats(name);
      if (info_result.has_value()) {
        notebook_infos.push_back(info_result.value());
      }
    } else {
      // Basic info only
      NotebookInfo info(name);
      
      // Get basic note count
      NoteQuery query;
      query.notebook = name;
      auto count_result = note_store_.count(query);
      if (count_result.has_value()) {
        info.note_count = count_result.value();
      }
      
      notebook_infos.push_back(info);
    }
  }
  
  // Sort by name for consistent ordering
  std::sort(notebook_infos.begin(), notebook_infos.end(), 
    [](const NotebookInfo& a, const NotebookInfo& b) {
      return a.name < b.name;
    });
  
  return notebook_infos;
}

Result<NotebookInfo> NotebookManager::getNotebookInfo(const std::string& name, bool include_stats) {
  // Check if notebook exists
  auto exists_result = notebookExists(name);
  if (!exists_result.has_value()) {
    return std::unexpected(exists_result.error());
  }
  
  if (!exists_result.value()) {
    return std::unexpected(makeError(ErrorCode::kNotFound, 
      "Notebook '" + name + "' not found"));
  }
  
  if (include_stats) {
    return calculateNotebookStats(name);
  } else {
    NotebookInfo info(name);
    
    // Get basic note count
    NoteQuery query;
    query.notebook = name;
    auto count_result = note_store_.count(query);
    if (count_result.has_value()) {
      info.note_count = count_result.value();
    }
    
    return info;
  }
}

Result<bool> NotebookManager::notebookExists(const std::string& name) {
  auto notebooks_result = note_store_.getAllNotebooks();
  if (!notebooks_result.has_value()) {
    return std::unexpected(notebooks_result.error());
  }
  
  const auto& notebooks = notebooks_result.value();
  return std::find(notebooks.begin(), notebooks.end(), name) != notebooks.end();
}

Result<NotebookStats> NotebookManager::getOverallStats() {
  NotebookStats stats;
  
  auto notebooks_result = listNotebooks(true);
  if (!notebooks_result.has_value()) {
    return std::unexpected(notebooks_result.error());
  }
  
  const auto& notebooks = notebooks_result.value();
  stats.total_notebooks = notebooks.size();
  
  size_t max_notes = 0;
  auto latest_activity = std::chrono::system_clock::time_point::min();
  
  for (const auto& notebook : notebooks) {
    stats.total_notes += notebook.note_count;
    
    if (notebook.note_count > max_notes) {
      max_notes = notebook.note_count;
      stats.largest_notebook = notebook.name;
    }
    
    if (notebook.last_modified > latest_activity) {
      latest_activity = notebook.last_modified;
      stats.most_active_notebook = notebook.name;
    }
  }
  
  stats.last_activity = latest_activity;
  
  return stats;
}

Result<void> NotebookManager::moveAllNotes(const std::string& from_notebook, 
                                          const std::string& to_notebook) {
  // Validate both notebooks exist
  auto from_exists = notebookExists(from_notebook);
  if (!from_exists.has_value()) {
    return std::unexpected(from_exists.error());
  }
  
  if (!from_exists.value()) {
    return std::unexpected(makeError(ErrorCode::kNotFound, 
      "Source notebook '" + from_notebook + "' not found"));
  }
  
  auto to_exists = notebookExists(to_notebook);
  if (!to_exists.has_value()) {
    return std::unexpected(to_exists.error());
  }
  
  if (!to_exists.value()) {
    // Create destination notebook
    auto create_result = createNotebook(to_notebook);
    if (!create_result.has_value()) {
      return std::unexpected(create_result.error());
    }
  }
  
  // Get all notes in source notebook
  auto notes_result = getNotesInNotebook(from_notebook);
  if (!notes_result.has_value()) {
    return std::unexpected(notes_result.error());
  }
  
  // Move each note
  for (const auto& id : notes_result.value()) {
    auto note_result = note_store_.load(id);
    if (!note_result.has_value()) {
      continue;  // Skip notes that can't be loaded
    }
    
    auto note = note_result.value();
    note.setNotebook(to_notebook);
    note.touch();
    
    auto store_result = note_store_.store(note);
    if (!store_result.has_value()) {
      return std::unexpected(store_result.error());
    }
  }
  
  return {};
}

Result<std::vector<nx::core::NoteId>> NotebookManager::getNotesInNotebook(const std::string& name) {
  NoteQuery query;
  query.notebook = name;
  return note_store_.list(query);
}

Result<size_t> NotebookManager::cleanupEmptyNotebooks() {
  auto notebooks_result = note_store_.getAllNotebooks();
  if (!notebooks_result.has_value()) {
    return std::unexpected(notebooks_result.error());
  }
  
  size_t cleaned_up = 0;
  
  for (const auto& name : notebooks_result.value()) {
    if (name == DEFAULT_NOTEBOOK) {
      continue;  // Never clean up default notebook
    }
    
    auto notes_result = getNotesInNotebook(name);
    if (!notes_result.has_value()) {
      continue;
    }
    
    // Check if notebook only contains placeholder notes
    bool has_user_notes = false;
    for (const auto& id : notes_result.value()) {
      auto note_result = note_store_.load(id);
      if (note_result.has_value()) {
        const auto& note = note_result.value();
        if (!note.title().starts_with(".notebook_")) {
          has_user_notes = true;
          break;
        }
      }
    }
    
    if (!has_user_notes) {
      auto delete_result = deleteNotebook(name, true);
      if (delete_result.has_value()) {
        cleaned_up++;
      }
    }
  }
  
  return cleaned_up;
}

Result<std::vector<std::string>> NotebookManager::validateNotebooks() {
  std::vector<std::string> errors;
  
  auto notebooks_result = note_store_.getAllNotebooks();
  if (!notebooks_result.has_value()) {
    errors.push_back("Failed to retrieve notebook list: " + notebooks_result.error().message());
    return errors;
  }
  
  for (const auto& name : notebooks_result.value()) {
    // Validate notebook name
    auto validation_result = validateNotebookName(name);
    if (!validation_result.has_value()) {
      errors.push_back("Invalid notebook name '" + name + "': " + validation_result.error().message());
    }
    
    // Check for orphaned notebook references
    auto notes_result = getNotesInNotebook(name);
    if (!notes_result.has_value()) {
      errors.push_back("Failed to access notes in notebook '" + name + "': " + notes_result.error().message());
      continue;
    }
    
    if (notes_result.value().empty()) {
      errors.push_back("Empty notebook detected: '" + name + "'");
    }
  }
  
  return errors;
}

// Private helper methods

Result<void> NotebookManager::validateNotebookName(const std::string& name) {
  if (name.empty()) {
    return std::unexpected(makeError(ErrorCode::kValidationError, "Notebook name cannot be empty"));
  }
  
  if (name.length() > MAX_NOTEBOOK_NAME_LENGTH) {
    return std::unexpected(makeError(ErrorCode::kValidationError, 
      "Notebook name too long (max " + std::to_string(MAX_NOTEBOOK_NAME_LENGTH) + " characters)"));
  }
  
  // Check for invalid characters (allow letters, numbers, spaces, hyphens, underscores)
  std::regex valid_name_pattern("^[a-zA-Z0-9 _-]+$");
  if (!std::regex_match(name, valid_name_pattern)) {
    return std::unexpected(makeError(ErrorCode::kValidationError, 
      "Notebook name contains invalid characters. Only letters, numbers, spaces, hyphens, and underscores are allowed"));
  }
  
  // Check for reserved names
  const std::vector<std::string> reserved_names = {
    ".", "..", "tmp", "temp", "cache", "index"
  };
  
  for (const auto& reserved : reserved_names) {
    if (name == reserved) {
      return std::unexpected(makeError(ErrorCode::kValidationError, 
        "'" + name + "' is a reserved notebook name"));
    }
  }
  
  return {};
}

Result<NotebookInfo> NotebookManager::calculateNotebookStats(const std::string& name) {
  NotebookInfo info(name);
  
  // Get all notes in notebook
  auto notes_result = getNotesInNotebook(name);
  if (!notes_result.has_value()) {
    return std::unexpected(notes_result.error());
  }
  
  const auto& note_ids = notes_result.value();
  info.note_count = note_ids.size();
  
  // Calculate detailed statistics
  auto now = std::chrono::system_clock::now();
  auto week_ago = now - std::chrono::hours(24 * 7);
  
  std::map<std::string, size_t> all_tag_counts;
  auto earliest_created = std::chrono::system_clock::time_point::max();
  auto latest_modified = std::chrono::system_clock::time_point::min();
  
  for (const auto& id : note_ids) {
    auto note_result = note_store_.load(id);
    if (!note_result.has_value()) {
      continue;
    }
    
    const auto& note = note_result.value();
    const auto& metadata = note.metadata();
    
    // Skip placeholder notes for user-facing statistics
    if (note.title().starts_with(".notebook_")) {
      info.note_count--;  // Don't count placeholder notes
      continue;
    }
    
    // Update timestamps
    if (metadata.created() < earliest_created) {
      earliest_created = metadata.created();
      info.created = earliest_created;
    }
    
    if (metadata.updated() > latest_modified) {
      latest_modified = metadata.updated();
      info.last_modified = latest_modified;
    }
    
    // Count recent notes
    if (metadata.updated() > week_ago) {
      info.recent_notes++;
    }
    
    // Accumulate content size
    info.total_size += note.content().size();
    
    // Count tags
    for (const auto& tag : note.tags()) {
      all_tag_counts[tag]++;
    }
  }
  
  // Get top tags
  info.tag_counts = all_tag_counts;
  info.tags = getTopTags(all_tag_counts, TOP_TAGS_LIMIT);
  
  return info;
}

Result<std::map<std::string, size_t>> NotebookManager::getTagCountsForNotebook(const std::string& name) {
  std::map<std::string, size_t> tag_counts;
  
  auto notes_result = getNotesInNotebook(name);
  if (!notes_result.has_value()) {
    return std::unexpected(notes_result.error());
  }
  
  for (const auto& id : notes_result.value()) {
    auto note_result = note_store_.load(id);
    if (!note_result.has_value()) {
      continue;
    }
    
    const auto& note = note_result.value();
    
    // Skip placeholder notes
    if (note.title().starts_with(".notebook_")) {
      continue;
    }
    
    for (const auto& tag : note.tags()) {
      tag_counts[tag]++;
    }
  }
  
  return tag_counts;
}

std::vector<std::string> NotebookManager::getTopTags(const std::map<std::string, size_t>& tag_counts, 
                                                    size_t limit) {
  // Create vector of pairs for sorting
  std::vector<std::pair<size_t, std::string>> tag_pairs;
  for (const auto& [tag, count] : tag_counts) {
    tag_pairs.emplace_back(count, tag);
  }
  
  // Sort by count (descending), then by name (ascending)
  std::sort(tag_pairs.begin(), tag_pairs.end(), 
    [](const auto& a, const auto& b) {
      if (a.first != b.first) {
        return a.first > b.first;  // Higher counts first
      }
      return a.second < b.second;  // Alphabetical for same counts
    });
  
  // Extract top tags
  std::vector<std::string> top_tags;
  for (size_t i = 0; i < std::min(limit, tag_pairs.size()); ++i) {
    top_tags.push_back(tag_pairs[i].second);
  }
  
  return top_tags;
}

}  // namespace nx::store