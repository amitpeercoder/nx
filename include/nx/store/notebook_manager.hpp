#pragma once

#include <string>
#include <vector>
#include <map>
#include <chrono>

#include "nx/common.hpp"
#include "nx/core/note_id.hpp"

// Forward declarations
namespace nx::store {
class NoteStore;
}

namespace nx::store {

/**
 * @brief Information about a notebook including statistics
 */
struct NotebookInfo {
  std::string name;
  size_t note_count = 0;
  std::chrono::system_clock::time_point created;
  std::chrono::system_clock::time_point last_modified;
  std::vector<std::string> tags;                    // Most common tags in notebook
  std::map<std::string, size_t> tag_counts;        // Tag frequency within notebook
  
  // Statistics
  size_t total_size = 0;        // Total content size in bytes
  size_t recent_notes = 0;      // Notes modified in last 7 days
  
  // Constructor
  NotebookInfo() = default;
  explicit NotebookInfo(const std::string& notebook_name);
};

/**
 * @brief Statistics for all notebooks combined
 */
struct NotebookStats {
  size_t total_notebooks = 0;
  size_t total_notes = 0;
  std::string most_active_notebook;         // Notebook with most recent activity
  std::string largest_notebook;             // Notebook with most notes
  std::chrono::system_clock::time_point last_activity;
};

/**
 * @brief Manager for notebook operations and statistics
 * 
 * Provides high-level operations for managing notebooks, including
 * creation, deletion, renaming, and statistical analysis.
 */
class NotebookManager {
public:
  /**
   * @brief Constructor
   * @param note_store Reference to the note store
   */
  explicit NotebookManager(NoteStore& note_store);

  // Notebook CRUD operations
  
  /**
   * @brief Create a new notebook
   * @param name Notebook name (must be unique)
   * @return Result indicating success or error
   */
  Result<void> createNotebook(const std::string& name);
  
  /**
   * @brief Delete a notebook and optionally its notes
   * @param name Notebook name
   * @param force If true, delete even if notebook contains notes
   * @return Result indicating success or error
   */
  Result<void> deleteNotebook(const std::string& name, bool force = false);
  
  /**
   * @brief Rename a notebook
   * @param old_name Current notebook name
   * @param new_name New notebook name (must be unique)
   * @return Result indicating success or error
   */
  Result<void> renameNotebook(const std::string& old_name, const std::string& new_name);

  // Query operations
  
  /**
   * @brief List all notebooks with their information
   * @param include_stats If true, calculate detailed statistics (slower)
   * @return Vector of NotebookInfo objects
   */
  Result<std::vector<NotebookInfo>> listNotebooks(bool include_stats = true);
  
  /**
   * @brief Get information about a specific notebook
   * @param name Notebook name
   * @param include_stats If true, calculate detailed statistics
   * @return NotebookInfo object or error if not found
   */
  Result<NotebookInfo> getNotebookInfo(const std::string& name, bool include_stats = true);
  
  /**
   * @brief Check if a notebook exists
   * @param name Notebook name
   * @return true if notebook exists, false otherwise
   */
  Result<bool> notebookExists(const std::string& name);

  /**
   * @brief Get overall notebook statistics
   * @return Combined statistics for all notebooks
   */
  Result<NotebookStats> getOverallStats();

  // Bulk operations
  
  /**
   * @brief Move all notes from one notebook to another
   * @param from_notebook Source notebook
   * @param to_notebook Destination notebook
   * @return Result indicating success or error
   */
  Result<void> moveAllNotes(const std::string& from_notebook, const std::string& to_notebook);
  
  /**
   * @brief Get all notes in a specific notebook
   * @param name Notebook name
   * @return Vector of note IDs in the notebook
   */
  Result<std::vector<nx::core::NoteId>> getNotesInNotebook(const std::string& name);

  // Maintenance operations
  
  /**
   * @brief Clean up empty notebooks (notebooks with no notes)
   * @return Number of notebooks cleaned up
   */
  Result<size_t> cleanupEmptyNotebooks();
  
  /**
   * @brief Validate notebook consistency
   * @return Vector of validation errors (empty if all valid)
   */
  Result<std::vector<std::string>> validateNotebooks();

  // Constants
  static constexpr std::string_view DEFAULT_NOTEBOOK = "default";
  static constexpr size_t MAX_NOTEBOOK_NAME_LENGTH = 100;
  static constexpr size_t TOP_TAGS_LIMIT = 10;  // Number of top tags to include in NotebookInfo

private:
  NoteStore& note_store_;
  
  // Helper methods
  Result<void> validateNotebookName(const std::string& name);
  Result<NotebookInfo> calculateNotebookStats(const std::string& name);
  Result<std::map<std::string, size_t>> getTagCountsForNotebook(const std::string& name);
  std::vector<std::string> getTopTags(const std::map<std::string, size_t>& tag_counts, size_t limit = TOP_TAGS_LIMIT);
};

}  // namespace nx::store