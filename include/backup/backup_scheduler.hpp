#pragma once

#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <mutex>
#include <thread>
#include <atomic>
#include <unordered_map>
#include "backup/backup_config.hpp"
#include "common/logger.hpp"

namespace vmware {

class BackupScheduler {
public:
    BackupScheduler();
    ~BackupScheduler();

    // Schedule management
    bool addSchedule(const std::string& vmId, const BackupConfig& config);
    bool removeSchedule(const std::string& vmId);
    bool updateSchedule(const std::string& vmId, const BackupConfig& config);
    bool getSchedule(const std::string& vmId, BackupConfig& config) const;
    
    // Schedule information
    void getAllSchedules(std::vector<std::pair<std::string, BackupConfig>>& schedules) const;
    std::chrono::system_clock::time_point getNextRunTime(const std::string& vmId) const;
    
    // Retention management
    bool applyRetentionPolicy(const std::string& vmId);
    bool cleanupOldBackups(const std::string& vmId);
    
    // Schedule control
    void start();
    void stop();
    bool isRunning() const { return running_; }

private:
    struct Schedule {
        BackupConfig config;
        std::chrono::system_clock::time_point nextRun;
        std::chrono::system_clock::time_point lastRun;
    };

    std::unordered_map<std::string, Schedule> schedules_;
    mutable std::mutex mutex_;
    std::thread schedulerThread_;
    std::atomic<bool> running_;
    
    void schedulerLoop();
    void checkSchedules();
    bool shouldRunBackup(const Schedule& schedule) const;
    void updateNextRun(Schedule& schedule);
    std::chrono::system_clock::time_point calculateNextRun(const BackupConfig& config) const;
    bool isBackupExpired(const std::string& backupPath, int retentionDays) const;
    std::vector<std::string> getBackupPaths(const std::string& vmId) const;
};

} // namespace vmware 