#pragma once

#include "nx/cli/application.hpp"

namespace nx::cli {

/**
 * @brief Garbage collection command for cleaning up orphaned files and optimizing storage
 * 
 * Provides garbage collection operations:
 * - cleanup: Remove orphaned attachments and temporary files
 * - optimize: Optimize database storage and indexes
 * - vacuum: Compact SQLite database
 * - stats: Show storage statistics before/after cleanup
 */
class GcCommand : public Command {
public:
  explicit GcCommand(Application& app);
  
  std::string name() const override { return "gc"; }
  std::string description() const override { return "Garbage collection and storage optimization"; }
  
  Result<int> execute(const GlobalOptions& options) override;
  void setupCommand(CLI::App* cmd) override;

private:
  struct GcStats {
    size_t orphaned_attachments = 0;
    size_t temp_files_removed = 0;
    size_t database_size_before = 0;
    size_t database_size_after = 0;
    size_t space_freed = 0;
    bool vacuum_performed = false;
    bool index_optimized = false;
  };

  // Subcommand operations
  Result<int> executeCleanup();
  Result<int> executeOptimize();
  Result<int> executeVacuum();
  Result<int> executeStats();
  Result<int> executeAll();

  // Core garbage collection operations
  Result<size_t> cleanupOrphanedAttachments();
  Result<size_t> cleanupTemporaryFiles();
  Result<void> optimizeDatabase();
  Result<void> vacuumDatabase();
  Result<GcStats> getStorageStats();
  
  // Helper functions
  Result<std::vector<std::filesystem::path>> findOrphanedAttachments();
  Result<std::vector<std::filesystem::path>> findTemporaryFiles();
  size_t calculateDirectorySize(const std::filesystem::path& path);
  
  void printStats(const GcStats& stats, bool json_output);

  // Application reference
  Application& app_;

  // Command line options
  bool cleanup_mode_ = false;
  bool optimize_mode_ = false;
  bool vacuum_mode_ = false;
  bool stats_mode_ = false;
  bool all_mode_ = false;
  bool dry_run_ = false;
  bool force_ = false;
};

} // namespace nx::cli