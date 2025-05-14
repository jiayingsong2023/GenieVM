#pragma once

#include <string>
#include <memory>
#include <vector>
#include <atomic>
#include "common/vmware_connection.hpp"
#include "backup/backup_config.hpp"

class BackupJob {
public:
    enum class Status {
        PENDING,
        RUNNING,
        PAUSED,
        COMPLETED,
        FAILED,
        CANCELLED
    };

    BackupJob(std::shared_ptr<VMwareConnection> connection, const BackupConfig& config);
    ~BackupJob();

    // Job control
    bool start();
    bool stop();
    bool pause();
    bool resume();
    bool cancel();

    // Status and information
    Status getStatus() const;
    std::string getId() const;
    BackupConfig getConfig() const;
    std::string getErrorMessage() const;
    double getProgress() const;

private:
    bool prepareVM();
    bool cleanupVM();
    std::string createSnapshot();
    bool removeSnapshot(const std::string& snapshotId);
    void updateProgress(double progress);
    void setStatus(Status status);
    void setError(const std::string& error);

    std::shared_ptr<VMwareConnection> connection_;
    BackupConfig config_;
    std::string id_;
    std::string snapshotId_;
    std::atomic<Status> status_;
    std::atomic<double> progress_;
    std::string errorMessage_;
}; 