#include "nx/cli/commands/sync_command.hpp"

#include <iostream>
#include <nlohmann/json.hpp>

#include "nx/sync/git_sync.hpp"

namespace nx::cli {

SyncCommand::SyncCommand(Application& app) : app_(app) {}

void SyncCommand::setupCommand(CLI::App* cmd) {
  cmd->add_option("operation", operation_, "Sync operation: status, init, clone, pull, push, sync, resolve")
     ->check(CLI::IsMember({"status", "init", "clone", "pull", "push", "sync", "resolve"}));
  
  cmd->add_option("--remote,-r", remote_url_, "Remote repository URL");
  cmd->add_option("--branch,-b", branch_, "Branch name (default: main)");
  cmd->add_option("--message,-m", message_, "Commit message for sync");
  cmd->add_flag("--force,-f", force_, "Force push (use with caution)");
  cmd->add_flag("--no-auto-resolve", auto_resolve_, "Disable automatic conflict resolution");
  cmd->add_option("--strategy,-s", strategy_, "Merge strategy: merge, rebase, fast-forward")
     ->check(CLI::IsMember({"merge", "rebase", "fast-forward"}));
  cmd->add_option("--resolve-strategy", resolve_strategy_, "Conflict resolution strategy: ours, theirs, manual")
     ->check(CLI::IsMember({"ours", "theirs", "manual"}));
  cmd->add_option("--files", resolve_files_, "Specific files to resolve (for resolve operation)");
  cmd->add_option("--user-name", user_name_, "Git user name");
  cmd->add_option("--user-email", user_email_, "Git user email");
}

Result<int> SyncCommand::execute(const GlobalOptions& options) {
  try {
    // Check if git is available
    if (!nx::sync::GitSync::isAvailable()) {
      if (options.json) {
        std::cout << R"({"error": "Git is not available", "success": false})" << std::endl;
      } else {
        std::cout << "Error: Git is not available. Please install git." << std::endl;
      }
      return 1;
    }

    auto notes_dir = app_.config().notes_dir;
    
    // Set up git configuration
    nx::sync::GitConfig git_config;
    git_config.remote_url = remote_url_;
    git_config.branch = branch_;
    git_config.user_name = user_name_.empty() ? app_.config().git_user_name : user_name_;
    git_config.user_email = user_email_.empty() ? app_.config().git_user_email : user_email_;

    if (operation_ == "init") {
      // Initialize new repository
      auto sync_result = nx::sync::GitSync::initializeRepository(notes_dir, git_config);
      if (!sync_result.has_value()) {
        if (options.json) {
          std::cout << R"({"error": ")" << sync_result.error().message() << R"(", "success": false})" << std::endl;
        } else {
          std::cout << "Error: " << sync_result.error().message() << std::endl;
        }
        return 1;
      }

      if (options.json) {
        nlohmann::json result;
        result["success"] = true;
        result["operation"] = "init";
        result["repository"] = notes_dir.string();
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "Initialized git repository in: " << notes_dir.string() << std::endl;
        if (!remote_url_.empty()) {
          std::cout << "Added remote: " << remote_url_ << std::endl;
        }
      }
      
      return 0;
    }

    if (operation_ == "clone") {
      if (remote_url_.empty()) {
        if (options.json) {
          std::cout << R"({"error": "Remote URL required for clone operation", "success": false})" << std::endl;
        } else {
          std::cout << "Error: Remote URL required for clone operation" << std::endl;
        }
        return 1;
      }

      auto sync_result = nx::sync::GitSync::cloneRepository(remote_url_, notes_dir, git_config);
      if (!sync_result.has_value()) {
        if (options.json) {
          std::cout << R"({"error": ")" << sync_result.error().message() << R"(", "success": false})" << std::endl;
        } else {
          std::cout << "Error: " << sync_result.error().message() << std::endl;
        }
        return 1;
      }

      if (options.json) {
        nlohmann::json result;
        result["success"] = true;
        result["operation"] = "clone";
        result["remote_url"] = remote_url_;
        result["local_path"] = notes_dir.string();
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "Cloned repository to: " << notes_dir.string() << std::endl;
      }
      
      return 0;
    }

    // For other operations, we need an existing repository
    auto sync_result = nx::sync::GitSync::initialize(notes_dir, git_config);
    if (!sync_result.has_value()) {
      if (options.json) {
        std::cout << R"({"error": ")" << sync_result.error().message() << R"(", "success": false})" << std::endl;
      } else {
        std::cout << "Error: " << sync_result.error().message() << std::endl;
        std::cout << "Hint: Initialize repository with: nx sync init" << std::endl;
      }
      return 1;
    }

    auto& sync = sync_result.value();

    if (operation_ == "status") {
      auto status_result = sync.getStatus();
      if (!status_result.has_value()) {
        if (options.json) {
          std::cout << R"({"error": ")" << status_result.error().message() << R"(", "success": false})" << std::endl;
        } else {
          std::cout << "Error: " << status_result.error().message() << std::endl;
        }
        return 1;
      }

      auto& status = status_result.value();

      if (options.json) {
        nlohmann::json result;
        result["success"] = true;
        result["status"] = [&]() {
          switch (status.status) {
            case nx::sync::SyncStatus::Clean: return "clean";
            case nx::sync::SyncStatus::Modified: return "modified";
            case nx::sync::SyncStatus::Ahead: return "ahead";
            case nx::sync::SyncStatus::Behind: return "behind";
            case nx::sync::SyncStatus::Diverged: return "diverged";
            case nx::sync::SyncStatus::Conflict: return "conflict";
            default: return "unknown";
          }
        }();
        result["current_branch"] = status.current_branch;
        result["commits_ahead"] = status.commits_ahead;
        result["commits_behind"] = status.commits_behind;
        result["modified_files"] = status.modified_files;
        result["untracked_files"] = status.untracked_files;
        result["last_commit_hash"] = status.last_commit_hash;
        result["last_commit_message"] = status.last_commit_message;
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "Repository Status:" << std::endl;
        std::cout << "  Branch: " << status.current_branch << std::endl;
        std::cout << "  Status: ";
        switch (status.status) {
          case nx::sync::SyncStatus::Clean:
            std::cout << "Clean (no changes)" << std::endl;
            break;
          case nx::sync::SyncStatus::Modified:
            std::cout << "Modified (local changes)" << std::endl;
            break;
          case nx::sync::SyncStatus::Ahead:
            std::cout << "Ahead (" << status.commits_ahead << " commits)" << std::endl;
            break;
          case nx::sync::SyncStatus::Behind:
            std::cout << "Behind (" << status.commits_behind << " commits)" << std::endl;
            break;
          case nx::sync::SyncStatus::Diverged:
            std::cout << "Diverged (ahead " << status.commits_ahead 
                      << ", behind " << status.commits_behind << ")" << std::endl;
            break;
          case nx::sync::SyncStatus::Conflict:
            std::cout << "Conflicts present" << std::endl;
            break;
        }

        if (!status.modified_files.empty()) {
          std::cout << "  Modified files (" << status.modified_files.size() << "):" << std::endl;
          for (const auto& file : status.modified_files) {
            std::cout << "    " << file << std::endl;
          }
        }

        if (!status.untracked_files.empty()) {
          std::cout << "  Untracked files (" << status.untracked_files.size() << "):" << std::endl;
          for (const auto& file : status.untracked_files) {
            std::cout << "    " << file << std::endl;
          }
        }

        if (!status.last_commit_hash.empty()) {
          std::cout << "  Last commit: " << status.last_commit_hash.substr(0, 8) 
                    << " " << status.last_commit_message << std::endl;
        }
      }

      return 0;
    }

    if (operation_ == "pull") {
      auto pull_result = sync.pull(strategy_);
      if (!pull_result.has_value()) {
        if (options.json) {
          std::cout << R"({"error": ")" << pull_result.error().message() << R"(", "success": false})" << std::endl;
        } else {
          std::cout << "Error: " << pull_result.error().message() << std::endl;
        }
        return 1;
      }

      if (options.json) {
        nlohmann::json result;
        result["success"] = true;
        result["operation"] = "pull";
        result["strategy"] = strategy_;
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "Pulled changes successfully" << std::endl;
      }

      return 0;
    }

    if (operation_ == "push") {
      auto push_result = sync.push(force_);
      if (!push_result.has_value()) {
        if (options.json) {
          std::cout << R"({"error": ")" << push_result.error().message() << R"(", "success": false})" << std::endl;
        } else {
          std::cout << "Error: " << push_result.error().message() << std::endl;
        }
        return 1;
      }

      if (options.json) {
        nlohmann::json result;
        result["success"] = true;
        result["operation"] = "push";
        result["force"] = force_;
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "Pushed changes successfully" << std::endl;
      }

      return 0;
    }

    if (operation_ == "sync") {
      auto sync_op_result = sync.sync(auto_resolve_);
      if (!sync_op_result.has_value()) {
        if (options.json) {
          std::cout << R"({"error": ")" << sync_op_result.error().message() << R"(", "success": false})" << std::endl;
        } else {
          std::cout << "Error: " << sync_op_result.error().message() << std::endl;
        }
        return 1;
      }

      auto& final_status = sync_op_result.value();

      if (options.json) {
        nlohmann::json result;
        result["success"] = true;
        result["operation"] = "sync";
        result["final_status"] = [&]() {
          switch (final_status.status) {
            case nx::sync::SyncStatus::Clean: return "clean";
            case nx::sync::SyncStatus::Modified: return "modified";
            case nx::sync::SyncStatus::Ahead: return "ahead";
            case nx::sync::SyncStatus::Behind: return "behind";
            case nx::sync::SyncStatus::Diverged: return "diverged";
            case nx::sync::SyncStatus::Conflict: return "conflict";
            default: return "unknown";
          }
        }();
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "Sync completed successfully" << std::endl;
        std::cout << "Final status: ";
        switch (final_status.status) {
          case nx::sync::SyncStatus::Clean:
            std::cout << "Clean" << std::endl;
            break;
          case nx::sync::SyncStatus::Modified:
            std::cout << "Modified" << std::endl;
            break;
          default:
            std::cout << "Other" << std::endl;
            break;
        }
      }

      return 0;
    }

    if (operation_ == "resolve") {
      // First check if there are conflicts to resolve
      auto status_result = sync.getStatus();
      if (!status_result.has_value()) {
        if (options.json) {
          std::cout << R"({"error": ")" << status_result.error().message() << R"(", "success": false})" << std::endl;
        } else {
          std::cout << "Error: " << status_result.error().message() << std::endl;
        }
        return 1;
      }

      auto& status = status_result.value();
      
      // Get list of conflicted files if no specific files provided
      std::vector<std::string> files_to_resolve = resolve_files_;
      if (files_to_resolve.empty()) {
        // Find conflicted files by checking git status
        for (const auto& file : status.modified_files) {
          // Simple check - in real implementation would check for conflict markers
          files_to_resolve.push_back(file);
        }
      }
      
      if (files_to_resolve.empty()) {
        if (options.json) {
          nlohmann::json result;
          result["success"] = true;
          result["operation"] = "resolve";
          result["message"] = "No conflicts to resolve";
          std::cout << result.dump(2) << std::endl;
        } else {
          std::cout << "No conflicts to resolve." << std::endl;
        }
        return 0;
      }

      // Resolve conflicts using the specified strategy
      auto resolve_result = sync.resolveConflicts(files_to_resolve, resolve_strategy_);
      if (!resolve_result.has_value()) {
        if (options.json) {
          std::cout << R"({"error": ")" << resolve_result.error().message() << R"(", "success": false})" << std::endl;
        } else {
          std::cout << "Error: " << resolve_result.error().message() << std::endl;
        }
        return 1;
      }

      if (options.json) {
        nlohmann::json result;
        result["success"] = true;
        result["operation"] = "resolve";
        result["strategy"] = resolve_strategy_;
        result["resolved_files"] = files_to_resolve;
        std::cout << result.dump(2) << std::endl;
      } else {
        std::cout << "Resolved " << files_to_resolve.size() << " file(s) using '" 
                  << resolve_strategy_ << "' strategy" << std::endl;
        for (const auto& file : files_to_resolve) {
          std::cout << "  " << file << std::endl;
        }
        std::cout << "\nNext steps:" << std::endl;
        std::cout << "  1. Verify resolution: nx sync status" << std::endl;
        std::cout << "  2. Commit changes: git commit -m \"Resolve conflicts\"" << std::endl;
      }

      return 0;
    }

    if (options.json) {
      std::cout << R"({"error": "Unknown operation", "success": false})" << std::endl;
    } else {
      std::cout << "Error: Unknown operation: " << operation_ << std::endl;
    }
    return 1;

  } catch (const std::exception& e) {
    if (options.json) {
      std::cout << R"({"error": ")" << e.what() << R"(", "success": false})" << std::endl;
    } else {
      std::cout << "Error: " << e.what() << std::endl;
    }
    return 1;
  }
}

} // namespace nx::cli