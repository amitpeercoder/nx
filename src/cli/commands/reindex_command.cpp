#include "nx/cli/commands/reindex_command.hpp"

#include <iostream>
#include <chrono>
#include <iomanip>
#include <nlohmann/json.hpp>

namespace nx::cli {

ReindexCommand::ReindexCommand(Application& app) : app_(app) {
}

Result<int> ReindexCommand::execute(const GlobalOptions& options) {
  try {
    if (stats_only_) {
      return executeStats();
    } else if (validate_only_) {
      return executeValidate();
    } else if (optimize_only_) {
      return executeOptimize();
    } else {
      return executeRebuild();
    }
    
  } catch (const std::exception& e) {
    if (options.json) {
      std::cout << R"({"error": ")" << e.what() << R"(", "operation": "reindex"})" << std::endl;
    } else {
      std::cout << "Error: " << e.what() << std::endl;
    }
    return 1;
  }
}

void ReindexCommand::setupCommand(CLI::App* cmd) {
  cmd->add_flag("--force", force_, "Force rebuild even if index appears healthy");
  cmd->add_flag("--optimize", optimize_only_, "Only optimize existing index, don't rebuild");
  cmd->add_flag("--validate", validate_only_, "Only validate index integrity");
  cmd->add_flag("--stats", stats_only_, "Show index statistics only");
  cmd->add_option("--type", index_type_, "Index type (sqlite, ripgrep) - defaults to current config");
}

Result<int> ReindexCommand::executeRebuild() {
  auto& search_index = app_.searchIndex();
  const auto& options = app_.globalOptions();
  
  outputProgress("Starting index rebuild...", options);
  
  // Check current index health unless forced
  if (!force_) {
    auto health_result = search_index.isHealthy();
    if (health_result.has_value() && *health_result) {
      auto validate_result = search_index.validateIndex();
      if (validate_result.has_value()) {
        if (options.json) {
          std::cout << R"({"status": "healthy", "action": "skipped", "message": "Index is healthy. Use --force to rebuild anyway."})" << std::endl;
        } else {
          std::cout << "Index appears healthy. Use --force to rebuild anyway." << std::endl;
          std::cout << "Run 'nx reindex --validate' for detailed integrity check." << std::endl;
        }
        return 0;
      }
    }
    
    outputProgress("Index health check failed, proceeding with rebuild...", options);
  }
  
  // Begin transaction for atomic rebuild
  auto tx_result = search_index.beginTransaction();
  if (!tx_result.has_value()) {
    if (options.json) {
      std::cout << R"({"error": ")" << tx_result.error().message() << R"(", "operation": "begin_transaction"})" << std::endl;
    } else {
      std::cout << "Error starting transaction: " << tx_result.error().message() << std::endl;
    }
    return 1;
  }
  
  // Get stats before rebuild
  auto stats_before = search_index.getStats();
  auto start_time = std::chrono::steady_clock::now();
  
  outputProgress("Rebuilding search index...", options);
  
  // Perform rebuild
  auto rebuild_result = search_index.rebuild();
  if (!rebuild_result.has_value()) {
    // Rollback on failure
    search_index.rollbackTransaction();
    
    if (options.json) {
      std::cout << R"({"error": ")" << rebuild_result.error().message() << R"(", "operation": "rebuild"})" << std::endl;
    } else {
      std::cout << "Error rebuilding index: " << rebuild_result.error().message() << std::endl;
    }
    return 1;
  }
  
  outputProgress("Optimizing index...", options);
  
  // Optimize after rebuild
  auto optimize_result = search_index.optimize();
  if (!optimize_result.has_value()) {
    // Continue even if optimization fails - rebuild was successful
    if (!options.quiet) {
      if (options.json) {
        std::cout << R"({"warning": ")" << optimize_result.error().message() << R"(", "operation": "optimize"})" << std::endl;
      } else {
        std::cout << "Warning: Index optimization failed: " << optimize_result.error().message() << std::endl;
      }
    }
  }
  
  // Commit transaction
  auto commit_result = search_index.commitTransaction();
  if (!commit_result.has_value()) {
    if (options.json) {
      std::cout << R"({"error": ")" << commit_result.error().message() << R"(", "operation": "commit"})" << std::endl;
    } else {
      std::cout << "Error committing changes: " << commit_result.error().message() << std::endl;
    }
    return 1;
  }
  
  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  
  // Get final stats
  auto stats_after = search_index.getStats();
  
  if (options.json) {
    nlohmann::json output;
    output["success"] = true;
    output["operation"] = "rebuild";
    output["duration_ms"] = duration.count();
    
    if (stats_before.has_value() && stats_after.has_value()) {
      output["before"]["total_notes"] = stats_before->total_notes;
      output["before"]["total_words"] = stats_before->total_words;
      output["before"]["index_size_bytes"] = stats_before->index_size_bytes;
      
      output["after"]["total_notes"] = stats_after->total_notes;
      output["after"]["total_words"] = stats_after->total_words;
      output["after"]["index_size_bytes"] = stats_after->index_size_bytes;
    }
    
    std::cout << output.dump(2) << std::endl;
  } else {
    std::cout << "Index rebuild completed successfully!" << std::endl;
    std::cout << "Duration: " << duration.count() << "ms" << std::endl;
    
    if (stats_after.has_value()) {
      std::cout << "\nIndex Statistics:" << std::endl;
      outputIndexStats(*stats_after, options);
    }
  }
  
  return 0;
}

Result<int> ReindexCommand::executeOptimize() {
  auto& search_index = app_.searchIndex();
  const auto& options = app_.globalOptions();
  
  outputProgress("Optimizing search index...", options);
  
  auto start_time = std::chrono::steady_clock::now();
  
  auto result = search_index.optimize();
  if (!result.has_value()) {
    if (options.json) {
      std::cout << R"({"error": ")" << result.error().message() << R"(", "operation": "optimize"})" << std::endl;
    } else {
      std::cout << "Error optimizing index: " << result.error().message() << std::endl;
    }
    return 1;
  }
  
  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  
  // Get stats after optimization
  auto stats = search_index.getStats();
  
  if (options.json) {
    nlohmann::json output;
    output["success"] = true;
    output["operation"] = "optimize";
    output["duration_ms"] = duration.count();
    
    if (stats.has_value()) {
      output["stats"]["total_notes"] = stats->total_notes;
      output["stats"]["total_words"] = stats->total_words;
      output["stats"]["index_size_bytes"] = stats->index_size_bytes;
    }
    
    std::cout << output.dump(2) << std::endl;
  } else {
    std::cout << "Index optimization completed successfully!" << std::endl;
    std::cout << "Duration: " << duration.count() << "ms" << std::endl;
    
    if (stats.has_value()) {
      std::cout << "\nIndex Statistics:" << std::endl;
      outputIndexStats(*stats, options);
    }
  }
  
  return 0;
}

Result<int> ReindexCommand::executeValidate() {
  auto& search_index = app_.searchIndex();
  const auto& options = app_.globalOptions();
  
  outputProgress("Validating search index...", options);
  
  auto start_time = std::chrono::steady_clock::now();
  
  // Check basic health
  auto health_result = search_index.isHealthy();
  if (!health_result.has_value() || !*health_result) {
    if (options.json) {
      std::cout << R"({"healthy": false, "validation": "failed", "message": "Basic health check failed"})" << std::endl;
    } else {
      std::cout << "Index health check failed - index may be corrupted" << std::endl;
      std::cout << "Run 'nx reindex' to rebuild the index" << std::endl;
    }
    return 1;
  }
  
  // Perform detailed validation
  auto validate_result = search_index.validateIndex();
  
  auto end_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
  
  if (!validate_result.has_value()) {
    if (options.json) {
      std::cout << R"({"healthy": false, "validation": "failed", "error": ")" 
                << validate_result.error().message() << R"(", "duration_ms": )" 
                << duration.count() << "}" << std::endl;
    } else {
      std::cout << "Index validation failed: " << validate_result.error().message() << std::endl;
      std::cout << "Run 'nx reindex' to rebuild the index" << std::endl;
    }
    return 1;
  }
  
  // Get stats for comprehensive report
  auto stats = search_index.getStats();
  
  if (options.json) {
    nlohmann::json output;
    output["healthy"] = true;
    output["validation"] = "passed";
    output["duration_ms"] = duration.count();
    
    if (stats.has_value()) {
      output["stats"]["total_notes"] = stats->total_notes;
      output["stats"]["total_words"] = stats->total_words;
      output["stats"]["index_size_bytes"] = stats->index_size_bytes;
      
      // Format timestamps
      auto last_updated = std::chrono::system_clock::to_time_t(stats->last_updated);
      auto last_optimized = std::chrono::system_clock::to_time_t(stats->last_optimized);
      
      output["stats"]["last_updated"] = last_updated;
      output["stats"]["last_optimized"] = last_optimized;
    }
    
    std::cout << output.dump(2) << std::endl;
  } else {
    std::cout << "Index validation passed - index is healthy!" << std::endl;
    std::cout << "Validation duration: " << duration.count() << "ms" << std::endl;
    
    if (stats.has_value()) {
      std::cout << "\nIndex Statistics:" << std::endl;
      outputIndexStats(*stats, options);
    }
  }
  
  return 0;
}

Result<int> ReindexCommand::executeStats() {
  auto& search_index = app_.searchIndex();
  const auto& options = app_.globalOptions();
  
  auto stats_result = search_index.getStats();
  if (!stats_result.has_value()) {
    if (options.json) {
      std::cout << R"({"error": ")" << stats_result.error().message() << R"(", "operation": "get_stats"})" << std::endl;
    } else {
      std::cout << "Error getting index statistics: " << stats_result.error().message() << std::endl;
    }
    return 1;
  }
  
  auto stats = stats_result.value();
  
  if (options.json) {
    nlohmann::json output;
    output["total_notes"] = stats.total_notes;
    output["total_words"] = stats.total_words;
    output["index_size_bytes"] = stats.index_size_bytes;
    
    // Format timestamps
    auto last_updated = std::chrono::system_clock::to_time_t(stats.last_updated);
    auto last_optimized = std::chrono::system_clock::to_time_t(stats.last_optimized);
    
    output["last_updated"] = last_updated;
    output["last_optimized"] = last_optimized;
    
    std::cout << output.dump(2) << std::endl;
  } else {
    outputIndexStats(stats, options);
  }
  
  return 0;
}

void ReindexCommand::outputIndexStats(const nx::index::IndexStats& stats, const GlobalOptions& options) {
  if (options.json) {
    return; // JSON output handled by caller
  }
  
  std::cout << "  Total notes: " << stats.total_notes << std::endl;
  std::cout << "  Total words: " << stats.total_words << std::endl;
  
  // Format file size
  double size_mb = static_cast<double>(stats.index_size_bytes) / (1024.0 * 1024.0);
  std::cout << "  Index size: " << std::fixed << std::setprecision(2) << size_mb << " MB" << std::endl;
  
  // Format timestamps
  auto last_updated = std::chrono::system_clock::to_time_t(stats.last_updated);
  auto last_optimized = std::chrono::system_clock::to_time_t(stats.last_optimized);
  
  std::cout << "  Last updated: " << std::put_time(std::localtime(&last_updated), "%Y-%m-%d %H:%M:%S") << std::endl;
  std::cout << "  Last optimized: " << std::put_time(std::localtime(&last_optimized), "%Y-%m-%d %H:%M:%S") << std::endl;
  
  // Performance metrics
  if (stats.total_notes > 0) {
    double avg_words_per_note = static_cast<double>(stats.total_words) / static_cast<double>(stats.total_notes);
    std::cout << "  Avg words/note: " << std::fixed << std::setprecision(1) << avg_words_per_note << std::endl;
    
    double bytes_per_note = static_cast<double>(stats.index_size_bytes) / static_cast<double>(stats.total_notes);
    std::cout << "  Index overhead: " << std::fixed << std::setprecision(1) << bytes_per_note << " bytes/note" << std::endl;
  }
}

void ReindexCommand::outputProgress(const std::string& message, const GlobalOptions& options) {
  if (!options.quiet && !options.json) {
    std::cout << message << std::endl;
  }
}

} // namespace nx::cli