#include "nx/sync/auto_sync_manager.hpp"

#include <iostream>
#include <sstream>
#include <algorithm>

namespace nx::sync {

AutoSyncManager::AutoSyncManager(const nx::config::Config& config) 
    : config_(config) {
    
    // Initialize GitSync if sync is enabled
    if (config_.sync == nx::config::Config::SyncType::kGit && 
        config_.auto_sync.enabled) {
        
        GitConfig git_config{
            .user_name = config_.git_user_name,
            .user_email = config_.git_user_email,
            .remote_url = config_.git_remote
        };
        
        auto sync_result = GitSync::initialize(config_.notes_dir, git_config);
        if (sync_result.has_value()) {
            git_sync_ = std::make_unique<GitSync>(std::move(sync_result.value()));
        }
    }
}

AutoSyncManager::~AutoSyncManager() {
    stop();
}

Result<void> AutoSyncManager::start() {
    if (!git_sync_) {
        return std::unexpected(makeError(ErrorCode::kConfigError,
                                         "Git sync not configured"));
    }
    
    if (running_.load()) {
        return {}; // Already running
    }
    
    // Pull on startup if configured
    if (config_.auto_sync.auto_pull_on_startup) {
        auto pull_result = pullOnStartup();
        if (!pull_result.has_value()) {
            logSyncEvent("Startup pull failed: " + pull_result.error().message(), false);
        }
    }
    
    // Start background sync thread
    running_.store(true);
    should_stop_.store(false);
    sync_thread_ = std::make_unique<std::thread>(&AutoSyncManager::syncLoop, this);
    
    logSyncEvent("Auto-sync started");
    return {};
}

void AutoSyncManager::stop() {
    if (!running_.load()) {
        return;
    }
    
    // Sync on shutdown if configured
    if (config_.auto_sync.sync_on_shutdown && pending_changes_.load()) {
        auto sync_result = syncOnShutdown();
        if (!sync_result.has_value()) {
            logSyncEvent("Shutdown sync failed: " + sync_result.error().message(), false);
        }
    }
    
    // Stop background thread
    should_stop_.store(true);
    if (sync_thread_ && sync_thread_->joinable()) {
        sync_thread_->join();
    }
    sync_thread_.reset();
    running_.store(false);
    
    logSyncEvent("Auto-sync stopped");
}

bool AutoSyncManager::isRunning() const {
    return running_.load();
}

Result<void> AutoSyncManager::pullOnStartup() {
    if (!git_sync_) {
        return std::unexpected(makeError(ErrorCode::kConfigError, "Git sync not available"));
    }
    
    updateStatus([](SyncStatus& status) {
        status.is_syncing = true;
        status.last_check = std::chrono::system_clock::now();
    });
    
    auto result = performPull();
    
    updateStatus([&](SyncStatus& status) {
        status.is_syncing = false;
        if (result.has_value()) {
            status.last_sync = std::chrono::system_clock::now();
            status.consecutive_failures = 0;
            status.last_error.clear();
        } else {
            status.consecutive_failures++;
            status.last_error = result.error().message();
        }
    });
    
    return result;
}

Result<void> AutoSyncManager::pushChanges(bool force_immediate) {
    if (!git_sync_ || !pending_changes_.load()) {
        return {};
    }
    
    // Check if we should delay the push
    if (!force_immediate && config_.auto_sync.auto_push_delay_seconds > 0) {
        auto now = std::chrono::system_clock::now();
        auto delay = std::chrono::seconds(config_.auto_sync.auto_push_delay_seconds);
        
        if (now - last_change_time_ < delay) {
            // Too soon after last change, wait for background sync
            return {};
        }
    }
    
    updateStatus([](SyncStatus& status) {
        status.is_syncing = true;
    });
    
    auto result = performPush();
    
    updateStatus([&](SyncStatus& status) {
        status.is_syncing = false;
        if (result.has_value()) {
            status.last_sync = std::chrono::system_clock::now();
            status.consecutive_failures = 0;
            status.last_error.clear();
        } else {
            status.consecutive_failures++;
            status.last_error = result.error().message();
        }
    });
    
    if (result.has_value()) {
        pending_changes_.store(false);
    }
    
    return result;
}

Result<void> AutoSyncManager::syncOnShutdown() {
    if (!git_sync_) {
        return {};
    }
    
    // Force immediate sync
    return performSync();
}

AutoSyncManager::SyncStatus AutoSyncManager::getStatus() const {
    std::lock_guard<std::mutex> lock(status_mutex_);
    return status_;
}

void AutoSyncManager::notifyNoteChanged() {
    if (config_.auto_sync.auto_push_on_changes) {
        pending_changes_.store(true);
        last_change_time_ = std::chrono::system_clock::now();
    }
}

void AutoSyncManager::notifyNotesChanged(size_t count) {
    if (config_.auto_sync.auto_push_on_changes && count > 0) {
        pending_changes_.store(true);
        last_change_time_ = std::chrono::system_clock::now();
    }
}

void AutoSyncManager::syncLoop() {
    auto sync_interval = std::chrono::seconds(config_.auto_sync.sync_interval_seconds);
    
    while (!should_stop_.load()) {
        try {
            // Check for network availability
            if (!isNetworkAvailable()) {
                std::this_thread::sleep_for(std::chrono::seconds(30));
                continue;
            }
            
            // Check if we need to push pending changes
            if (pending_changes_.load() && config_.auto_sync.auto_push_on_changes) {
                auto now = std::chrono::system_clock::now();
                auto delay = std::chrono::seconds(config_.auto_sync.auto_push_delay_seconds);
                
                if (now - last_change_time_ >= delay) {
                    auto push_result = pushChanges(true);
                    if (!push_result.has_value()) {
                        logSyncEvent("Background push failed: " + push_result.error().message(), false);
                    }
                }
            }
            
            // Periodic full sync
            auto now = std::chrono::system_clock::now();
            if (now - status_.last_sync >= sync_interval) {
                auto sync_result = performSync();
                if (!sync_result.has_value()) {
                    logSyncEvent("Background sync failed: " + sync_result.error().message(), false);
                }
            }
            
        } catch (const std::exception& e) {
            logSyncEvent("Sync loop exception: " + std::string(e.what()), false);
        }
        
        // Sleep for a short interval before checking again
        std::this_thread::sleep_for(std::chrono::seconds(30));
    }
}

Result<void> AutoSyncManager::performSync() {
    if (!git_sync_) {
        return std::unexpected(makeError(ErrorCode::kConfigError, "Git sync not available"));
    }
    
    updateStatus([](SyncStatus& status) {
        status.is_syncing = true;
        status.last_check = std::chrono::system_clock::now();
    });
    
    // First pull to get remote changes
    auto pull_result = git_sync_->pull("merge");
    if (!pull_result.has_value()) {
        // Check if we can auto-resolve conflicts
        if (shouldAutoResolveConflicts()) {
            auto resolve_result = autoResolveConflicts();
            if (!resolve_result.has_value()) {
                updateStatus([&](SyncStatus& status) {
                    status.is_syncing = false;
                    status.consecutive_failures++;
                    status.last_error = "Failed to auto-resolve conflicts: " + resolve_result.error().message();
                });
                return resolve_result;
            }
        } else {
            updateStatus([&](SyncStatus& status) {
                status.is_syncing = false;
                status.consecutive_failures++;
                status.last_error = "Pull failed: " + pull_result.error().message();
            });
            return pull_result;
        }
    }
    
    // Then push local changes
    auto push_result = git_sync_->push(false);
    
    updateStatus([&](SyncStatus& status) {
        status.is_syncing = false;
        if (push_result.has_value()) {
            status.last_sync = std::chrono::system_clock::now();
            status.consecutive_failures = 0;
            status.last_error.clear();
        } else {
            status.consecutive_failures++;
            status.last_error = "Push failed: " + push_result.error().message();
        }
    });
    
    if (push_result.has_value()) {
        pending_changes_.store(false);
    }
    
    return push_result;
}

Result<void> AutoSyncManager::performPull() {
    if (!git_sync_) {
        return std::unexpected(makeError(ErrorCode::kConfigError, "Git sync not available"));
    }
    
    return git_sync_->pull("merge");
}

Result<void> AutoSyncManager::performPush() {
    if (!git_sync_) {
        return std::unexpected(makeError(ErrorCode::kConfigError, "Git sync not available"));
    }
    
    return git_sync_->push(false);
}

bool AutoSyncManager::shouldAutoResolveConflicts() const {
    return config_.auto_sync.conflict_strategy != "manual" &&
           status_.consecutive_failures < config_.auto_sync.max_auto_resolve_attempts;
}

Result<void> AutoSyncManager::autoResolveConflicts() {
    if (!git_sync_) {
        return std::unexpected(makeError(ErrorCode::kConfigError, "Git sync not available"));
    }
    
    // Simple auto-resolution strategies
    if (config_.auto_sync.conflict_strategy == "ours") {
        return git_sync_->resolveConflicts({}, "ours");
    } else if (config_.auto_sync.conflict_strategy == "theirs") {
        return git_sync_->resolveConflicts({}, "theirs");
    } else if (config_.auto_sync.conflict_strategy == "smart") {
        // For now, use "ours" strategy - could be enhanced with AI resolution
        return git_sync_->resolveConflicts({}, "ours");
    }
    
    return std::unexpected(makeError(ErrorCode::kGitError, "Unknown conflict resolution strategy"));
}

bool AutoSyncManager::isNetworkAvailable() const {
    // Simple network check - could be enhanced
    return true;
}

void AutoSyncManager::updateStatus(const std::function<void(SyncStatus&)>& updater) {
    std::lock_guard<std::mutex> lock(status_mutex_);
    updater(status_);
}

void AutoSyncManager::logSyncEvent(const std::string& event, bool success) {
    // For now, just log to stderr - could integrate with proper logging
    std::cerr << "[AutoSync] " << event << std::endl;
}

} // namespace nx::sync