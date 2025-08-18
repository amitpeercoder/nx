#pragma once

#include "nx/cli/application.hpp"
#include "nx/common.hpp"

namespace nx::cli {

/**
 * @brief Command for exporting notes to various formats
 */
class ExportCommand : public Command {
public:
  explicit ExportCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "export"; }
  std::string description() const override { return "Export notes to various formats"; }
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  std::string format_ = "markdown";
  std::string output_path_;
  std::vector<std::string> tag_filter_;
  std::string notebook_filter_;
  std::string date_filter_;
  bool include_metadata_ = true;
  bool preserve_structure_ = true;
  bool include_attachments_ = false;
  bool compress_ = false;
  std::string template_file_;
};

} // namespace nx::cli