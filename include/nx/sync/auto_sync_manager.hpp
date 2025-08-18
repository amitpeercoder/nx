#pragma once

#include <memory>
#include <filesystem>
#include <chrono>
#include <thread>
#include <atomic>
#include <mutex>

#include "nx/common.hpp"
#include "nx/config/config.hpp"
#include "nx/sync/git_sync.hpp"

namespace nx::sync {

/**
 * @brief Manages automatic Git synchronization based on configuration
 */
class AutoSyncManager {
public:
    explicit AutoSyncManager(const nx::config::Config& config);
    ~AutoSyncManager();

    // Start/stop automatic sync
    Result<void> start();
    void stop();
    bool isRunning() const;

    // Manual sync operations
    Result<void> pullOnStartup();
    Result<void> pushChanges(bool force_immediate = false);
    Result<void> syncOnShutdown();

    // Status and monitoring
    struct SyncStatus {
        bool is_syncing = false;
        bool has_local_changes = false;
        bool has_remote_changes = false;
        std::chrono::system_clock::time_point last_sync;
        std::chrono::system_clock::time_point last_check;
        std::string last_error;
        int consecutive_failures = 0;
    };
    
    SyncStatus getStatus() const;
    
    // Called when notes are modified
    void notifyNoteChanged();
    void notifyNotesChanged(size_t count);

private:
    const nx::config::Config& config_;
    std::unique_ptr<GitSync> git_sync_;
    
    // Threading
    std::atomic<bool> running_{false};
    std::atomic<bool> should_stop_{false};
    std::unique_ptr<std::thread> sync_thread_;
    mutable std::mutex status_mutex_;
    
    // State tracking
    SyncStatus status_;
    std::atomic<bool> pending_changes_{false};
    std::chrono::system_clock::time_point last_change_time_;
    
    // Background sync loop
    void syncLoop();
    
    // Internal operations
    Result<void> performSync();
    Result<void> performPull();
    Result<void> performPush();
    bool shouldAutoResolveConflicts() const;
    Result<void> autoResolveConflicts();
    
    // Utilities
    bool isNetworkAvailable() const;
    void updateStatus(const std::function<void(SyncStatus&)>& updater);
    void logSyncEvent(const std::string& event, bool success = true);
};

} // namespace nx::sync