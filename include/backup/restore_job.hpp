#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <future>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "common/logger.hpp"
#include "common/vsphere_manager.hpp"
#include "backup/backup_config.hpp"

enum class RestoreStatus {
    NOT_FOUND,
    PENDING,
    RUNNING,
    PAUSED,
    COMPLETED,
    FAILED,
    CANCELLED
};

class RestoreJob {
public:
    RestoreJob(const std::string& vmId, const std::string& backupId, const BackupConfig& config);
    ~RestoreJob();

    // Job control
    bool start();
    bool stop();
    bool pause();
    bool resume();

    // Status and information
    RestoreStatus getStatus() const;
    std::string getVMId() const;
    std::string getBackupId() const;
    const BackupConfig& getConfig() const;
    double getProgress() const;
    std::string getErrorMessage() const;

private:
    void runRestore();
    bool validateConfig();
    void updateProgress(double progress);
    void setError(const std::string& message);

    std::string vmId_;
    std::string backupId_;
    BackupConfig config_;
    RestoreStatus status_;
    double progress_;
    std::string errorMessage_;
    mutable std::mutex mutex_;
    std::future<void> restoreFuture_;
    std::atomic<bool> cancelled_;
    std::shared_ptr<VSphereRestClient> vsphereClient_;
}; 