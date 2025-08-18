#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <optional>

#include "nx/common.hpp"

namespace nx::sync {

/**
 * @brief Git synchronization status
 */
enum class SyncStatus {
  Clean,      // No changes
  Modified,   // Local changes
  Ahead,      // Local commits ahead of remote
  Behind,     // Local behind remote
  Diverged,   // Local and remote have diverged
  Conflict    // Merge conflicts
};

/**
 * @brief Information about sync status
 */
struct SyncInfo {
  SyncStatus status;
  int commits_ahead = 0;
  int commits_behind = 0;
  std::vector<std::string> modified_files;
  std::vector<std::string> untracked_files;
  std::string current_branch;
  std::string remote_branch;
  std::string last_commit_hash;
  std::string last_commit_message;
};

/**
 * @brief Git configuration for synchronization
 */
struct GitConfig {
  std::string remote_url;
  std::string branch = "main";
  std::string user_name;
  std::string user_email;
  bool auto_push = false;
  bool auto_pull = false;
};

/**
 * @brief Git synchronization interface using libgit2
 * 
 * Provides safe git operations for synchronizing notes with remote repositories.
 */
class GitSync {
public:
  /**
   * @brief Initialize git sync for a repository
   * @param repo_path Path to git repository (notes directory)
   * @param config Git configuration
   * @return GitSync instance or error
   */
  static Result<GitSync> initialize(const std::filesystem::path& repo_path, const GitConfig& config);

  /**
   * @brief Initialize a new git repository
   * @param repo_path Path where to create repository
   * @param config Git configuration
   * @return GitSync instance or error
   */
  static Result<GitSync> initializeRepository(const std::filesystem::path& repo_path, const GitConfig& config);

  /**
   * @brief Clone a remote repository
   * @param remote_url URL of remote repository
   * @param local_path Local path to clone to
   * @param config Git configuration
   * @return GitSync instance or error
   */
  static Result<GitSync> cloneRepository(const std::string& remote_url, 
                                        const std::filesystem::path& local_path,
                                        const GitConfig& config);

  /**
   * @brief Get current sync status
   * @return Sync information or error
   */
  Result<SyncInfo> getStatus() const;

  /**
   * @brief Add and commit changes
   * @param message Commit message
   * @param files Optional list of specific files to commit (empty = all changes)
   * @return Success or error
   */
  Result<void> commit(const std::string& message, const std::vector<std::string>& files = {});

  /**
   * @brief Pull changes from remote
   * @param strategy Merge strategy ("merge", "rebase", "fast-forward")
   * @return Success or error
   */
  Result<void> pull(const std::string& strategy = "merge");

  /**
   * @brief Push changes to remote
   * @param force Force push (use with caution)
   * @return Success or error
   */
  Result<void> push(bool force = false);

  /**
   * @brief Sync with remote (pull then push)
   * @param auto_resolve Try to auto-resolve simple conflicts
   * @return Sync information or error
   */
  Result<SyncInfo> sync(bool auto_resolve = true);

  /**
   * @brief Get commit history
   * @param limit Maximum number of commits to return
   * @param since Optional date filter
   * @return List of commit information or error
   */
  Result<std::vector<std::string>> getHistory(int limit = 10, 
                                              const std::optional<std::string>& since = std::nullopt) const;

  /**
   * @brief Create a new branch
   * @param branch_name Name of new branch
   * @param checkout Switch to new branch immediately
   * @return Success or error
   */
  Result<void> createBranch(const std::string& branch_name, bool checkout = true);

  /**
   * @brief Switch to a different branch
   * @param branch_name Branch to switch to
   * @return Success or error
   */
  Result<void> checkoutBranch(const std::string& branch_name);

  /**
   * @brief List all branches
   * @param include_remote Include remote branches
   * @return List of branch names or error
   */
  Result<std::vector<std::string>> listBranches(bool include_remote = false) const;

  /**
   * @brief Resolve merge conflicts
   * @param files Files with conflicts to resolve
   * @param strategy Resolution strategy ("ours", "theirs", "manual")
   * @return Success or error
   */
  Result<void> resolveConflicts(const std::vector<std::string>& files, 
                               const std::string& strategy = "manual");

  /**
   * @brief Check if libgit2 is available
   * @return true if git operations are supported
   */
  static bool isAvailable();

  /**
   * @brief Get git version information
   * @return Version string or error
   */
  static Result<std::string> getVersion();

private:
  explicit GitSync(std::filesystem::path repo_path, GitConfig config);

  std::filesystem::path repo_path_;
  GitConfig config_;
  void* repo_handle_ = nullptr; // libgit2 repository handle

  /**
   * @brief Initialize libgit2 library
   * @return Success or error
   */
  static Result<void> initializeLibgit2();

  /**
   * @brief Open existing repository
   * @return Success or error
   */
  Result<void> openRepository();

  /**
   * @brief Close repository handle
   */
  void closeRepository();

  /**
   * @brief Get repository status using libgit2
   * @return Status information or error
   */
  Result<SyncInfo> getRepositoryStatus() const;

  /**
   * @brief Stage files for commit
   * @param files Files to stage (empty = all changes)
   * @return Success or error
   */
  Result<void> stageFiles(const std::vector<std::string>& files);

  /**
   * @brief Manually resolve a single conflicted file interactively
   * @param file Path to the conflicted file
   * @return Success or error
   */
  Result<void> resolveConflictManually(const std::string& file);

  /**
   * @brief Create commit with staged changes
   * @param message Commit message
   * @return Commit hash or error
   */
  Result<std::string> createCommit(const std::string& message);
};

} // namespace nx::sync