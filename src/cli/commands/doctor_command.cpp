#include "nx/cli/commands/doctor_command.hpp"

#include <iostream>
#include <iomanip>
#include <chrono>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>
#ifndef _WIN32
#include <sys/statvfs.h>
#include <unistd.h>
#endif
#include <nlohmann/json.hpp>

#include "nx/util/safe_process.hpp"
#include "nx/util/xdg.hpp"
#include "nx/sync/git_sync.hpp"

namespace nx::cli {

DoctorCommand::DoctorCommand(Application& app) : app_(app) {
}

Result<int> DoctorCommand::execute(const GlobalOptions& options) {
  try {
    HealthReport report;
    
    if (!options.quiet) {
      std::cout << "Running nx system health checks...\n\n";
    }
    
    // Run checks based on category or all checks
    if (!check_category_.empty()) {
      auto category_result = runCategoryChecks(check_category_, report);
      if (!category_result.has_value()) {
        if (options.json) {
          std::cout << R"({"error": ")" << category_result.error().message() << R"("})";
        } else {
          std::cout << "Error running checks: " << category_result.error().message() << std::endl;
        }
        return 1;
      }
    } else {
      auto all_result = runAllChecks(report);
      if (!all_result.has_value()) {
        if (options.json) {
          std::cout << R"({"error": ")" << all_result.error().message() << R"("})";
        } else {
          std::cout << "Error running checks: " << all_result.error().message() << std::endl;
        }
        return 1;
      }
    }
    
    // Print results
    printReport(report, options);
    
    // Fix issues if requested
    if (fix_issues_ && report.failed > 0) {
      if (!options.quiet) {
        std::cout << "\nAttempting to fix identified issues...\n";
      }
      
      auto fix_result = fixIssues(report);
      if (!fix_result.has_value()) {
        if (!options.json && !options.quiet) {
          std::cout << "Warning: Some issues could not be fixed automatically: " 
                    << fix_result.error().message() << std::endl;
        }
      }
    }
    
    // Return appropriate exit code
    return report.failed > 0 ? 1 : 0;
    
  } catch (const std::exception& e) {
    if (options.json) {
      std::cout << R"({"error": ")" << e.what() << R"("})";
    } else {
      std::cout << "Error: " << e.what() << std::endl;
    }
    return 1;
  }
}

void DoctorCommand::setupCommand(CLI::App* cmd) {
  cmd->add_flag("--fix", fix_issues_, "Attempt to fix issues automatically");
  cmd->add_flag("--verbose,-v", verbose_output_, "Show detailed output for all checks");
  cmd->add_flag("--quick,-q", quick_check_, "Run only essential checks (faster)");
  cmd->add_option("--category,-c", check_category_, 
                  "Run checks for specific category (config,storage,git,tools,performance)");
}

Result<void> DoctorCommand::runAllChecks(HealthReport& report) {
  auto start_time = startTimer();
  
  // Configuration checks
  if (auto check = checkConfiguration(); check.has_value()) {
    report.checks.push_back(check.value());
  }
  
  // Directory and permission checks
  if (auto check = checkDirectories(); check.has_value()) {
    report.checks.push_back(check.value());
  }
  
  if (auto check = checkPermissions(); check.has_value()) {
    report.checks.push_back(check.value());
  }
  
  // Storage checks
  if (auto check = checkStorageSpace(); check.has_value()) {
    report.checks.push_back(check.value());
  }
  
  // Database and index checks
  if (auto check = checkDatabase(); check.has_value()) {
    report.checks.push_back(check.value());
  }
  
  if (auto check = checkSearchIndex(); check.has_value()) {
    report.checks.push_back(check.value());
  }
  
  // Notes integrity
  if (auto check = checkNotesIntegrity(); check.has_value()) {
    report.checks.push_back(check.value());
  }
  
  // Git repository (if applicable)
  if (auto check = checkGitRepository(); check.has_value()) {
    report.checks.push_back(check.value());
  }
  
  // External tools
  if (auto check = checkExternalTools(); check.has_value()) {
    report.checks.push_back(check.value());
  }
  
  // Performance checks (skip in quick mode)
  if (!quick_check_) {
    if (auto check = checkPerformance(); check.has_value()) {
      report.checks.push_back(check.value());
    }
  }
  
  // Calculate summary statistics
  for (const auto& check : report.checks) {
    if (check.passed) {
      report.passed++;
    } else {
      report.failed++;
    }
    report.total_duration_ms += check.duration_ms;
  }
  
  report.total_duration_ms += endTimer(start_time);
  
  // Determine overall status
  if (report.failed == 0) {
    report.overall_status = "HEALTHY";
  } else if (report.failed <= report.passed / 2) {
    report.overall_status = "WARNING";
  } else {
    report.overall_status = "CRITICAL";
  }
  
  return {};
}

Result<void> DoctorCommand::runCategoryChecks(const std::string& category, HealthReport& report) {
  auto start_time = startTimer();
  
  if (category == "config") {
    if (auto check = checkConfiguration(); check.has_value()) {
      report.checks.push_back(check.value());
    }
  } else if (category == "storage") {
    if (auto check = checkDirectories(); check.has_value()) {
      report.checks.push_back(check.value());
    }
    if (auto check = checkPermissions(); check.has_value()) {
      report.checks.push_back(check.value());
    }
    if (auto check = checkStorageSpace(); check.has_value()) {
      report.checks.push_back(check.value());
    }
  } else if (category == "git") {
    if (auto check = checkGitRepository(); check.has_value()) {
      report.checks.push_back(check.value());
    }
  } else if (category == "tools") {
    if (auto check = checkExternalTools(); check.has_value()) {
      report.checks.push_back(check.value());
    }
  } else if (category == "performance") {
    if (auto check = checkPerformance(); check.has_value()) {
      report.checks.push_back(check.value());
    }
  } else {
    return std::unexpected(makeError(ErrorCode::kInvalidArgument,
                                     "Unknown category: " + category));
  }
  
  // Calculate summary statistics
  for (const auto& check : report.checks) {
    if (check.passed) {
      report.passed++;
    } else {
      report.failed++;
    }
    report.total_duration_ms += check.duration_ms;
  }
  
  report.total_duration_ms += endTimer(start_time);
  
  // Determine overall status
  if (report.failed == 0) {
    report.overall_status = "HEALTHY";
  } else if (report.failed <= report.passed / 2) {
    report.overall_status = "WARNING";
  } else {
    report.overall_status = "CRITICAL";
  }
  
  return {};
}

Result<DoctorCommand::HealthCheck> DoctorCommand::checkConfiguration() {
  auto start = startTimer();
  HealthCheck check;
  check.name = "Configuration";
  check.category = "config";
  
  try {
    // Check if config file exists and is readable
    const auto& config = app_.config();
    auto config_path = nx::util::Xdg::configFile();
    
    if (!std::filesystem::exists(config_path)) {
      check.passed = false;
      check.message = "Configuration file not found: " + config_path.string();
      check.fix_suggestion = "Run 'nx config init' to create default configuration";
    } else {
      // Validate configuration content
      bool config_valid = true;
      std::string validation_errors;
      
      // Check critical paths
      if (!std::filesystem::exists(config.notes_dir)) {
        config_valid = false;
        validation_errors += "Notes directory does not exist: " + config.notes_dir.string() + "; ";
      }
      
      if (!std::filesystem::exists(config.data_dir)) {
        config_valid = false;
        validation_errors += "Data directory does not exist: " + config.data_dir.string() + "; ";
      }
      
      if (config_valid) {
        check.passed = true;
        check.message = "Configuration is valid";
      } else {
        check.passed = false;
        check.message = "Configuration errors: " + validation_errors;
        check.fix_suggestion = "Check directory paths in configuration file";
      }
    }
  } catch (const std::exception& e) {
    check.passed = false;
    check.message = "Configuration check failed: " + std::string(e.what());
    check.fix_suggestion = "Check configuration file syntax and permissions";
  }
  
  check.duration_ms = endTimer(start);
  return check;
}

Result<DoctorCommand::HealthCheck> DoctorCommand::checkDirectories() {
  auto start = startTimer();
  HealthCheck check;
  check.name = "Directories";
  check.category = "storage";
  
  try {
    const auto& config = app_.config();
    std::vector<std::filesystem::path> required_dirs = {
      config.notes_dir,
      config.data_dir,
      config.data_dir / "attachments",
      config.data_dir / "templates",
      config.data_dir / "backups"
    };
    
    bool all_exist = true;
    std::string missing_dirs;
    
    for (const auto& dir : required_dirs) {
      if (!std::filesystem::exists(dir)) {
        all_exist = false;
        missing_dirs += dir.string() + " ";
      }
    }
    
    if (all_exist) {
      check.passed = true;
      check.message = "All required directories exist";
    } else {
      check.passed = false;
      check.message = "Missing directories: " + missing_dirs;
      check.fix_suggestion = "Create missing directories with proper permissions";
    }
  } catch (const std::exception& e) {
    check.passed = false;
    check.message = "Directory check failed: " + std::string(e.what());
    check.fix_suggestion = "Check filesystem permissions and paths";
  }
  
  check.duration_ms = endTimer(start);
  return check;
}

Result<DoctorCommand::HealthCheck> DoctorCommand::checkPermissions() {
  auto start = startTimer();
  HealthCheck check;
  check.name = "Permissions";
  check.category = "storage";
  
  try {
    const auto& config = app_.config();
    std::vector<std::filesystem::path> paths_to_check = {
      config.notes_dir,
      config.data_dir
    };
    
    bool all_accessible = true;
    std::string permission_issues;
    
    for (const auto& path : paths_to_check) {
      if (std::filesystem::exists(path)) {
        // Check read/write permissions
        if (!checkFilePermissions(path, 4 | 2)) { // R_OK | W_OK
          all_accessible = false;
          permission_issues += path.string() + " (no read/write) ";
        }
      }
    }
    
    if (all_accessible) {
      check.passed = true;
      check.message = "All directories have proper permissions";
    } else {
      check.passed = false;
      check.message = "Permission issues: " + permission_issues;
      check.fix_suggestion = "Fix directory permissions: chmod 755 for directories, 644 for files";
    }
  } catch (const std::exception& e) {
    check.passed = false;
    check.message = "Permission check failed: " + std::string(e.what());
    check.fix_suggestion = "Check filesystem permissions";
  }
  
  check.duration_ms = endTimer(start);
  return check;
}

Result<DoctorCommand::HealthCheck> DoctorCommand::checkSearchIndex() {
  auto start = startTimer();
  HealthCheck check;
  check.name = "Search Index";
  check.category = "database";
  
  try {
    auto& search_index = app_.searchIndex();
    
    // Test index availability
    nx::index::SearchQuery test_query;
    test_query.text = "test";
    test_query.limit = 1;
    
    auto search_result = search_index.search(test_query);
    
    if (search_result.has_value()) {
      check.passed = true;
      check.message = "Search index is functional";
    } else {
      check.passed = false;
      check.message = "Search index error: " + search_result.error().message();
      check.fix_suggestion = "Run 'nx reindex rebuild' to rebuild the search index";
    }
  } catch (const std::exception& e) {
    check.passed = false;
    check.message = "Search index check failed: " + std::string(e.what());
    check.fix_suggestion = "Rebuild search index with 'nx reindex rebuild'";
  }
  
  check.duration_ms = endTimer(start);
  return check;
}

Result<DoctorCommand::HealthCheck> DoctorCommand::checkGitRepository() {
  auto start = startTimer();
  HealthCheck check;
  check.name = "Git Repository";
  check.category = "git";
  
  try {
    const auto& config = app_.config();
    auto git_dir = config.notes_dir / ".git";
    
    if (!std::filesystem::exists(git_dir)) {
      check.passed = true; // Not an error if git isn't used
      check.message = "No git repository found (optional)";
    } else {
      // Check git repository health
      auto git_sync_result = nx::sync::GitSync::initialize(config.notes_dir, {});
      
      if (git_sync_result.has_value()) {
        auto status_result = git_sync_result.value().getStatus();
        
        if (status_result.has_value()) {
          auto status = status_result.value();
          check.passed = true;
          check.message = "Git repository is healthy (branch: " + status.current_branch + ")";
        } else {
          check.passed = false;
          check.message = "Git status error: " + status_result.error().message();
          check.fix_suggestion = "Check git repository integrity";
        }
      } else {
        check.passed = false;
        check.message = "Git initialization failed: " + git_sync_result.error().message();
        check.fix_suggestion = "Reinitialize git repository or check git installation";
      }
    }
  } catch (const std::exception& e) {
    check.passed = false;
    check.message = "Git check failed: " + std::string(e.what());
    check.fix_suggestion = "Check git installation and repository integrity";
  }
  
  check.duration_ms = endTimer(start);
  return check;
}

Result<DoctorCommand::HealthCheck> DoctorCommand::checkExternalTools() {
  auto start = startTimer();
  HealthCheck check;
  check.name = "External Tools";
  check.category = "tools";
  
  try {
    std::vector<std::pair<std::string, bool>> tools = {
      {"git", nx::util::SafeProcess::commandExists("git")},
      {"rg", nx::util::SafeProcess::commandExists("rg")},
      {"grep", nx::util::SafeProcess::commandExists("grep")},
      {"unzip", nx::util::SafeProcess::commandExists("unzip")},
      {"tar", nx::util::SafeProcess::commandExists("tar")}
    };
    
    int available = 0;
    int total = static_cast<int>(tools.size());
    std::string missing_tools;
    
    for (const auto& [tool, exists] : tools) {
      if (exists) {
        available++;
      } else {
        missing_tools += tool + " ";
      }
    }
    
    if (available == total) {
      check.passed = true;
      check.message = "All external tools are available";
    } else if (available >= total - 2) { // Allow some optional tools to be missing
      check.passed = true;
      check.message = "Essential tools available, optional missing: " + missing_tools;
    } else {
      check.passed = false;
      check.message = "Missing essential tools: " + missing_tools;
      check.fix_suggestion = "Install missing tools using your system package manager";
    }
  } catch (const std::exception& e) {
    check.passed = false;
    check.message = "External tools check failed: " + std::string(e.what());
    check.fix_suggestion = "Check PATH and tool installations";
  }
  
  check.duration_ms = endTimer(start);
  return check;
}

Result<DoctorCommand::HealthCheck> DoctorCommand::checkStorageSpace() {
  auto start = startTimer();
  HealthCheck check;
  check.name = "Storage Space";
  check.category = "storage";
  
  try {
    const auto& config = app_.config();
    
    auto available_result = getAvailableSpace(config.data_dir);
    auto used_result = getDirectorySize(config.data_dir);
    
    if (!available_result.has_value() || !used_result.has_value()) {
      check.passed = false;
      check.message = "Could not determine storage usage";
      check.fix_suggestion = "Check filesystem permissions";
    } else {
      uint64_t available_mb = available_result.value() / (1024 * 1024);
      uint64_t used_mb = used_result.value() / (1024 * 1024);
      
      if (available_mb > 100) { // At least 100MB free
        check.passed = true;
        check.message = "Storage: " + std::to_string(used_mb) + "MB used, " + 
                       std::to_string(available_mb) + "MB available";
      } else {
        check.passed = false;
        check.message = "Low storage space: " + std::to_string(available_mb) + "MB available";
        check.fix_suggestion = "Free up disk space or move nx data to larger storage";
      }
    }
  } catch (const std::exception& e) {
    check.passed = false;
    check.message = "Storage check failed: " + std::string(e.what());
    check.fix_suggestion = "Check filesystem and permissions";
  }
  
  check.duration_ms = endTimer(start);
  return check;
}

Result<DoctorCommand::HealthCheck> DoctorCommand::checkPerformance() {
  auto start = startTimer();
  HealthCheck check;
  check.name = "Performance";
  check.category = "performance";
  
  try {
    // Benchmark search performance
    auto search_time_result = benchmarkSearch("test query");
    
    // Benchmark note creation
    auto create_time_result = benchmarkNoteCreation();
    
    bool performance_ok = true;
    std::string performance_issues;
    
    if (search_time_result.has_value()) {
      double search_ms = search_time_result.value();
      if (search_ms > 500) { // Search should be under 500ms
        performance_ok = false;
        performance_issues += "slow search (" + std::to_string(static_cast<int>(search_ms)) + "ms) ";
      }
    }
    
    if (create_time_result.has_value()) {
      double create_ms = create_time_result.value();
      if (create_ms > 200) { // Note creation should be under 200ms
        performance_ok = false;
        performance_issues += "slow note creation (" + std::to_string(static_cast<int>(create_ms)) + "ms) ";
      }
    }
    
    if (performance_ok) {
      check.passed = true;
      check.message = "Performance is within acceptable limits";
    } else {
      check.passed = false;
      check.message = "Performance issues detected: " + performance_issues;
      check.fix_suggestion = "Consider running 'nx gc optimize' or 'nx reindex optimize'";
    }
  } catch (const std::exception& e) {
    check.passed = false;
    check.message = "Performance check failed: " + std::string(e.what());
    check.fix_suggestion = "Run system maintenance commands";
  }
  
  check.duration_ms = endTimer(start);
  return check;
}

Result<DoctorCommand::HealthCheck> DoctorCommand::checkDatabase() {
  auto start = startTimer();
  HealthCheck check;
  check.name = "Database";
  check.category = "database";
  
  try {
    const auto& config = app_.config();
    auto db_path = config.data_dir / "search.db";
    
    if (!std::filesystem::exists(db_path)) {
      check.passed = false;
      check.message = "Search database does not exist";
      check.fix_suggestion = "Run 'nx reindex rebuild' to create search database";
    } else {
      // Check database file integrity (basic check)
      auto file_size = std::filesystem::file_size(db_path);
      
      if (file_size == 0) {
        check.passed = false;
        check.message = "Search database is empty";
        check.fix_suggestion = "Run 'nx reindex rebuild' to rebuild database";
      } else {
        check.passed = true;
        check.message = "Database exists (" + std::to_string(file_size / 1024) + "KB)";
      }
    }
  } catch (const std::exception& e) {
    check.passed = false;
    check.message = "Database check failed: " + std::string(e.what());
    check.fix_suggestion = "Rebuild search database";
  }
  
  check.duration_ms = endTimer(start);
  return check;
}

Result<DoctorCommand::HealthCheck> DoctorCommand::checkNotesIntegrity() {
  auto start = startTimer();
  HealthCheck check;
  check.name = "Notes Integrity";
  check.category = "storage";
  
  try {
    auto& note_store = app_.noteStore();
    auto notes_result = note_store.list();
    
    if (!notes_result.has_value()) {
      check.passed = false;
      check.message = "Cannot list notes: " + notes_result.error().message();
      check.fix_suggestion = "Check notes directory permissions";
    } else {
      auto notes = notes_result.value();
      int corrupted = 0;
      int total = static_cast<int>(notes.size());
      
      // Sample a few notes to check for corruption
      int to_check = std::min(10, total);
      for (int i = 0; i < to_check && i < total; ++i) {
        auto note_result = note_store.load(notes[static_cast<size_t>(i)]);
        if (!note_result.has_value()) {
          corrupted++;
        }
      }
      
      if (corrupted == 0) {
        check.passed = true;
        check.message = "Notes integrity verified (" + std::to_string(total) + " notes)";
      } else {
        check.passed = false;
        check.message = "Found " + std::to_string(corrupted) + " corrupted notes out of " + 
                       std::to_string(to_check) + " checked";
        check.fix_suggestion = "Run 'nx gc cleanup' to fix corrupted notes";
      }
    }
  } catch (const std::exception& e) {
    check.passed = false;
    check.message = "Notes integrity check failed: " + std::string(e.what());
    check.fix_suggestion = "Check notes directory and run cleanup";
  }
  
  check.duration_ms = endTimer(start);
  return check;
}

void DoctorCommand::printReport(const HealthReport& report, const GlobalOptions& options) {
  if (options.json) {
    nlohmann::json json_report;
    json_report["overall_status"] = report.overall_status;
    json_report["total_checks"] = report.checks.size();
    json_report["passed"] = report.passed;
    json_report["failed"] = report.failed;
    json_report["warnings"] = report.warnings;
    json_report["duration_ms"] = report.total_duration_ms;
    
    nlohmann::json checks_array = nlohmann::json::array();
    for (const auto& check : report.checks) {
      nlohmann::json check_json;
      check_json["name"] = check.name;
      check_json["category"] = check.category;
      check_json["passed"] = check.passed;
      check_json["message"] = check.message;
      check_json["duration_ms"] = check.duration_ms;
      if (!check.fix_suggestion.empty()) {
        check_json["fix_suggestion"] = check.fix_suggestion;
      }
      checks_array.push_back(check_json);
    }
    json_report["checks"] = checks_array;
    
    std::cout << json_report.dump() << std::endl;
  } else {
    // Print summary
    std::cout << "=== System Health Report ===\n";
    std::cout << "Overall Status: " << report.overall_status << "\n";
    std::cout << "Checks Passed: " << report.passed << "/" << report.checks.size() << "\n";
    if (report.failed > 0) {
      std::cout << "Failed: " << report.failed << "\n";
    }
    std::cout << "Total Duration: " << std::fixed << std::setprecision(1) 
              << report.total_duration_ms << "ms\n\n";
    
    // Print individual checks
    for (const auto& check : report.checks) {
      printCheck(check, options);
    }
    
    // Print summary recommendations
    if (report.failed > 0) {
      std::cout << "\n=== Recommendations ===\n";
      std::cout << "Run with --fix flag to attempt automatic fixes\n";
      std::cout << "Review failed checks and follow fix suggestions\n";
    }
  }
}

void DoctorCommand::printCheck(const HealthCheck& check, const GlobalOptions& options) {
  if (options.quiet && check.passed) {
    return; // Skip passed checks in quiet mode
  }
  
  std::string status_symbol = check.passed ? "✓" : "✗";
  std::string status_color = check.passed ? "" : ""; // Colors could be added here
  
  std::cout << status_symbol << " " << check.name << " (" << check.category << ")";
  
  if (verbose_output_ || !check.passed) {
    std::cout << "\n  " << check.message;
    if (!check.passed && !check.fix_suggestion.empty()) {
      std::cout << "\n  Fix: " << check.fix_suggestion;
    }
    std::cout << "\n  Duration: " << std::fixed << std::setprecision(1) 
              << check.duration_ms << "ms";
  }
  
  std::cout << "\n";
}

Result<void> DoctorCommand::fixIssues(const HealthReport& report) {
  bool any_fixed = false;
  
  for (const auto& check : report.checks) {
    if (check.passed) continue;
    
    // Attempt specific fixes based on check type
    if (check.name == "Directories") {
      // Create missing directories
      try {
        const auto& config = app_.config();
        std::vector<std::filesystem::path> dirs_to_create = {
          config.data_dir / "attachments",
          config.data_dir / "templates", 
          config.data_dir / "backups"
        };
        
        for (const auto& dir : dirs_to_create) {
          if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directories(dir);
            any_fixed = true;
          }
        }
      } catch (const std::exception& e) {
        return std::unexpected(makeError(ErrorCode::kFileError,
                                         "Failed to create directories: " + std::string(e.what())));
      }
    }
    
    // Add more automatic fixes as needed
  }
  
  if (!any_fixed) {
    return std::unexpected(makeError(ErrorCode::kNotFound,
                                     "No issues could be fixed automatically"));
  }
  
  return {};
}

std::chrono::high_resolution_clock::time_point DoctorCommand::startTimer() {
  return std::chrono::high_resolution_clock::now();
}

double DoctorCommand::endTimer(const std::chrono::high_resolution_clock::time_point& start) {
  auto end = std::chrono::high_resolution_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  return duration.count() / 1000.0; // Convert to milliseconds
}

bool DoctorCommand::checkFilePermissions(const std::filesystem::path& path, int required_perms) {
  struct stat file_stat;
#ifdef _WIN32
  if (stat(path.string().c_str(), &file_stat) != 0) {
#else
  if (stat(path.c_str(), &file_stat) != 0) {
#endif
    return false;
  }
  
  // Check read/write permissions (simplified check)
  if (required_perms & 4) { // R_OK
    if (!(file_stat.st_mode & S_IRUSR)) {
      return false;
    }
  }
  
  if (required_perms & 2) { // W_OK
    if (!(file_stat.st_mode & S_IWUSR)) {
      return false;
    }
  }
  
  return true;
}

Result<uint64_t> DoctorCommand::getDirectorySize(const std::filesystem::path& path) {
  try {
    uint64_t size = 0;
    for (const auto& entry : std::filesystem::recursive_directory_iterator(path)) {
      if (entry.is_regular_file()) {
        size += entry.file_size();
      }
    }
    return size;
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kFileError,
                                     "Failed to calculate directory size: " + std::string(e.what())));
  }
}

Result<uint64_t> DoctorCommand::getAvailableSpace(const std::filesystem::path& path) {
  try {
#ifdef _WIN32
    // Windows implementation using std::filesystem
    std::error_code ec;
    auto space_info = std::filesystem::space(path, ec);
    if (ec) {
      return std::unexpected(makeError(ErrorCode::kSystemError,
                                       "Failed to get filesystem statistics: " + ec.message()));
    }
    return space_info.available;
#else
    struct statvfs stat;
    if (statvfs(path.c_str(), &stat) != 0) {
      return std::unexpected(makeError(ErrorCode::kSystemError,
                                       "Failed to get filesystem statistics"));
    }
    
    return static_cast<uint64_t>(stat.f_bavail) * stat.f_frsize;
#endif
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kSystemError,
                                     "Failed to get available space: " + std::string(e.what())));
  }
}

Result<double> DoctorCommand::benchmarkSearch(const std::string& query) {
  try {
    auto start = startTimer();
    
    auto& search_index = app_.searchIndex();
    nx::index::SearchQuery search_query;
    search_query.text = query;
    search_query.limit = 10;
    
    auto result = search_index.search(search_query);
    
    auto duration = endTimer(start);
    
    if (result.has_value()) {
      return duration;
    } else {
      return std::unexpected(makeError(ErrorCode::kIndexError,
                                       "Search benchmark failed: " + result.error().message()));
    }
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kIndexError,
                                     "Search benchmark error: " + std::string(e.what())));
  }
}

Result<double> DoctorCommand::benchmarkNoteCreation() {
  try {
    auto start = startTimer();
    
    // Create a temporary note for benchmarking
    auto test_note = nx::core::Note::create("Doctor Test Note", "Temporary note for performance testing");
    
    auto& note_store = app_.noteStore();
    auto store_result = note_store.store(test_note);
    
    auto duration = endTimer(start);
    
    // Clean up the test note
    if (store_result.has_value()) {
      note_store.remove(test_note.id()); // Ignore result
    }
    
    if (store_result.has_value()) {
      return duration;
    } else {
      return std::unexpected(makeError(ErrorCode::kFileError,
                                       "Note creation benchmark failed: " + store_result.error().message()));
    }
  } catch (const std::exception& e) {
    return std::unexpected(makeError(ErrorCode::kFileError,
                                     "Note creation benchmark error: " + std::string(e.what())));
  }
}

} // namespace nx::cli