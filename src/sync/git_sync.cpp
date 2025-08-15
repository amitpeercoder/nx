#include "nx/sync/git_sync.hpp"

#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <cstdlib>

// Implementation uses git commands via SafeProcess for reliable cross-platform operation
#include "nx/util/safe_process.hpp"

namespace nx::sync {

GitSync::GitSync(std::filesystem::path repo_path, GitConfig config)
  : repo_path_(std::move(repo_path)), config_(std::move(config)) {}

Result<GitSync> GitSync::initialize(const std::filesystem::path& repo_path, const GitConfig& config) {
  if (!std::filesystem::exists(repo_path)) {
    return std::unexpected(makeError(ErrorCode::kDirectoryNotFound,
                                     "Repository path does not exist: " + repo_path.string()));
  }

  auto git_dir = repo_path / ".git";
  if (!std::filesystem::exists(git_dir)) {
    return std::unexpected(makeError(ErrorCode::kGitError,
                                     "Not a git repository: " + repo_path.string()));
  }

  if (!isAvailable()) {
    return std::unexpected(makeError(ErrorCode::kExternalToolError,
                                     "Git is not available. Please install git."));
  }

  GitSync sync(repo_path, config);
  
  // Set up git config if provided
  if (!config.user_name.empty()) {
    auto name_result = nx::util::SafeProcess::execute("git", 
      {"config", "user.name", config.user_name}, repo_path.string());
    if (!name_result.has_value() || !name_result->success()) {
      return std::unexpected(makeError(ErrorCode::kGitError,
                                       "Failed to set git user name"));
    }
  }

  if (!config.user_email.empty()) {
    auto email_result = nx::util::SafeProcess::execute("git", 
      {"config", "user.email", config.user_email}, repo_path.string());
    if (!email_result.has_value() || !email_result->success()) {
      return std::unexpected(makeError(ErrorCode::kGitError,
                                       "Failed to set git user email"));
    }
  }

  return sync;
}

Result<GitSync> GitSync::initializeRepository(const std::filesystem::path& repo_path, const GitConfig& config) {
  if (!isAvailable()) {
    return std::unexpected(makeError(ErrorCode::kExternalToolError,
                                     "Git is not available. Please install git."));
  }

  // Create directory if it doesn't exist
  if (!std::filesystem::exists(repo_path)) {
    std::error_code ec;
    std::filesystem::create_directories(repo_path, ec);
    if (ec) {
      return std::unexpected(makeError(ErrorCode::kDirectoryCreateError,
                                       "Failed to create repository directory: " + ec.message()));
    }
  }

  // Initialize git repository
  auto init_result = nx::util::SafeProcess::execute("git", {"init"}, repo_path.string());
  if (!init_result.has_value() || !init_result->success()) {
    return std::unexpected(makeError(ErrorCode::kGitError,
                                     "Failed to initialize git repository: " + 
                                     (init_result.has_value() ? init_result->stderr_output : "unknown error")));
  }

  // Set up initial configuration
  GitSync sync(repo_path, config);
  
  // Configure user
  if (!config.user_name.empty()) {
    auto name_result = nx::util::SafeProcess::execute("git", 
      {"config", "user.name", config.user_name}, repo_path.string());
    if (!name_result.has_value() || !name_result->success()) {
      return std::unexpected(makeError(ErrorCode::kGitError,
                                       "Failed to set git user name"));
    }
  }

  if (!config.user_email.empty()) {
    auto email_result = nx::util::SafeProcess::execute("git", 
      {"config", "user.email", config.user_email}, repo_path.string());
    if (!email_result.has_value() || !email_result->success()) {
      return std::unexpected(makeError(ErrorCode::kGitError,
                                       "Failed to set git user email"));
    }
  }

  // Set up remote if provided
  if (!config.remote_url.empty()) {
    auto remote_result = nx::util::SafeProcess::execute("git", 
      {"remote", "add", "origin", config.remote_url}, repo_path.string());
    if (!remote_result.has_value() || !remote_result->success()) {
      return std::unexpected(makeError(ErrorCode::kGitError,
                                       "Failed to add remote: " + 
                                       (remote_result.has_value() ? remote_result->stderr_output : "unknown error")));
    }
  }

  return sync;
}

Result<GitSync> GitSync::cloneRepository(const std::string& remote_url, 
                                        const std::filesystem::path& local_path,
                                        const GitConfig& config) {
  if (!isAvailable()) {
    return std::unexpected(makeError(ErrorCode::kExternalToolError,
                                     "Git is not available. Please install git."));
  }

  // Clone repository
  auto clone_result = nx::util::SafeProcess::execute("git", 
    {"clone", remote_url, local_path.string()});
  if (!clone_result.has_value() || !clone_result->success()) {
    return std::unexpected(makeError(ErrorCode::kGitError,
                                     "Failed to clone repository: " + 
                                     (clone_result.has_value() ? clone_result->stderr_output : "unknown error")));
  }

  // Create GitSync instance with the cloned repository
  GitConfig clone_config = config;
  clone_config.remote_url = remote_url;
  
  return initialize(local_path, clone_config);
}

Result<SyncInfo> GitSync::getStatus() const {
  SyncInfo info;
  
  // Get current branch
  auto branch_result = nx::util::SafeProcess::executeForOutput("git", 
    {"branch", "--show-current"}, repo_path_.string());
  if (branch_result.has_value()) {
    info.current_branch = branch_result.value();
    // Remove trailing newline
    if (!info.current_branch.empty() && info.current_branch.back() == '\n') {
      info.current_branch.pop_back();
    }
  }

  // Get status
  auto status_result = nx::util::SafeProcess::executeForOutput("git", 
    {"status", "--porcelain"}, repo_path_.string());
  if (!status_result.has_value()) {
    return std::unexpected(makeError(ErrorCode::kGitError,
                                     "Failed to get git status: " + status_result.error().message()));
  }

  // Parse status output
  std::istringstream status_stream(status_result.value());
  std::string line;
  while (std::getline(status_stream, line)) {
    if (line.length() >= 3) {
      char status_char = line[0];
      std::string file_path = line.substr(3);
      
      if (status_char == '?') {
        info.untracked_files.push_back(file_path);
      } else if (status_char != ' ') {
        info.modified_files.push_back(file_path);
      }
    }
  }

  // Get ahead/behind info
  auto ahead_behind_result = nx::util::SafeProcess::executeForOutput("git", 
    {"rev-list", "--left-right", "--count", "HEAD...@{upstream}"}, repo_path_.string());
  if (ahead_behind_result.has_value()) {
    std::istringstream ab_stream(ahead_behind_result.value());
    ab_stream >> info.commits_ahead >> info.commits_behind;
  }

  // Get last commit info
  auto commit_result = nx::util::SafeProcess::executeForOutput("git", 
    {"log", "-1", "--format=%H %s"}, repo_path_.string());
  if (commit_result.has_value()) {
    std::istringstream commit_stream(commit_result.value());
    commit_stream >> info.last_commit_hash;
    std::getline(commit_stream, info.last_commit_message);
    if (!info.last_commit_message.empty() && info.last_commit_message[0] == ' ') {
      info.last_commit_message = info.last_commit_message.substr(1);
    }
  }

  // Determine overall status
  if (!info.modified_files.empty() || !info.untracked_files.empty()) {
    info.status = SyncStatus::Modified;
  } else if (info.commits_ahead > 0 && info.commits_behind > 0) {
    info.status = SyncStatus::Diverged;
  } else if (info.commits_ahead > 0) {
    info.status = SyncStatus::Ahead;
  } else if (info.commits_behind > 0) {
    info.status = SyncStatus::Behind;
  } else {
    info.status = SyncStatus::Clean;
  }

  return info;
}

Result<void> GitSync::commit(const std::string& message, const std::vector<std::string>& files) {
  if (message.empty()) {
    return std::unexpected(makeError(ErrorCode::kInvalidArgument,
                                     "Commit message cannot be empty"));
  }

  // Stage files
  if (files.empty()) {
    // Stage all changes
    auto add_result = nx::util::SafeProcess::execute("git", 
      {"add", "."}, repo_path_.string());
    if (!add_result.has_value() || !add_result->success()) {
      return std::unexpected(makeError(ErrorCode::kGitError,
                                       "Failed to stage files: " + 
                                       (add_result.has_value() ? add_result->stderr_output : "unknown error")));
    }
  } else {
    // Stage specific files
    for (const auto& file : files) {
      auto add_result = nx::util::SafeProcess::execute("git", 
        {"add", file}, repo_path_.string());
      if (!add_result.has_value() || !add_result->success()) {
        return std::unexpected(makeError(ErrorCode::kGitError,
                                         "Failed to stage file " + file + ": " + 
                                         (add_result.has_value() ? add_result->stderr_output : "unknown error")));
      }
    }
  }

  // Create commit
  auto commit_result = nx::util::SafeProcess::execute("git", 
    {"commit", "-m", message}, repo_path_.string());
  if (!commit_result.has_value() || !commit_result->success()) {
    return std::unexpected(makeError(ErrorCode::kGitError,
                                     "Failed to create commit: " + 
                                     (commit_result.has_value() ? commit_result->stderr_output : "unknown error")));
  }

  return {};
}

Result<void> GitSync::pull(const std::string& strategy) {
  std::vector<std::string> args = {"pull"};
  
  if (strategy == "rebase") {
    args.push_back("--rebase");
  } else if (strategy == "fast-forward") {
    args.push_back("--ff-only");
  }
  // Default is merge

  auto pull_result = nx::util::SafeProcess::execute("git", args, repo_path_.string());
  if (!pull_result.has_value() || !pull_result->success()) {
    return std::unexpected(makeError(ErrorCode::kGitError,
                                     "Failed to pull: " + 
                                     (pull_result.has_value() ? pull_result->stderr_output : "unknown error")));
  }

  return {};
}

Result<void> GitSync::push(bool force) {
  std::vector<std::string> args = {"push"};
  
  if (force) {
    args.push_back("--force");
  }

  auto push_result = nx::util::SafeProcess::execute("git", args, repo_path_.string());
  if (!push_result.has_value() || !push_result->success()) {
    return std::unexpected(makeError(ErrorCode::kGitError,
                                     "Failed to push: " + 
                                     (push_result.has_value() ? push_result->stderr_output : "unknown error")));
  }

  return {};
}

Result<SyncInfo> GitSync::sync(bool auto_resolve) {
  // Get current status
  auto status_result = getStatus();
  if (!status_result.has_value()) {
    return std::unexpected(status_result.error());
  }

  auto status = status_result.value();

  // Commit local changes if any
  if (!status.modified_files.empty() || !status.untracked_files.empty()) {
    auto commit_result = commit("Auto-commit before sync");
    if (!commit_result.has_value()) {
      return std::unexpected(commit_result.error());
    }
  }

  // Pull changes
  auto pull_result = pull();
  if (!pull_result.has_value()) {
    return std::unexpected(pull_result.error());
  }

  // Push changes
  auto push_result = push();
  if (!push_result.has_value()) {
    return std::unexpected(push_result.error());
  }

  // Return updated status
  return getStatus();
}

Result<std::vector<std::string>> GitSync::getHistory(int limit, 
                                                     const std::optional<std::string>& since) const {
  std::vector<std::string> args = {"log", "--oneline"};
  
  if (limit > 0) {
    args.push_back("-" + std::to_string(limit));
  }
  
  if (since.has_value()) {
    args.push_back("--since=" + since.value());
  }

  auto log_result = nx::util::SafeProcess::executeForOutput("git", args, repo_path_.string());
  if (!log_result.has_value()) {
    return std::unexpected(makeError(ErrorCode::kGitError,
                                     "Failed to get git log: " + log_result.error().message()));
  }

  std::vector<std::string> commits;
  std::istringstream log_stream(log_result.value());
  std::string line;
  while (std::getline(log_stream, line)) {
    if (!line.empty()) {
      commits.push_back(line);
    }
  }

  return commits;
}

Result<void> GitSync::createBranch(const std::string& branch_name, bool checkout) {
  if (branch_name.empty()) {
    return std::unexpected(makeError(ErrorCode::kInvalidArgument,
                                     "Branch name cannot be empty"));
  }

  std::vector<std::string> args = {"branch", branch_name};
  
  auto branch_result = nx::util::SafeProcess::execute("git", args, repo_path_.string());
  if (!branch_result.has_value() || !branch_result->success()) {
    return std::unexpected(makeError(ErrorCode::kGitError,
                                     "Failed to create branch: " + 
                                     (branch_result.has_value() ? branch_result->stderr_output : "unknown error")));
  }

  if (checkout) {
    return checkoutBranch(branch_name);
  }

  return {};
}

Result<void> GitSync::checkoutBranch(const std::string& branch_name) {
  if (branch_name.empty()) {
    return std::unexpected(makeError(ErrorCode::kInvalidArgument,
                                     "Branch name cannot be empty"));
  }

  auto checkout_result = nx::util::SafeProcess::execute("git", 
    {"checkout", branch_name}, repo_path_.string());
  if (!checkout_result.has_value() || !checkout_result->success()) {
    return std::unexpected(makeError(ErrorCode::kGitError,
                                     "Failed to checkout branch: " + 
                                     (checkout_result.has_value() ? checkout_result->stderr_output : "unknown error")));
  }

  return {};
}

Result<std::vector<std::string>> GitSync::listBranches(bool include_remote) const {
  std::vector<std::string> args = {"branch"};
  
  if (include_remote) {
    args.push_back("-a");
  }

  auto branch_result = nx::util::SafeProcess::executeForOutput("git", args, repo_path_.string());
  if (!branch_result.has_value()) {
    return std::unexpected(makeError(ErrorCode::kGitError,
                                     "Failed to list branches: " + branch_result.error().message()));
  }

  std::vector<std::string> branches;
  std::istringstream branch_stream(branch_result.value());
  std::string line;
  while (std::getline(branch_stream, line)) {
    // Remove leading spaces and current branch marker (*)
    line.erase(0, line.find_first_not_of(" *"));
    if (!line.empty()) {
      branches.push_back(line);
    }
  }

  return branches;
}

Result<void> GitSync::resolveConflicts(const std::vector<std::string>& files, 
                                      const std::string& strategy) {
  if (strategy == "ours") {
    for (const auto& file : files) {
      auto resolve_result = nx::util::SafeProcess::execute("git", 
        {"checkout", "--ours", file}, repo_path_.string());
      if (!resolve_result.has_value() || !resolve_result->success()) {
        return std::unexpected(makeError(ErrorCode::kGitError,
                                         "Failed to resolve conflict for " + file));
      }
    }
  } else if (strategy == "theirs") {
    for (const auto& file : files) {
      auto resolve_result = nx::util::SafeProcess::execute("git", 
        {"checkout", "--theirs", file}, repo_path_.string());
      if (!resolve_result.has_value() || !resolve_result->success()) {
        return std::unexpected(makeError(ErrorCode::kGitError,
                                         "Failed to resolve conflict for " + file));
      }
    }
  } else if (strategy == "manual") {
    // Interactive manual conflict resolution
    for (const auto& file : files) {
      auto manual_result = resolveConflictManually(file);
      if (!manual_result.has_value()) {
        return std::unexpected(manual_result.error());
      }
    }
  } else {
    return std::unexpected(makeError(ErrorCode::kInvalidArgument,
                                     "Invalid strategy. Use 'ours', 'theirs', or 'manual'."));
  }

  // Stage resolved files
  for (const auto& file : files) {
    auto add_result = nx::util::SafeProcess::execute("git", 
      {"add", file}, repo_path_.string());
    if (!add_result.has_value() || !add_result->success()) {
      return std::unexpected(makeError(ErrorCode::kGitError,
                                       "Failed to stage resolved file " + file));
    }
  }

  return {};
}

Result<void> GitSync::resolveConflictManually(const std::string& file) {
  // Get the full path to the file
  auto file_path = repo_path_ / file;
  
  if (!std::filesystem::exists(file_path)) {
    return std::unexpected(makeError(ErrorCode::kFileNotFound,
                                     "Conflicted file not found: " + file));
  }
  
  std::cout << "\n=== Manual Conflict Resolution ===\n";
  std::cout << "File: " << file << "\n\n";
  
  // Show conflict status
  auto status_result = nx::util::SafeProcess::executeForOutput("git", 
    {"status", "--porcelain", file}, repo_path_.string());
  
  if (status_result.has_value()) {
    std::cout << "Status: " << status_result.value() << "\n";
  }
  
  // Present options to the user
  std::cout << "Choose resolution strategy:\n";
  std::cout << "  1) Keep our version (local changes)\n";
  std::cout << "  2) Keep their version (remote changes)\n";
  std::cout << "  3) Edit manually in editor\n";
  std::cout << "  4) Show conflict diff\n";
  std::cout << "  5) Skip this file\n";
  std::cout << "\nChoice [1-5]: ";
  
  std::string choice;
  std::getline(std::cin, choice);
  
  if (choice == "1") {
    // Use "ours" strategy
    auto resolve_result = nx::util::SafeProcess::execute("git", 
      {"checkout", "--ours", file}, repo_path_.string());
    if (!resolve_result.has_value() || !resolve_result->success()) {
      return std::unexpected(makeError(ErrorCode::kGitError,
                                       "Failed to resolve conflict using 'ours' strategy"));
    }
    std::cout << "Resolved using local version.\n";
  } 
  else if (choice == "2") {
    // Use "theirs" strategy
    auto resolve_result = nx::util::SafeProcess::execute("git", 
      {"checkout", "--theirs", file}, repo_path_.string());
    if (!resolve_result.has_value() || !resolve_result->success()) {
      return std::unexpected(makeError(ErrorCode::kGitError,
                                       "Failed to resolve conflict using 'theirs' strategy"));
    }
    std::cout << "Resolved using remote version.\n";
  }
  else if (choice == "3") {
    // Launch editor for manual resolution
    std::string editor = std::getenv("EDITOR") ? std::getenv("EDITOR") : "vi";
    
    std::cout << "Launching editor: " << editor << "\n";
    std::cout << "Please resolve conflicts manually and save the file.\n";
    std::cout << "Look for conflict markers: <<<<<<<, =======, >>>>>>>\n\n";
    
    std::ostringstream edit_cmd;
    edit_cmd << editor << " \"" << file_path.string() << "\"";
    
    int edit_result = std::system(edit_cmd.str().c_str());
    if (edit_result != 0) {
      std::cout << "Editor exited with non-zero status. Conflict may not be resolved.\n";
    }
    
    // Ask user to confirm resolution
    std::cout << "\nHas the conflict been resolved? (y/N): ";
    std::string confirm;
    std::getline(std::cin, confirm);
    
    if (confirm != "y" && confirm != "Y") {
      return std::unexpected(makeError(ErrorCode::kInvalidState,
                                       "Manual resolution cancelled by user"));
    }
    
    std::cout << "Manual resolution completed.\n";
  }
  else if (choice == "4") {
    // Show conflict diff and recurse for another choice
    auto diff_result = nx::util::SafeProcess::executeForOutput("git", 
      {"diff", file}, repo_path_.string());
    
    if (diff_result.has_value()) {
      std::cout << "\n--- Conflict Diff ---\n";
      std::cout << diff_result.value() << "\n";
      std::cout << "--- End Diff ---\n\n";
    } else {
      std::cout << "Could not show diff.\n";
    }
    
    // Recurse to ask for choice again
    return resolveConflictManually(file);
  }
  else if (choice == "5") {
    std::cout << "Skipping file: " << file << "\n";
    return std::unexpected(makeError(ErrorCode::kInvalidState,
                                     "User chose to skip conflict resolution"));
  }
  else {
    std::cout << "Invalid choice. Please try again.\n";
    return resolveConflictManually(file); // Recurse for valid input
  }
  
  // Verify that conflict markers are gone (basic check)
  std::ifstream file_stream(file_path);
  if (file_stream) {
    std::string line;
    while (std::getline(file_stream, line)) {
      if (line.find("<<<<<<<") != std::string::npos || 
          line.find("=======") != std::string::npos || 
          line.find(">>>>>>>") != std::string::npos) {
        std::cout << "Warning: Conflict markers still found in file.\n";
        std::cout << "Please ensure all conflicts are properly resolved.\n";
        break;
      }
    }
  }
  
  return {};
}

bool GitSync::isAvailable() {
  return nx::util::SafeProcess::commandExists("git");
}

Result<std::string> GitSync::getVersion() {
  auto version_result = nx::util::SafeProcess::executeForOutput("git", {"--version"});
  if (!version_result.has_value()) {
    return std::unexpected(makeError(ErrorCode::kExternalToolError,
                                     "Failed to get git version: " + version_result.error().message()));
  }

  return version_result.value();
}

} // namespace nx::sync