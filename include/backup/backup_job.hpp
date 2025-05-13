#pragma once

#include <string>
#include <memory>
#include <mutex>
#include <atomic>
#include <future>
#include "backup/backup_config.hpp"

namespace vmware {

enum class BackupStatus {
    NOT_FOUND,
    PENDING,
    RUNNING,
    PAUSED,
    COMPLETED,
    FAILED,
    CANCELLED
};

class BackupJob {
public:
    BackupJob(const std::string& vmId, const BackupConfig& config);
    ~BackupJob();

    // Job control
    bool start();
    bool stop();
    bool pause();
    bool resume();
    
    // Status and info
    BackupStatus getStatus() const;
    const std::string& getVMId() const { return vmId_; }
    const BackupConfig& getConfig() const { return config_; }
    bool setConfig(const BackupConfig& config);
    
    // Progress
    double getProgress() const;
    std::string getErrorMessage() const;

private:
    std::string vmId_;
    BackupConfig config_;
    std::atomic<BackupStatus> status_;
    std::atomic<double> progress_;
    std::string errorMessage_;
    mutable std::mutex mutex_;
    
    std::future<void> backupFuture_;
    std::atomic<bool> shouldStop_;
    
    void runBackup();
    bool validateConfig() const;
    void updateProgress(double progress);
    void setError(const std::string& error);
};

} // namespace vmware 