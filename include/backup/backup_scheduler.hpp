#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include <chrono>
#include <mutex>
#include <thread>
#include "backup/backup_config.hpp"
#include "common/vmware_connection.hpp"

class BackupScheduler {
public:
    BackupScheduler(std::shared_ptr<VMwareConnection> connection);
    ~BackupScheduler();

    bool initialize();
    bool scheduleBackup(const std::string& vmId, const BackupConfig& config);
    bool cancelBackup(const std::string& backupId);
    bool pauseBackup(const std::string& backupId);
    bool resumeBackup(const std::string& backupId);
    std::vector<BackupConfig> getScheduledBackups() const;
    BackupConfig getBackupConfig(const std::string& backupId) const;

    // Schedule management
    void addSchedule(const std::string& vmId, const BackupConfig& config);
    void removeSchedule(const std::string& vmId);
    void updateSchedule(const std::string& vmId, const BackupConfig& config);
    BackupConfig getSchedule(const std::string& vmId) const;
    void getAllSchedules(std::vector<std::pair<std::string, BackupConfig>>& schedules) const;
    std::chrono::system_clock::time_point getNextRunTime(const std::string& vmId) const;

    // Retention policy
    void applyRetentionPolicy(const std::string& vmId);
    void cleanupOldBackups(const std::string& vmId);

    // Scheduler control
    void start();
    void stop();

private:
    struct Schedule {
        BackupConfig config;
        std::chrono::system_clock::time_point nextRun;
        std::chrono::system_clock::time_point lastRun;
    };

    void schedulerLoop();
    void checkSchedules();
    bool shouldRunBackup(const Schedule& schedule) const;
    void updateNextRun(Schedule& schedule);
    std::chrono::system_clock::time_point calculateNextRun(const BackupConfig& config) const;
    bool isBackupExpired(const std::string& backupPath, int retentionDays) const;
    std::vector<std::string> getBackupPaths(const std::string& vmId) const;

    std::shared_ptr<VMwareConnection> connection_;
    std::map<std::string, BackupConfig> scheduledBackups_;
    std::map<std::string, Schedule> schedules_;
    std::mutex mutex_;
    std::thread schedulerThread_;
    bool isRunning_;
}; 