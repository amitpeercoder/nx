#pragma once

#include "nx/cli/application.hpp"

namespace nx::cli {

/**
 * @brief Command for system health checks and diagnostics
 * 
 * The doctor command performs comprehensive health checks on the nx system:
 * - Configuration validation
 * - Directory permissions and accessibility
 * - Search index integrity
 * - Git repository status (if enabled)
 * - External tool availability
 * - Performance benchmarks
 * - Storage usage analysis
 */
class DoctorCommand : public Command {
public:
  explicit DoctorCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  void setupCommand(CLI::App* cmd) override;
  
  std::string name() const override { return "doctor"; }
  std::string description() const override { 
    return "Run comprehensive system health checks and diagnostics"; 
  }

private:
  Application& app_;
  
  // Command options
  bool fix_issues_ = false;
  bool verbose_output_ = false;
  bool quick_check_ = false;
  std::string check_category_;
  
  // Health check implementations
  struct HealthCheck {
    std::string name;
    std::string category;
    bool passed = false;
    std::string message;
    std::string fix_suggestion;
    double duration_ms = 0.0;
  };
  
  struct HealthReport {
    std::vector<HealthCheck> checks;
    int passed = 0;
    int failed = 0;
    int warnings = 0;
    double total_duration_ms = 0.0;
    std::string overall_status;
  };
  
  // Check categories
  Result<HealthCheck> checkConfiguration();
  Result<HealthCheck> checkDirectories();
  Result<HealthCheck> checkPermissions();
  Result<HealthCheck> checkSearchIndex();
  Result<HealthCheck> checkGitRepository();
  Result<HealthCheck> checkExternalTools();
  Result<HealthCheck> checkStorageSpace();
  Result<HealthCheck> checkPerformance();
  Result<HealthCheck> checkDatabase();
  Result<HealthCheck> checkNotesIntegrity();
  
  // Utility functions
  Result<void> runAllChecks(HealthReport& report);
  Result<void> runCategoryChecks(const std::string& category, HealthReport& report);
  void printReport(const HealthReport& report, const GlobalOptions& options);
  void printCheck(const HealthCheck& check, const GlobalOptions& options);
  Result<void> fixIssues(const HealthReport& report);
  
  // Performance utilities
  std::chrono::high_resolution_clock::time_point startTimer();
  double endTimer(const std::chrono::high_resolution_clock::time_point& start);
  
  // Helper functions
  bool checkFilePermissions(const std::filesystem::path& path, int required_perms);
  Result<uint64_t> getDirectorySize(const std::filesystem::path& path);
  Result<uint64_t> getAvailableSpace(const std::filesystem::path& path);
  Result<double> benchmarkSearch(const std::string& query);
  Result<double> benchmarkNoteCreation();
};

} // namespace nx::cli