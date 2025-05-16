#pragma once

#include "backup/backup_config.hpp"
#include "common/scheduler.hpp"
#include "common/logger.hpp"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>
#include <chrono>

class BackupManager;

class BackupScheduler {
public:
    struct Schedule {
        BackupConfig config;
        std::chrono::system_clock::time_point nextRun;
        std::chrono::system_clock::time_point lastRun;
        std::string lastBackupPath;
    };

    explicit BackupScheduler(std::shared_ptr<BackupManager> manager);
    ~BackupScheduler();

    bool initialize();
    bool scheduleBackup(const std::string& vmId, const BackupConfig& config);
    bool cancelBackup(const std::string& backupId);
    bool pauseBackup(const std::string& backupId);
    bool resumeBackup(const std::string& backupId);
    std::vector<BackupConfig> getScheduledBackups() const;
    BackupConfig getBackupConfig(const std::string& backupId) const;

    void addSchedule(const std::string& vmId, const BackupConfig& config);
    void removeSchedule(const std::string& vmId);
    void updateSchedule(const std::string& vmId, const BackupConfig& config);
    BackupConfig getSchedule(const std::string& vmId) const;
    void getAllSchedules(std::vector<std::pair<std::string, BackupConfig>>& schedules) const;
    std::chrono::system_clock::time_point getNextRunTime(const std::string& vmId) const;

    void applyRetentionPolicy(const std::string& vmId);
    void cleanupOldBackups(const std::string& vmId);

    void start();
    void stop();

private:
    std::shared_ptr<BackupManager> manager_;
    std::unique_ptr<Scheduler> scheduler_;
    std::unordered_map<std::string, Schedule> schedules_;
    std::unordered_map<std::string, BackupConfig> scheduledBackups_;
    std::mutex mutex_;
    bool running_ = false;

    void checkSchedules();
    bool shouldRunBackup(const Schedule& schedule) const;
    void updateNextRun(Schedule& schedule);
    std::chrono::system_clock::time_point calculateNextRun(const BackupConfig& config) const;
    std::vector<std::string> getBackupPaths(const std::string& vmId) const;
    bool isBackupExpired(const std::string& vmId, int retentionDays) const;
    bool isBackupNeeded(const std::string& vmId, const BackupConfig& config) const;
    std::string getBackupPath(const std::string& vmId, const BackupConfig& config) const;
}; 