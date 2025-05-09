#pragma once

#include <string>
#include <memory>
#include <vector>
#include <chrono>
#include "common/vmware_connection.hpp"
#include "common/scheduler.hpp"
#include "common/parallel_task_manager.hpp"
#include "backup/disk_backup.hpp"

namespace vmware {

class BackupManager {
public:
    BackupManager(const std::string& host,
                 const std::string& username,
                 const std::string& password,
                 size_t maxConcurrentBackups = 4);
    ~BackupManager();

    // Initialize connection to vCenter
    bool initialize();

    // Perform full backup of a VM
    bool backupVM(const std::string& vmName,
                 const std::string& backupDir,
                 bool useCBT = true);

    // Schedule a backup task
    bool scheduleBackup(const std::string& vmName,
                       const std::string& backupDir,
                       const std::chrono::system_clock::time_point& scheduledTime,
                       bool useCBT = true);

    // Schedule a periodic backup task
    bool schedulePeriodicBackup(const std::string& vmName,
                              const std::string& backupDir,
                              const std::chrono::seconds& interval,
                              bool useCBT = true);

    // Cancel a scheduled backup
    bool cancelScheduledBackup(const std::string& taskId);

    // Get list of VMs available for backup
    std::vector<std::string> getAvailableVMs();

    // Snapshot management
    bool createBackupSnapshot(const std::string& vmName);
    bool removeBackupSnapshot(const std::string& vmName);
    std::string getBackupSnapshotName(const std::string& vmName) const;

    // Start the backup scheduler
    void startScheduler();

    // Stop the backup scheduler
    void stopScheduler();

private:
    std::unique_ptr<VMwareConnection> connection_;
    std::unique_ptr<Scheduler> scheduler_;
    std::unique_ptr<ParallelTaskManager> taskManager_;
    std::string host_;
    std::string username_;
    std::string password_;
    bool initialized_;

    // Helper methods
    bool enableCBT(const std::string& vmName);
    bool disableCBT(const std::string& vmName);
    bool createBackupDirectory(const std::string& backupDir);
    void logBackupProgress(const std::string& message);
    std::string generateTaskId(const std::string& vmName) const;
}; 