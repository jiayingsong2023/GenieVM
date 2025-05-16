#include "backup/backup_scheduler.hpp"
#include "backup/backup_manager.hpp"
#include "common/logger.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <nlohmann/json.hpp>
#include <chrono>

using namespace std::chrono;
using namespace std::filesystem;

BackupScheduler::BackupScheduler(std::shared_ptr<BackupManager> manager)
    : manager_(manager), scheduler_(std::make_unique<Scheduler>()) {
}

BackupScheduler::~BackupScheduler() {
    stop();
}

bool BackupScheduler::initialize() {
    if (!manager_) {
        Logger::error("No manager provided");
        return false;
    }
    return true;
}

bool BackupScheduler::scheduleBackup(const std::string& vmId, const BackupConfig& config) {
    if (!manager_) {
        Logger::error("No manager provided");
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    scheduledBackups_[vmId] = config;
    return true;
}

bool BackupScheduler::cancelBackup(const std::string& backupId) {
    std::lock_guard<std::mutex> lock(mutex_);
    return scheduledBackups_.erase(backupId) > 0;
}

bool BackupScheduler::pauseBackup(const std::string& backupId) {
    // TODO: Implement pause logic
    return true;
}

bool BackupScheduler::resumeBackup(const std::string& backupId) {
    // TODO: Implement resume logic
    return true;
}

std::vector<BackupConfig> BackupScheduler::getScheduledBackups() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    std::vector<BackupConfig> configs;
    configs.reserve(scheduledBackups_.size());
    for (const auto& [_, config] : scheduledBackups_) {
        configs.push_back(config);
    }
    return configs;
}

BackupConfig BackupScheduler::getBackupConfig(const std::string& backupId) const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    auto it = scheduledBackups_.find(backupId);
    return it != scheduledBackups_.end() ? it->second : BackupConfig{};
}

void BackupScheduler::addSchedule(const std::string& vmId, const BackupConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Calculate next run time
    auto nextRun = calculateNextRun(config);
    
    // Create schedule entry
    Schedule schedule;
    schedule.config = config;
    schedule.nextRun = nextRun;
    schedule.lastRun = system_clock::time_point();
    schedule.lastBackupPath = "";
    
    // Add to schedules map
    schedules_[vmId] = schedule;
    
    // Schedule the task
    scheduler_->scheduleTask(vmId, system_clock::to_time_t(nextRun), [this, vmId]() {
        checkSchedules();
    });
    
    Logger::info("Added backup schedule for VM: " + vmId);
}

void BackupScheduler::removeSchedule(const std::string& vmId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        Logger::error("No schedule found for VM: " + vmId);
        return;
    }

    if (running_) {
        scheduler_->cancelTask(vmId);
    }

    schedules_.erase(it);
    Logger::info("Removed backup schedule for VM: " + vmId);
}

void BackupScheduler::updateSchedule(const std::string& vmId, const BackupConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        Logger::error("No schedule found for VM: " + vmId);
        return;
    }
    
    // Calculate next run time
    auto nextRun = calculateNextRun(config);
    
    // Update schedule
    it->second.config = config;
    it->second.nextRun = nextRun;
    
    // Reschedule the task
    if (running_) {
        scheduler_->cancelTask(vmId);
        scheduler_->scheduleTask(vmId, system_clock::to_time_t(nextRun), [this, vmId]() {
            checkSchedules();
        });
    }
    
    Logger::info("Updated backup schedule for VM: " + vmId);
}

BackupConfig BackupScheduler::getSchedule(const std::string& vmId) const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        throw std::runtime_error("No schedule found for VM: " + vmId);
    }

    return it->second.config;
}

void BackupScheduler::getAllSchedules(std::vector<std::pair<std::string, BackupConfig>>& schedules) const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    schedules.clear();
    schedules.reserve(schedules_.size());
    
    for (const auto& [vmId, schedule] : schedules_) {
        schedules.emplace_back(vmId, schedule.config);
    }
}

system_clock::time_point BackupScheduler::getNextRunTime(const std::string& vmId) const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        return system_clock::time_point::min();
    }
    
    return it->second.nextRun;
}

void BackupScheduler::applyRetentionPolicy(const std::string& vmId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        Logger::error("No schedule found for VM: " + vmId);
        return;
    }

    const auto& config = it->second.config;
    if (config.retentionDays <= 0) {
        return;
    }

    cleanupOldBackups(vmId);
}

void BackupScheduler::cleanupOldBackups(const std::string& vmId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        Logger::error("No schedule found for VM: " + vmId);
        return;
    }

    const auto& config = it->second.config;
    if (config.maxBackups <= 0) {
        return;
    }

    auto backupPaths = getBackupPaths(vmId);
    
    // Sort backups by creation time (newest first)
    std::sort(backupPaths.begin(), backupPaths.end(),
              [](const std::string& a, const std::string& b) {
                  return last_write_time(a) > last_write_time(b);
              });

    // Keep only the most recent backups within retention period
    size_t keptBackups = 0;
    for (const auto& path : backupPaths) {
        if (keptBackups >= config.maxBackups || isBackupExpired(vmId, config.retentionDays)) {
            try {
                remove_all(path);
                Logger::info("Removed old backup: " + path);
            } catch (const std::filesystem::filesystem_error& e) {
                Logger::error("Failed to remove old backup " + path + ": " + e.what());
            }
        } else {
            keptBackups++;
        }
    }
}

void BackupScheduler::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) {
        Logger::info("Scheduler is already running");
        return;
    }
    
    running_ = true;
    
    // Schedule all existing tasks
    for (const auto& [vmId, schedule] : schedules_) {
        scheduler_->scheduleTask(vmId, system_clock::to_time_t(schedule.nextRun), [this, vmId]() {
            checkSchedules();
        });
    }
    
    Logger::info("Backup scheduler started");
}

void BackupScheduler::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    scheduler_.reset();
    Logger::info("Backup scheduler stopped");
}

void BackupScheduler::checkSchedules() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = system_clock::now();
    
    for (auto& [vmId, schedule] : schedules_) {
        if (now >= schedule.nextRun) {
            // Check if backup is needed
            if (isBackupNeeded(vmId, schedule.config)) {
                // Start backup
                if (manager_->startBackup(vmId, schedule.config)) {
                    schedule.lastRun = now;
                    schedule.lastBackupPath = getBackupPath(vmId, schedule.config);
                    
                    // Calculate next run time
                    schedule.nextRun = calculateNextRun(schedule.config);
                    
                    // Reschedule next run
                    scheduler_->scheduleTask(vmId, system_clock::to_time_t(schedule.nextRun), [this, vmId]() {
                        checkSchedules();
                    });
                    
                    Logger::info("Started scheduled backup for VM: " + vmId);
                } else {
                    Logger::error("Failed to start scheduled backup for VM: " + vmId);
                }
            } else {
                // Skip backup, calculate next run time
                schedule.nextRun = calculateNextRun(schedule.config);
                
                // Reschedule next run
                scheduler_->scheduleTask(vmId, system_clock::to_time_t(schedule.nextRun), [this, vmId]() {
                    checkSchedules();
                });
                
                Logger::info("Skipped scheduled backup for VM: " + vmId + " (not needed)");
            }
        }
    }
}

bool BackupScheduler::shouldRunBackup(const Schedule& schedule) const {
    return schedule.nextRun <= system_clock::now();
}

void BackupScheduler::updateNextRun(Schedule& schedule) {
    schedule.nextRun = calculateNextRun(schedule.config);
}

system_clock::time_point BackupScheduler::calculateNextRun(const BackupConfig& config) const {
    auto now = system_clock::now();
    auto nowTime = system_clock::to_time_t(now);
    auto localTime = std::localtime(&nowTime);
    
    if (config.scheduleType == "daily") {
        localTime->tm_hour = config.schedule.hour;
        localTime->tm_min = config.schedule.minute;
        localTime->tm_sec = 0;
        
        auto nextRun = system_clock::from_time_t(std::mktime(localTime));
        if (nextRun <= now) {
            nextRun += hours(24);
        }
        return nextRun;
    } else if (config.scheduleType == "weekly") {
        int daysUntilNext = (config.schedule.day - localTime->tm_wday + 7) % 7;
        
        if (daysUntilNext == 0 && 
            (localTime->tm_hour > config.schedule.hour ||
             (localTime->tm_hour == config.schedule.hour && localTime->tm_min >= config.schedule.minute))) {
            daysUntilNext = 7;
        }
        
        localTime->tm_mday += daysUntilNext;
        localTime->tm_hour = config.schedule.hour;
        localTime->tm_min = config.schedule.minute;
        localTime->tm_sec = 0;
        
        return system_clock::from_time_t(std::mktime(localTime));
    } else if (config.scheduleType == "monthly") {
        localTime->tm_mday = config.schedule.day;
        localTime->tm_hour = config.schedule.hour;
        localTime->tm_min = config.schedule.minute;
        localTime->tm_sec = 0;
        
        auto nextRun = system_clock::from_time_t(std::mktime(localTime));
        if (nextRun <= now) {
            localTime->tm_mon++;
            nextRun = system_clock::from_time_t(std::mktime(localTime));
        }
        return nextRun;
    }
    
    return now + hours(24); // Default to daily if schedule type is invalid
}

std::vector<std::string> BackupScheduler::getBackupPaths(const std::string& vmId) const {
    std::vector<std::string> paths;
    std::string backupDir = "backups/" + vmId;
    
    try {
        for (const auto& entry : directory_iterator(backupDir)) {
            if (entry.is_directory()) {
                paths.push_back(entry.path().string());
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        Logger::error("Failed to read backup directory: " + std::string(e.what()));
    }
    
    return paths;
}

bool BackupScheduler::isBackupExpired(const std::string& vmId, int retentionDays) const {
    auto it = schedules_.find(vmId);
    if (it == schedules_.end() || it->second.lastBackupPath.empty()) {
        return true;
    }
    
    try {
        auto now = system_clock::now();
        auto lastWriteTime = system_clock::from_time_t(
            std::chrono::system_clock::to_time_t(
                system_clock::now() - duration_cast<system_clock::duration>(
                    last_write_time(it->second.lastBackupPath).time_since_epoch()
                )
            )
        );
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - lastWriteTime);
        return age.count() >= retentionDays * 24;
    } catch (const std::exception& e) {
        Logger::error("Error checking backup expiration: " + std::string(e.what()));
        return true;
    }
}

bool BackupScheduler::isBackupNeeded(const std::string& vmId, const BackupConfig& config) const {
    // Check if backup is expired
    if (isBackupExpired(vmId, config.retentionDays)) {
        return true;
    }
    
    // Check if VM has changed
    if (config.enableCBT) {
        std::vector<std::pair<uint64_t, uint64_t>> changedBlocks;
        if (manager_->getChangedBlocks(vmId, "", changedBlocks)) {
            return !changedBlocks.empty();
        }
    }
    
    return false;
}

std::string BackupScheduler::getBackupPath(const std::string& vmId, const BackupConfig& config) const {
    auto now = system_clock::now();
    auto nowTime = system_clock::to_time_t(now);
    auto localTime = std::localtime(&nowTime);
    
    std::stringstream ss;
    ss << config.backupPath << "/" << vmId << "_"
       << std::put_time(localTime, "%Y%m%d_%H%M%S");
    return ss.str();
} 