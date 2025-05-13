#include "backup/backup_scheduler.hpp"
#include <filesystem>
#include <algorithm>
#include <ctime>

namespace vmware {

BackupScheduler::BackupScheduler()
    : running_(false) {
}

BackupScheduler::~BackupScheduler() {
    stop();
}

bool BackupScheduler::addSchedule(const std::string& vmId, const BackupConfig& config) {
    if (!config.scheduled) {
        Logger::warning("Attempting to add non-scheduled backup for VM " + vmId);
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    
    Schedule schedule;
    schedule.config = config;
    schedule.nextRun = calculateNextRun(config);
    schedule.lastRun = std::chrono::system_clock::time_point::min();
    
    schedules_[vmId] = schedule;
    Logger::info("Added backup schedule for VM " + vmId);
    return true;
}

bool BackupScheduler::removeSchedule(const std::string& vmId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (schedules_.erase(vmId) > 0) {
        Logger::info("Removed backup schedule for VM " + vmId);
        return true;
    }
    return false;
}

bool BackupScheduler::updateSchedule(const std::string& vmId, const BackupConfig& config) {
    if (!config.scheduled) {
        return removeSchedule(vmId);
    }

    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        return addSchedule(vmId, config);
    }

    it->second.config = config;
    it->second.nextRun = calculateNextRun(config);
    Logger::info("Updated backup schedule for VM " + vmId);
    return true;
}

bool BackupScheduler::getSchedule(const std::string& vmId, BackupConfig& config) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        return false;
    }

    config = it->second.config;
    return true;
}

bool BackupScheduler::applyRetentionPolicy(const std::string& vmId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        return false;
    }

    const auto& config = it->second.config;
    if (config.retentionDays <= 0) {
        return true;  // No retention policy
    }

    return cleanupOldBackups(vmId);
}

bool BackupScheduler::cleanupOldBackups(const std::string& vmId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        return false;
    }

    const auto& config = it->second.config;
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

    return true;
}

void BackupScheduler::start() {
    if (running_) {
        return;
    }

    running_ = true;
    schedulerThread_ = std::thread(&BackupScheduler::schedulerLoop, this);
    Logger::info("Backup scheduler started");
}

void BackupScheduler::stop() {
    if (!running_) {
        return;
    }

    running_ = false;
    if (schedulerThread_.joinable()) {
        schedulerThread_.join();
    }
    Logger::info("Backup scheduler stopped");
}

void BackupScheduler::schedulerLoop() {
    while (running_) {
        checkSchedules();
        std::this_thread::sleep_for(std::chrono::seconds(60));  // Check every minute
    }
}

void BackupScheduler::checkSchedules() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();

    for (auto& [vmId, schedule] : schedules_) {
        if (shouldRunBackup(schedule)) {
            // TODO: Trigger backup job
            Logger::info("Scheduled backup triggered for VM " + vmId);
            
            schedule.lastRun = now;
            updateNextRun(schedule);
        }
    }
}

bool BackupScheduler::shouldRunBackup(const Schedule& schedule) const {
    auto now = std::chrono::system_clock::now();
    return schedule.config.scheduled && now >= schedule.nextRun;
}

void BackupScheduler::updateNextRun(Schedule& schedule) {
    schedule.nextRun = calculateNextRun(schedule.config);
}

std::chrono::system_clock::time_point BackupScheduler::calculateNextRun(const BackupConfig& config) const {
    auto now = std::chrono::system_clock::now();
    
    if (!config.scheduled) {
        return now;
    }

    // If scheduled time is in the past, start from next interval
    if (config.scheduledTime < now) {
        auto elapsed = now - config.scheduledTime;
        auto intervals = std::chrono::duration_cast<std::chrono::seconds>(elapsed) / config.interval;
        return config.scheduledTime + (intervals + 1) * config.interval;
    }

    return config.scheduledTime;
}

bool BackupScheduler::isBackupExpired(const std::string& backupPath, int retentionDays) const {
    try {
        auto lastWriteTime = std::filesystem::last_write_time(backupPath);
        auto now = std::chrono::system_clock::now();
        auto fileTime = std::chrono::clock_cast<std::chrono::system_clock>(lastWriteTime);
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - fileTime);
        
        return age > std::chrono::hours(24 * retentionDays);
    } catch (const std::filesystem::filesystem_error& e) {
        Logger::error("Failed to check backup age for " + backupPath + ": " + e.what());
        return false;
    }
}

std::vector<std::string> BackupScheduler::getBackupPaths(const std::string& vmId) const {
    std::vector<std::string> paths;
    try {
        auto it = schedules_.find(vmId);
        if (it == schedules_.end()) {
            return paths;
        }

        const auto& backupDir = it->second.config.backupDir;
        for (const auto& entry : std::filesystem::directory_iterator(backupDir)) {
            if (entry.is_directory() && entry.path().filename().string().find(vmId) == 0) {
                paths.push_back(entry.path().string());
            }
        }
    } catch (const std::filesystem::filesystem_error& e) {
        Logger::error("Failed to get backup paths for VM " + vmId + ": " + e.what());
    }
    return paths;
}

void BackupScheduler::getAllSchedules(std::vector<std::pair<std::string, BackupConfig>>& schedules) const {
    std::lock_guard<std::mutex> lock(mutex_);
    schedules.clear();
    schedules.reserve(schedules_.size());
    
    for (const auto& [vmId, schedule] : schedules_) {
        schedules.emplace_back(vmId, schedule.config);
    }
}

std::chrono::system_clock::time_point BackupScheduler::getNextRunTime(const std::string& vmId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        return std::chrono::system_clock::time_point::min();
    }
    
    return it->second.nextRun;
}

} // namespace vmware 