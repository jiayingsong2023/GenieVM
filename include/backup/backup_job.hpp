#pragma once

#include "backup/backup_provider.hpp"
#include "backup/vm_config.hpp"
#include "common/logger.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>

// Callback type definitions
using ProgressCallback = std::function<void(int progress)>;
using StatusCallback = std::function<void(const std::string& status)>;

class BackupJob {
public:
    BackupJob(std::shared_ptr<BackupProvider> provider, const BackupConfig& config);
    ~BackupJob();

    // Job control
    bool start();
    bool cancel();
    bool pause();
    bool resume();

    // Status queries
    std::string getId() const;
    std::string getStatus() const;
    double getProgress() const;
    std::string getError() const;
    bool isRunning() const;
    bool isPaused() const;

    // Configuration
    BackupConfig getConfig() const;
    void setConfig(const BackupConfig& config);

    // Callbacks
    void setProgressCallback(ProgressCallback callback);
    void setStatusCallback(StatusCallback callback);

    // Verification
    bool verifyBackup();

private:
    std::shared_ptr<BackupProvider> provider_;
    BackupConfig config_;
    std::string id_;
    std::string status_;
    double progress_;
    std::string error_;
    std::atomic<bool> isRunning_;
    std::atomic<bool> isPaused_;
    ProgressCallback progressCallback_;
    StatusCallback statusCallback_;
    mutable std::mutex mutex_;

    // Helper methods
    void updateProgress(double progress);
    void setError(const std::string& error);
    std::string generateId() const;
}; 