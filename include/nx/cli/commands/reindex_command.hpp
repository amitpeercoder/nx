#pragma once

#include "nx/cli/application.hpp"

namespace nx::cli {

class ReindexCommand : public Command {
public:
  explicit ReindexCommand(Application& app);

  std::string name() const override { return "reindex"; }
  std::string description() const override { return "Rebuild and optimize search index"; }

  Result<int> execute(const GlobalOptions& options) override;
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  
  // Command options
  bool force_ = false;
  bool optimize_only_ = false;
  bool validate_only_ = false;
  bool stats_only_ = false;
  std::string index_type_;
  
  // Subcommand implementations
  Result<int> executeRebuild();
  Result<int> executeOptimize();
  Result<int> executeValidate();
  Result<int> executeStats();
  
  // Helper methods
  void outputIndexStats(const nx::index::IndexStats& stats, const GlobalOptions& options);
  void outputProgress(const std::string& message, const GlobalOptions& options);
};

} // namespace nx::cli