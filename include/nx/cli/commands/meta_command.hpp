#pragma once

#include <string>
#include <vector>
#include <map>
#include <CLI/CLI.hpp>
#include "nx/cli/application.hpp"

namespace nx::cli {

class MetaCommand : public Command {
public:
  explicit MetaCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "meta"; }
  std::string description() const override { return "Metadata management"; }
  void setupCommand(CLI::App* cmd) override;

private:
  Application& app_;
  
  // Subcommand selection
  std::string subcommand_;
  
  // Common options
  std::string note_id_;
  std::string key_;
  std::string value_;
  std::vector<std::string> key_value_pairs_;
  bool list_all_ = false;
  
  // Subcommand implementations
  Result<int> executeGet();
  Result<int> executeSet();
  Result<int> executeList();
  Result<int> executeRemove();
  Result<int> executeClear();
  Result<int> executeUpdate();
  
  // Helper methods
  void outputMetadata(const nx::core::Metadata& metadata, const GlobalOptions& options);
  void outputKeyValue(const std::string& key, const std::string& value, const GlobalOptions& options);
  std::map<std::string, std::string> parseKeyValuePairs(const std::vector<std::string>& pairs);
  Result<nx::core::NoteId> resolveNoteId(const std::string& partial_id);
  
  // Metadata field helpers
  Result<std::string> getMetadataField(const nx::core::Metadata& metadata, const std::string& key);
  Result<void> setMetadataField(nx::core::Metadata& metadata, const std::string& key, const std::string& value);
  Result<void> setNoteTitle(nx::core::Note& note, const std::string& new_title);
  std::vector<std::pair<std::string, std::string>> getAllMetadataFields(const nx::core::Metadata& metadata);
};

} // namespace nx::cli