#pragma once

#include <string>
#include <vector>

#include <CLI/CLI.hpp>
#include <nlohmann/json.hpp>
#include "nx/cli/application.hpp"

namespace nx::cli {

/**
 * @brief Generate outlines for topics
 * Usage: nx outline <topic> [--depth N] [--style bullets|numbered|tree] [--create] [--title "Custom Title"]
 */
class OutlineCommand : public Command {
public:
  explicit OutlineCommand(Application& app);
  
  Result<int> execute(const GlobalOptions& options) override;
  std::string name() const override { return "outline"; }
  std::string description() const override { return "Generate hierarchical outlines for topics using AI"; }
  
  void setupCommand(CLI::App* cmd) override;

private:
  // Outline node structure
  struct OutlineNode {
    std::string text;
    int level;                    // 1-based depth level
    std::vector<OutlineNode> children;
  };
  
  Application& app_;
  
  // Command options
  std::string topic_;
  int depth_ = 3;               // Maximum outline depth
  std::string style_ = "bullets"; // bullets, numbered, tree
  bool create_note_ = false;    // Create a note with the outline
  std::string custom_title_;    // Custom title for created note
  
  // Helper methods
  Result<void> validateAiConfig();
  Result<std::vector<OutlineNode>> generateOutline(const std::string& topic);
  std::vector<OutlineNode> parseOutlineFromJson(const nlohmann::json& outline_json);
  std::vector<OutlineNode> parseOutlineFromText(const std::string& text);
  Result<nx::core::NoteId> createNoteWithOutline(const std::vector<OutlineNode>& outline);
  std::string formatOutline(const std::vector<OutlineNode>& outline, const std::string& style);
  std::string formatOutlineNode(const OutlineNode& node, const std::string& style, const std::string& prefix = "");
  void outputResult(const std::vector<OutlineNode>& outline, const std::optional<nx::core::NoteId>& created_note_id, const GlobalOptions& options);
};

} // namespace nx::cli