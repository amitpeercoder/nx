#include "nx/cli/commands/gc_command.hpp"

#include <fstream>
#include <sstream>
#include <chrono>
#include <regex>

#include <nlohmann/json.hpp>

namespace nx::cli {

GcCommand::GcCommand(Application& app) : app_(app) {}

void GcCommand::setupCommand(CLI::App* cmd) {
  cmd->description("Garbage collection and storage optimization");
  
  // Add subcommands
  auto cleanup_cmd = cmd->add_subcommand("cleanup", "Remove orphaned attachments and temporary files");
  cleanup_cmd->callback([this]() { cleanup_mode_ = true; });
  
  auto optimize_cmd = cmd->add_subcommand("optimize", "Optimize database indexes and storage");
  optimize_cmd->callback([this]() { optimize_mode_ = true; });
  
  auto vacuum_cmd = cmd->add_subcommand("vacuum", "Compact SQLite database");
  vacuum_cmd->callback([this]() { vacuum_mode_ = true; });
  
  auto stats_cmd = cmd->add_subcommand("stats", "Show storage statistics");
  stats_cmd->callback([this]() { stats_mode_ = true; });
  
  auto all_cmd = cmd->add_subcommand("all", "Perform all garbage collection operations");
  all_cmd->callback([this]() { all_mode_ = true; });
  
  // Global options for all subcommands
  cmd->add_flag("--dry-run", dry_run_, "Show what would be done without making changes");
  cmd->add_flag("--force", force_, "Force operations without confirmation prompts");
  
  // Require exactly one subcommand
  cmd->require_subcommand(1);
}

Result<int> GcCommand::execute(const GlobalOptions& options) {
  if (cleanup_mode_) {
    return executeCleanup();
  } else if (optimize_mode_) {
    return executeOptimize();
  } else if (vacuum_mode_) {
    return executeVacuum();
  } else if (stats_mode_) {
    return executeStats();
  } else if (all_mode_) {
    return executeAll();
  }
  
  return std::unexpected(makeError(ErrorCode::kInvalidArgument, "No subcommand specified"));
}

Result<int> GcCommand::executeCleanup() {
  if (!dry_run_) {
    std::cout << "🧹 Performing cleanup operations...\n\n";
  } else {
    std::cout << "🔍 Dry run: showing what would be cleaned up...\n\n";
  }
  
  size_t total_files_removed = 0;
  
  // Clean up orphaned attachments
  auto orphaned_result = cleanupOrphanedAttachments();
  if (orphaned_result.has_value()) {
    total_files_removed += orphaned_result.value();
    std::cout << "📎 Orphaned attachments: " << orphaned_result.value() << " files\n";
  } else {
    std::cout << "⚠️  Failed to clean orphaned attachments: " << orphaned_result.error().message() << "\n";
  }
  
  // Clean up temporary files
  auto temp_result = cleanupTemporaryFiles();
  if (temp_result.has_value()) {
    total_files_removed += temp_result.value();
    std::cout << "🗂️  Temporary files: " << temp_result.value() << " files\n";
  } else {
    std::cout << "⚠️  Failed to clean temporary files: " << temp_result.error().message() << "\n";
  }
  
  if (!dry_run_) {
    std::cout << "\n✅ Cleanup completed: " << total_files_removed << " files removed\n";
  } else {
    std::cout << "\n📊 Would remove: " << total_files_removed << " files\n";
  }
  
  return 0;
}

Result<int> GcCommand::executeOptimize() {
  if (!dry_run_) {
    std::cout << "⚙️  Optimizing database and indexes...\n\n";
    
    auto optimize_result = optimizeDatabase();
    if (optimize_result.has_value()) {
      std::cout << "✅ Database optimization completed\n";
    } else {
      std::cout << "❌ Database optimization failed: " << optimize_result.error().message() << "\n";
      return 1;
    }
  } else {
    std::cout << "🔍 Dry run: would optimize database indexes and storage\n";
  }
  
  return 0;
}

Result<int> GcCommand::executeVacuum() {
  if (!dry_run_) {
    if (!force_) {
      std::cout << "⚠️  Database vacuum can take time and temporarily use extra disk space.\n";
      std::cout << "Continue? (y/N): ";
      std::string confirm;
      std::getline(std::cin, confirm);
      
      if (confirm != "y" && confirm != "Y") {
        std::cout << "Vacuum cancelled.\n";
        return 0;
      }
    }
    
    std::cout << "🗜️  Vacuuming database...\n";
    
    auto vacuum_result = vacuumDatabase();
    if (vacuum_result.has_value()) {
      std::cout << "✅ Database vacuum completed\n";
    } else {
      std::cout << "❌ Database vacuum failed: " << vacuum_result.error().message() << "\n";
      return 1;
    }
  } else {
    std::cout << "🔍 Dry run: would vacuum database to reclaim space\n";
  }
  
  return 0;
}

Result<int> GcCommand::executeStats() {
  std::cout << "📊 Calculating storage statistics...\n\n";
  
  auto stats_result = getStorageStats();
  if (!stats_result.has_value()) {
    std::cout << "❌ Failed to calculate statistics: " << stats_result.error().message() << "\n";
    return 1;
  }
  
  auto stats = stats_result.value();
  printStats(stats, app_.globalOptions().json);
  
  return 0;
}

Result<int> GcCommand::executeAll() {
  if (!force_ && !dry_run_) {
    std::cout << "⚠️  This will perform all garbage collection operations:\n";
    std::cout << "   • Clean up orphaned attachments\n";
    std::cout << "   • Remove temporary files\n";
    std::cout << "   • Optimize database indexes\n";
    std::cout << "   • Vacuum database\n\n";
    std::cout << "Continue? (y/N): ";
    std::string confirm;
    std::getline(std::cin, confirm);
    
    if (confirm != "y" && confirm != "Y") {
      std::cout << "Garbage collection cancelled.\n";
      return 0;
    }
  }
  
  // Get initial stats
  auto initial_stats = getStorageStats();
  if (!initial_stats.has_value()) {
    std::cout << "❌ Failed to get initial statistics\n";
    return 1;
  }
  
  if (!dry_run_) {
    std::cout << "🧹 Performing complete garbage collection...\n\n";
  } else {
    std::cout << "🔍 Dry run: showing what would be done...\n\n";
  }
  
  // Execute all operations
  auto cleanup_result = executeCleanup();
  if (!cleanup_result.has_value() || cleanup_result.value() != 0) {
    return cleanup_result;
  }
  
  std::cout << "\n";
  auto optimize_result = executeOptimize();
  if (!optimize_result.has_value() || optimize_result.value() != 0) {
    return optimize_result;
  }
  
  std::cout << "\n";
  auto vacuum_result = executeVacuum();
  if (!vacuum_result.has_value() || vacuum_result.value() != 0) {
    return vacuum_result;
  }
  
  if (!dry_run_) {
    // Get final stats
    auto final_stats = getStorageStats();
    if (final_stats.has_value()) {
      std::cout << "\n📊 Summary:\n";
      final_stats.value().database_size_before = initial_stats.value().database_size_before;
      printStats(final_stats.value(), app_.globalOptions().json);
    }
    
    std::cout << "\n✅ Complete garbage collection finished\n";
  }
  
  return 0;
}

Result<size_t> GcCommand::cleanupOrphanedAttachments() {
  auto orphaned_result = findOrphanedAttachments();
  if (!orphaned_result.has_value()) {
    return std::unexpected(orphaned_result.error());
  }
  
  auto orphaned_files = orphaned_result.value();
  size_t removed_count = 0;
  
  for (const auto& file_path : orphaned_files) {
    if (!dry_run_) {
      std::error_code ec;
      if (std::filesystem::remove(file_path, ec)) {
        removed_count++;
      }
    } else {
      removed_count++; // Count what would be removed in dry run
    }
  }
  
  return removed_count;
}

Result<size_t> GcCommand::cleanupTemporaryFiles() {
  auto temp_result = findTemporaryFiles();
  if (!temp_result.has_value()) {
    return std::unexpected(temp_result.error());
  }
  
  auto temp_files = temp_result.value();
  size_t removed_count = 0;
  
  for (const auto& file_path : temp_files) {
    if (!dry_run_) {
      std::error_code ec;
      if (std::filesystem::remove(file_path, ec)) {
        removed_count++;
      }
    } else {
      removed_count++; // Count what would be removed in dry run
    }
  }
  
  return removed_count;
}

Result<void> GcCommand::optimizeDatabase() {
  // Get database connection through search index
  auto& search_index = app_.searchIndex();
  
  // Start a transaction for optimization
  auto tx_result = search_index.beginTransaction();
  if (!tx_result.has_value()) {
    return std::unexpected(tx_result.error());
  }
  
  // Optimize the search index
  auto optimize_result = search_index.optimize();
  if (!optimize_result.has_value()) {
    search_index.rollbackTransaction();
    return std::unexpected(optimize_result.error());
  }
  
  // Commit the transaction
  auto commit_result = search_index.commitTransaction();
  if (!commit_result.has_value()) {
    return std::unexpected(commit_result.error());
  }
  
  return {};
}

Result<void> GcCommand::vacuumDatabase() {
  // Get database connection through search index
  auto& search_index = app_.searchIndex();
  
  // Perform vacuum operation
  auto vacuum_result = search_index.vacuum();
  if (!vacuum_result.has_value()) {
    return std::unexpected(vacuum_result.error());
  }
  
  return {};
}

Result<GcCommand::GcStats> GcCommand::getStorageStats() {
  GcStats stats;
  
  try {
    // Calculate database size
    auto db_path = app_.config().data_dir / "search.db";
    if (std::filesystem::exists(db_path)) {
      stats.database_size_before = std::filesystem::file_size(db_path);
      stats.database_size_after = stats.database_size_before;
    }
    
    // Count orphaned attachments
    auto orphaned_result = findOrphanedAttachments();
    if (orphaned_result.has_value()) {
      stats.orphaned_attachments = orphaned_result.value().size();
    }
    
    // Count temporary files
    auto temp_result = findTemporaryFiles();
    if (temp_result.has_value()) {
      stats.temp_files_removed = temp_result.value().size();
    }
    
    // Calculate space that would be freed
    if (orphaned_result.has_value()) {
      for (const auto& file : orphaned_result.value()) {
        if (std::filesystem::exists(file)) {
          stats.space_freed += std::filesystem::file_size(file);
        }
      }
    }
    
    if (temp_result.has_value()) {
      for (const auto& file : temp_result.value()) {
        if (std::filesystem::exists(file)) {
          stats.space_freed += std::filesystem::file_size(file);
        }
      }
    }
    
  } catch (const std::filesystem::filesystem_error& e) {
    return std::unexpected(makeError(ErrorCode::kFileError, 
                     "Filesystem error calculating stats: " + std::string(e.what())));
  }
  
  return stats;
}

Result<std::vector<std::filesystem::path>> GcCommand::findOrphanedAttachments() {
  std::vector<std::filesystem::path> orphaned_files;
  
  // Get attachments directory
  auto attachments_dir = app_.config().data_dir / "attachments";
  if (!std::filesystem::exists(attachments_dir)) {
    return orphaned_files; // No attachments directory means no orphaned files
  }
  
  try {
    // Get all notes to check which attachments are referenced
    auto& note_store = app_.noteStore();
    auto all_notes_result = note_store.list();
    if (!all_notes_result.has_value()) {
      return std::unexpected(all_notes_result.error());
    }
    
    // Collect all referenced attachment filenames
    std::set<std::string> referenced_attachments;
    for (const auto& note_id : all_notes_result.value()) {
      auto note_result = note_store.load(note_id);
      if (note_result.has_value()) {
        const auto& content = note_result.value().content();
        
        // Look for attachment references in note content
        // Pattern: ![alt text](attachments/filename) or [link text](attachments/filename)
        std::regex attachment_regex(R"(\[([^\]]*)\]\(attachments/([^)]+)\))");
        std::sregex_iterator iter(content.begin(), content.end(), attachment_regex);
        std::sregex_iterator end;
        
        while (iter != end) {
          std::smatch match = *iter;
          referenced_attachments.insert(match[2].str());
          ++iter;
        }
      }
    }
    
    // Find files in attachments directory that aren't referenced
    for (const auto& entry : std::filesystem::recursive_directory_iterator(attachments_dir)) {
      if (entry.is_regular_file()) {
        auto relative_path = std::filesystem::relative(entry.path(), attachments_dir);
        auto filename = relative_path.string();
        
        if (referenced_attachments.find(filename) == referenced_attachments.end()) {
          orphaned_files.push_back(entry.path());
        }
      }
    }
    
  } catch (const std::filesystem::filesystem_error& e) {
    return std::unexpected(makeError(ErrorCode::kFileError, 
                     "Error scanning attachments: " + std::string(e.what())));
  }
  
  return orphaned_files;
}

Result<std::vector<std::filesystem::path>> GcCommand::findTemporaryFiles() {
  std::vector<std::filesystem::path> temp_files;
  
  try {
    // Check system temp directory for nx-related temporary files
    auto temp_dir = std::filesystem::temp_directory_path();
    
    for (const auto& entry : std::filesystem::directory_iterator(temp_dir)) {
      if (entry.is_regular_file()) {
        auto filename = entry.path().filename().string();
        
        // Look for nx-specific temporary files
        if (filename.find("nx_") == 0 || 
            filename.find("nx-") == 0 ||
            filename.find("notion_extract_") != std::string::npos ||
            filename.find("nx_backup_") == 0) {
          
          // Check if file is older than 1 hour (cleanup old temp files)
          auto file_time = std::filesystem::last_write_time(entry.path());
          auto now = std::filesystem::file_time_type::clock::now();
          auto age = now - file_time;
          
          if (age > std::chrono::hours(1)) {
            temp_files.push_back(entry.path());
          }
        }
      }
    }
    
    // Also check for temporary files in the data directory
    auto data_dir = app_.config().data_dir;
    if (std::filesystem::exists(data_dir)) {
      for (const auto& entry : std::filesystem::directory_iterator(data_dir)) {
        if (entry.is_regular_file()) {
          auto filename = entry.path().filename().string();
          
          // Look for temporary files (start with . or end with .tmp)
          if (filename[0] == '.' || 
              (filename.size() >= 4 && filename.substr(filename.size() - 4) == ".tmp") ||
              (filename.size() >= 5 && filename.substr(filename.size() - 5) == ".temp")) {
            temp_files.push_back(entry.path());
          }
        }
      }
    }
    
  } catch (const std::filesystem::filesystem_error& e) {
    return std::unexpected(makeError(ErrorCode::kFileError, 
                     "Error scanning temporary files: " + std::string(e.what())));
  }
  
  return temp_files;
}

size_t GcCommand::calculateDirectorySize(const std::filesystem::path& path) {
  size_t total_size = 0;
  
  try {
    if (std::filesystem::exists(path)) {
      for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
        if (entry.is_regular_file()) {
          total_size += std::filesystem::file_size(entry);
        }
      }
    }
  } catch (const std::filesystem::filesystem_error&) {
    // Ignore errors and return partial size
  }
  
  return total_size;
}

void GcCommand::printStats(const GcStats& stats, bool json_output) {
  if (json_output) {
    nlohmann::json output;
    output["orphaned_attachments"] = stats.orphaned_attachments;
    output["temp_files"] = stats.temp_files_removed;
    output["database_size_before"] = stats.database_size_before;
    output["database_size_after"] = stats.database_size_after;
    output["space_freed"] = stats.space_freed;
    output["vacuum_performed"] = stats.vacuum_performed;
    output["index_optimized"] = stats.index_optimized;
    
    std::cout << output.dump(2) << "\n";
  } else {
    // Human-readable format with nice formatting
    auto format_bytes = [](size_t bytes) -> std::string {
      if (bytes < 1024) return std::to_string(bytes) + " B";
      if (bytes < 1024 * 1024) return std::to_string(bytes / 1024) + " KB";
      if (bytes < 1024 * 1024 * 1024) return std::to_string(bytes / (1024 * 1024)) + " MB";
      return std::to_string(bytes / (1024 * 1024 * 1024)) + " GB";
    };
    
    std::cout << "Storage Statistics:\n";
    std::cout << "  📎 Orphaned attachments: " << stats.orphaned_attachments << " files\n";
    std::cout << "  🗂️  Temporary files: " << stats.temp_files_removed << " files\n";
    std::cout << "  💾 Database size: " << format_bytes(stats.database_size_before);
    
    if (stats.database_size_after != stats.database_size_before) {
      std::cout << " → " << format_bytes(stats.database_size_after);
      size_t db_saved = stats.database_size_before - stats.database_size_after;
      std::cout << " (saved " << format_bytes(db_saved) << ")";
    }
    std::cout << "\n";
    
    if (stats.space_freed > 0) {
      std::cout << "  💿 Space that can be freed: " << format_bytes(stats.space_freed) << "\n";
    }
    
    if (stats.vacuum_performed) {
      std::cout << "  ✅ Database vacuum: completed\n";
    }
    
    if (stats.index_optimized) {
      std::cout << "  ✅ Index optimization: completed\n";
    }
  }
}

} // namespace nx::cli