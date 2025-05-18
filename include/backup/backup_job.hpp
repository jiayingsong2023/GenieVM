#pragma once

#include <memory>
#include <string>
#include <functional>
#include "backup/backup_provider.hpp"
#include "backup/vm_config.hpp"
#include <vector>
#include <thread>
#include <mutex>
#include <atomic>

// Forward declarations
class BackupProvider;

// Callback type definitions
using ProgressCallback = std::function<void(int progress)>;
using StatusCallback = std::function<void(const std::string& status)>;

class BackupJob {
public:
    enum class Status {
        PENDING,
        RUNNING,
        COMPLETED,
        FAILED,
        CANCELLED
    };

    BackupJob(std::shared_ptr<BackupProvider> provider, const BackupConfig& config);
    ~BackupJob();

    // Job control
    void start();
    void cancel();
    void verifyBackup();

    // Job status
    std::string getId() const;
    Status getStatus() const;
    double getProgress() const;
    std::string getErrorMessage() const;

    // Callbacks
    void setProgressCallback(std::function<void(int)> callback);
    void setStatusCallback(std::function<void(const std::string&)> callback);

private:
    std::shared_ptr<BackupProvider> provider_;
    BackupConfig config_;
    std::string id_;
    Status status_;
    double progress_;
    std::string errorMessage_;
    std::atomic<bool> running_;
    std::atomic<bool> paused_;
    std::thread worker_;
    mutable std::mutex mutex_;
    std::function<void(int)> progressCallback_;
    std::function<void(const std::string&)> statusCallback_;

    // Helper methods
    void workerFunction();
    void updateStatus(const std::string& status);
    void updateProgress(double progress);
    void setError(const std::string& error);
    std::string generateId() const;
}; 