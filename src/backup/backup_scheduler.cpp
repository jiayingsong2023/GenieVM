#include "backup/backup_scheduler.hpp"
#include "common/logger.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>
#include <algorithm>
#include <stdexcept>
#include <nlohmann/json.hpp>

BackupScheduler::BackupScheduler(std::shared_ptr<VMwareConnection> connection)
    : connection_(connection)
    , isRunning_(false) {
}

BackupScheduler::~BackupScheduler() {
    // Cleanup any running backups
    for (const auto& [backupId, config] : scheduledBackups_) {
        cancelBackup(backupId);
    }
}

bool BackupScheduler::initialize() {
    if (!connection_) {
        Logger::error("No connection provided");
        return false;
    }
    return true;
}

bool BackupScheduler::scheduleBackup(const std::string& vmId, const BackupConfig& config) {
    if (!connection_) {
        Logger::error("No connection provided");
        return false;
    }

    // Generate a unique backup ID
    std::string backupId = vmId + "_" + std::to_string(std::time(nullptr));
    scheduledBackups_[backupId] = config;
    return true;
}

bool BackupScheduler::cancelBackup(const std::string& backupId) {
    auto it = scheduledBackups_.find(backupId);
    if (it == scheduledBackups_.end()) {
        Logger::error("Backup not found: " + backupId);
        return false;
    }

    scheduledBackups_.erase(it);
    return true;
}

bool BackupScheduler::pauseBackup(const std::string& backupId) {
    auto it = scheduledBackups_.find(backupId);
    if (it == scheduledBackups_.end()) {
        Logger::error("Backup not found: " + backupId);
        return false;
    }

    // TODO: Implement pause logic
    return true;
}

bool BackupScheduler::resumeBackup(const std::string& backupId) {
    auto it = scheduledBackups_.find(backupId);
    if (it == scheduledBackups_.end()) {
        Logger::error("Backup not found: " + backupId);
        return false;
    }

    // TODO: Implement resume logic
    return true;
}

std::vector<BackupConfig> BackupScheduler::getScheduledBackups() const {
    std::vector<BackupConfig> configs;
    configs.reserve(scheduledBackups_.size());
    for (const auto& [_, config] : scheduledBackups_) {
        configs.push_back(config);
    }
    return configs;
}

BackupConfig BackupScheduler::getBackupConfig(const std::string& backupId) const {
    auto it = scheduledBackups_.find(backupId);
    if (it == scheduledBackups_.end()) {
        return BackupConfig{};
    }
    return it->second;
}

void BackupScheduler::addSchedule(const std::string& vmId, const BackupConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if schedule already exists
    if (schedules_.find(vmId) != schedules_.end()) {
        throw std::runtime_error("Schedule already exists for VM: " + vmId);
    }

    // Validate schedule time
    if (!config.scheduled) {
        throw std::runtime_error("Schedule is not enabled for VM: " + vmId);
    }

    // Add schedule
    Schedule schedule;
    schedule.config = config;
    schedule.nextRun = calculateNextRun(config);
    schedule.lastRun = std::chrono::system_clock::time_point::min();
    
    schedules_[vmId] = schedule;
    Logger::info("Added backup schedule for VM: " + vmId);
}

void BackupScheduler::removeSchedule(const std::string& vmId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        throw std::runtime_error("No schedule found for VM: " + vmId);
    }

    schedules_.erase(it);
    Logger::info("Removed backup schedule for VM: " + vmId);
}

void BackupScheduler::updateSchedule(const std::string& vmId, const BackupConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        throw std::runtime_error("No schedule found for VM: " + vmId);
    }

    // Validate schedule time
    if (!config.scheduled) {
        throw std::runtime_error("Schedule is not enabled for VM: " + vmId);
    }

    // Update schedule
    it->second.config = config;
    it->second.nextRun = calculateNextRun(config);
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

std::chrono::system_clock::time_point BackupScheduler::getNextRunTime(const std::string& vmId) const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        return std::chrono::system_clock::time_point::min();
    }
    
    return it->second.nextRun;
}

void BackupScheduler::applyRetentionPolicy(const std::string& vmId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        throw std::runtime_error("No schedule found for VM: " + vmId);
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
        throw std::runtime_error("No schedule found for VM: " + vmId);
    }

    const auto& config = it->second.config;
    if (config.maxBackups <= 0) {
        return;
    }

    auto backupPaths = getBackupPaths(vmId);
    
    // Sort backups by creation time (newest first)
    std::sort(backupPaths.begin(), backupPaths.end(),
              [](const std::string& a, const std::string& b) {
                  return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
              });

    // Keep only the most recent backups within retention period
    size_t keptBackups = 0;
    for (const auto& path : backupPaths) {
        if (keptBackups >= config.maxBackups || isBackupExpired(path, config.retentionDays)) {
            try {
                std::filesystem::remove_all(path);
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
    if (isRunning_) {
        return;
    }

    isRunning_ = true;
    schedulerThread_ = std::thread(&BackupScheduler::schedulerLoop, this);
    Logger::info("Backup scheduler started");
}

void BackupScheduler::stop() {
    if (!isRunning_) {
        return;
    }

    isRunning_ = false;
    if (schedulerThread_.joinable()) {
        schedulerThread_.join();
    }
    Logger::info("Backup scheduler stopped");
}

void BackupScheduler::schedulerLoop() {
    while (isRunning_) {
        checkSchedules();
        std::this_thread::sleep_for(std::chrono::minutes(1));
    }
}

void BackupScheduler::checkSchedules() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();

    for (auto& [vmId, schedule] : schedules_) {
        if (shouldRunBackup(schedule)) {
            try {
                // Create and start backup job
                scheduleBackup(vmId, schedule.config);
                schedule.lastRun = now;
                updateNextRun(schedule);
            } catch (const std::exception& e) {
                Logger::error("Failed to run scheduled backup for VM " + vmId + ": " + e.what());
            }
        }
    }
}

bool BackupScheduler::shouldRunBackup(const Schedule& schedule) const {
    auto now = std::chrono::system_clock::now();
    return schedule.nextRun <= now;
}

void BackupScheduler::updateNextRun(Schedule& schedule) {
    schedule.nextRun = calculateNextRun(schedule.config);
}

std::chrono::system_clock::time_point BackupScheduler::calculateNextRun(const BackupConfig& config) const {
    auto now = std::chrono::system_clock::now();
    auto now_time = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_time);

    // Set the scheduled time
    now_tm->tm_hour = config.scheduleHour;
    now_tm->tm_min = config.scheduleMinute;
    now_tm->tm_sec = 0;

    auto next_run = std::chrono::system_clock::from_time_t(std::mktime(now_tm));

    // If the scheduled time has already passed today, schedule for tomorrow
    if (next_run <= now) {
        next_run += std::chrono::hours(24);
    }

    return next_run;
}

std::vector<std::string> BackupScheduler::getBackupPaths(const std::string& vmId) const {
    std::vector<std::string> paths;
    std::string backupDir = "backups/" + vmId;

    try {
        for (const auto& entry : std::filesystem::directory_iterator(backupDir)) {
            if (entry.is_directory()) {
                paths.push_back(entry.path().string());
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        Logger::error("Failed to get backup paths for VM " + vmId + ": " + e.what());
    }

    return paths;
}

bool BackupScheduler::isBackupExpired(const std::string& backupPath, int retentionDays) const {
    try {
        auto lastWriteTime = std::filesystem::last_write_time(backupPath);
        auto now = std::chrono::system_clock::now();
        
        // Convert file time to system time
        auto fileTime = std::chrono::system_clock::from_time_t(
            std::chrono::system_clock::to_time_t(now) - 
            std::chrono::duration_cast<std::chrono::seconds>(
                now.time_since_epoch() - lastWriteTime.time_since_epoch()
            ).count()
        );
        
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - fileTime);
        return age > std::chrono::hours(24 * retentionDays);
    } catch (const std::filesystem::filesystem_error& e) {
        Logger::error("Failed to check backup expiration for " + backupPath + ": " + e.what());
        return false;
    }
} 