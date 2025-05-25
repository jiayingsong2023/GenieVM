#pragma once

#include "backup/job_manager.hpp"
#include "backup/backup_job.hpp"
#include "backup/backup_provider.hpp"
#include "common/logger.hpp"
#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <filesystem>

class BackupScheduler {
public:
    BackupScheduler(std::shared_ptr<JobManager> jobManager,
                   std::shared_ptr<BackupProvider> provider);
    ~BackupScheduler();

    // Core functionality
    bool initialize();
    bool scheduleBackup(const std::string& vmId, const BackupConfig& config);
    bool cancelBackup(const std::string& backupId);
    bool pauseBackup(const std::string& backupId);
    bool resumeBackup(const std::string& backupId);
    std::vector<BackupConfig> getScheduledBackups() const;
    BackupConfig getBackupConfig(const std::string& backupId) const;
    std::chrono::system_clock::time_point getNextRunTime(const BackupConfig& config) const;

    // Schedule management
    void addSchedule(const std::string& vmId, const BackupConfig& config);
    void removeSchedule(const std::string& vmId);
    void updateSchedule(const std::string& vmId, const BackupConfig& config);

    // Retention and cleanup
    void applyRetentionPolicy(const std::string& vmId);
    void cleanupOldBackups(const std::string& vmId);
    std::vector<std::string> getBackupPaths(const std::string& vmId) const;
    bool isBackupExpired(const std::string& vmId, int retentionDays) const;
    bool isBackupNeeded(const std::string& vmId, const BackupConfig& config) const;
    std::string getBackupPath(const std::string& vmId, const BackupConfig& config) const;

    // Thread control
    void start();
    void stop();
    bool isRunning() const { return running_; }

private:
    void executeBackup(const std::string& vmId, const BackupConfig& config);
    void checkSchedules();
    bool shouldRunBackup(const BackupConfig& config) const;

    std::shared_ptr<JobManager> jobManager_;
    std::shared_ptr<BackupProvider> provider_;
    std::map<std::string, BackupConfig> schedules_;
    std::mutex mutex_;
    std::thread schedulerThread_;
    std::atomic<bool> running_;
    std::atomic<bool> stopRequested_;
}; 