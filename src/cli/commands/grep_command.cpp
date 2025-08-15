#include "nx/cli/commands/grep_command.hpp"

#include <iostream>
#include <iomanip>
#include <nlohmann/json.hpp>
#include "nx/index/index.hpp"

namespace nx::cli {

GrepCommand::GrepCommand(Application& app) : app_(app) {
}

Result<int> GrepCommand::execute(const GlobalOptions& options) {
  try {
    // Create search query
    nx::index::SearchQuery search_query;
    search_query.text = query_;
    search_query.limit = 50;  // Default limit
    search_query.highlight = true;

    // Note: The ignore_case and use_regex flags would ideally be passed to the search implementation
    // Perform the search with the basic query

    // Perform the search
    auto search_result = app_.searchIndex().search(search_query);
    if (!search_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": ")" << search_result.error().message() << R"(", "query": ")" << query_ << R"("})" << std::endl;
      } else {
        std::cout << "Error: " << search_result.error().message() << std::endl;
      }
      return 1;
    }

    const auto& results = *search_result;

    if (options.json) {
      nlohmann::json json_results = nlohmann::json::array();
      
      for (const auto& result : results) {
        nlohmann::json json_result;
        json_result["id"] = result.id.toString();
        json_result["title"] = result.title;
        json_result["snippet"] = result.snippet;
        json_result["score"] = result.score;
        json_result["modified"] = std::chrono::duration_cast<std::chrono::seconds>(
          result.modified.time_since_epoch()).count();
        json_result["tags"] = result.tags;
        if (result.notebook.has_value()) {
          json_result["notebook"] = *result.notebook;
        } else {
          json_result["notebook"] = nullptr;
        }
        json_results.push_back(json_result);
      }
      
      nlohmann::json output;
      output["query"] = query_;
      output["total_results"] = results.size();
      output["results"] = json_results;
      output["use_regex"] = use_regex_;
      output["ignore_case"] = ignore_case_;
      
      std::cout << output.dump() << std::endl;
    } else {
      if (results.empty()) {
        std::cout << "No results found for query: " << query_ << std::endl;
        return 0;
      }

      if (!options.quiet) {
        std::cout << "Found " << results.size() << " result(s) for query: " << query_ << std::endl;
        std::cout << std::string(50, '-') << std::endl;
      }

      for (const auto& result : results) {
        std::cout << result.id.toString() << " | " << result.title;
        if (result.notebook.has_value()) {
          std::cout << " [" << *result.notebook << "]";
        }
        std::cout << " (score: " << std::fixed << std::setprecision(2) << result.score << ")" << std::endl;
        
        if (!result.snippet.empty()) {
          std::cout << "  " << result.snippet << std::endl;
        }
        
        if (!result.tags.empty()) {
          std::cout << "  Tags: ";
          for (size_t i = 0; i < result.tags.size(); ++i) {
            if (i > 0) std::cout << ", ";
            std::cout << result.tags[i];
          }
          std::cout << std::endl;
        }
        
        std::cout << std::endl;
      }
    }

    return 0;

  } catch (const std::exception& e) {
    if (options.json) {
      std::cout << R"({"error": ")" << e.what() << R"(", "query": ")" << query_ << R"("})" << std::endl;
    } else {
      std::cout << "Error: " << e.what() << std::endl;
    }
    return 1;
  }
}

void GrepCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("query", query_, "Search query/pattern")->required();
  cmd->add_flag("--regex,-r", use_regex_, "Treat query as regex pattern");
  cmd->add_flag("--ignore-case,-i", ignore_case_, "Case insensitive search");
}

} // namespace nx::cli