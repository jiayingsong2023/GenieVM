#include "backup/backup_scheduler.hpp"
#include "common/job_manager.hpp"
#include "common/job.hpp"
#include "common/logger.hpp"
#include <chrono>
#include <thread>
#include <filesystem>
#include <algorithm>
#include <ctime>
#include <iomanip>
#include <sstream>

BackupScheduler::BackupScheduler(std::shared_ptr<JobManager> jobManager)
    : jobManager_(jobManager)
    , running_(false)
    , stopRequested_(false) {
}

BackupScheduler::~BackupScheduler() {
    stop();
}

bool BackupScheduler::initialize() {
    if (running_) {
        return true;
    }

    running_ = true;
    stopRequested_ = false;
    schedulerThread_ = std::thread(&BackupScheduler::checkSchedules, this);
    return true;
}

bool BackupScheduler::scheduleBackup(const std::string& vmId, const BackupConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    schedules_[vmId] = config;
    return true;
}

bool BackupScheduler::cancelBackup(const std::string& backupId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = schedules_.find(backupId);
    if (it != schedules_.end()) {
        schedules_.erase(it);
        return true;
    }
    return false;
}

bool BackupScheduler::pauseBackup(const std::string& backupId) {
    // TODO: Implement pause functionality in JobManager
    Logger::warning("Pause functionality not implemented");
    return false;
}

bool BackupScheduler::resumeBackup(const std::string& backupId) {
    // TODO: Implement resume functionality in JobManager
    Logger::warning("Resume functionality not implemented");
    return false;
}

std::vector<BackupConfig> BackupScheduler::getScheduledBackups() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<BackupConfig> result;
    for (const auto& pair : schedules_) {
        result.push_back(pair.second);
    }
    return result;
}

BackupConfig BackupScheduler::getBackupConfig(const std::string& backupId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = schedules_.find(backupId);
    if (it != schedules_.end()) {
        return it->second;
    }
    return BackupConfig();
}

std::chrono::system_clock::time_point BackupScheduler::getNextRunTime(const BackupConfig& config) const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_time_t);

    std::tm next_tm = *now_tm;
    next_tm.tm_hour = config.schedule.hour;
    next_tm.tm_min = config.schedule.minute;
    next_tm.tm_sec = 0;

    auto next_time = std::mktime(&next_tm);
    auto next_time_point = std::chrono::system_clock::from_time_t(next_time);

    if (next_time_point <= now) {
        if (config.scheduleType == "daily") {
            next_time_point += std::chrono::hours(24);
        } else if (config.scheduleType == "weekly") {
            next_time_point += std::chrono::hours(24 * 7);
        } else if (config.scheduleType == "monthly") {
            next_tm.tm_mon++;
            next_time = std::mktime(&next_tm);
            next_time_point = std::chrono::system_clock::from_time_t(next_time);
        }
    }

    return next_time_point;
}

void BackupScheduler::addSchedule(const std::string& vmId, const BackupConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    schedules_[vmId] = config;
}

void BackupScheduler::removeSchedule(const std::string& vmId) {
    std::lock_guard<std::mutex> lock(mutex_);
    schedules_.erase(vmId);
}

void BackupScheduler::updateSchedule(const std::string& vmId, const BackupConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    schedules_[vmId] = config;
}

void BackupScheduler::applyRetentionPolicy(const std::string& vmId) {
    auto config = getBackupConfig(vmId);
    if (config.retentionDays > 0) {
        cleanupOldBackups(vmId);
    }
}

void BackupScheduler::cleanupOldBackups(const std::string& vmId) {
    auto config = getBackupConfig(vmId);
    auto backupPaths = getBackupPaths(vmId);
    
    // Sort backups by date (newest first)
    std::sort(backupPaths.begin(), backupPaths.end(), std::greater<>());
    
    // Keep only the most recent backups up to maxBackups
    if (config.maxBackups > 0 && backupPaths.size() > config.maxBackups) {
        for (size_t i = config.maxBackups; i < backupPaths.size(); ++i) {
            std::filesystem::remove(backupPaths[i]);
        }
    }
}

std::vector<std::string> BackupScheduler::getBackupPaths(const std::string& vmId) const {
    auto config = getBackupConfig(vmId);
    std::vector<std::string> paths;
    
    for (const auto& entry : std::filesystem::directory_iterator(config.backupDir)) {
        if (entry.path().string().find(vmId) != std::string::npos) {
            paths.push_back(entry.path().string());
        }
    }
    
    return paths;
}

bool BackupScheduler::isBackupExpired(const std::string& vmId, int retentionDays) const {
    auto backupPaths = getBackupPaths(vmId);
    if (backupPaths.empty()) {
        return false;
    }

    auto now = std::chrono::system_clock::now();
    auto oldestBackup = std::filesystem::last_write_time(backupPaths.back());
    
    // Convert file_time_type to system_clock::time_point
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        oldestBackup - std::filesystem::file_time_type::clock::now()
        + std::chrono::system_clock::now());
    
    auto age = std::chrono::duration_cast<std::chrono::hours>(now - sctp);
    return age.count() > (retentionDays * 24);
}

bool BackupScheduler::isBackupNeeded(const std::string& vmId, const BackupConfig& config) const {
    if (config.scheduleType == "once") {
        return true;
    }

    auto lastBackup = getLastBackupTime(vmId);
    if (!lastBackup) {
        return true;
    }

    auto now = std::chrono::system_clock::now();
    auto timeSinceLastBackup = std::chrono::duration_cast<std::chrono::hours>(now - *lastBackup);

    if (config.scheduleType == "daily") {
        return timeSinceLastBackup.count() >= 24;
    } else if (config.scheduleType == "weekly") {
        return timeSinceLastBackup.count() >= 24 * 7;
    } else if (config.scheduleType == "monthly") {
        return timeSinceLastBackup.count() >= 24 * 30;
    }

    return false;
}

std::string BackupScheduler::getBackupPath(const std::string& vmId, const BackupConfig& config) const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_time_t);
    
    std::stringstream ss;
    ss << config.backupDir << "/" << vmId << "_"
       << std::put_time(now_tm, "%Y%m%d_%H%M%S");
    
    return ss.str();
}

void BackupScheduler::start() {
    if (!running_) {
        running_ = true;
        stopRequested_ = false;
        schedulerThread_ = std::thread(&BackupScheduler::checkSchedules, this);
    }
}

void BackupScheduler::stop() {
    if (running_) {
        stopRequested_ = true;
        if (schedulerThread_.joinable()) {
            schedulerThread_.join();
        }
        running_ = false;
    }
}

void BackupScheduler::checkSchedules() {
    while (!stopRequested_) {
        std::vector<std::pair<std::string, BackupConfig>> jobsToRun;
        
        {
            std::lock_guard<std::mutex> lock(mutex_);
            for (const auto& pair : schedules_) {
                if (shouldRunBackup(pair.second)) {
                    jobsToRun.push_back(pair);
                }
            }
        }
        
        for (const auto& job : jobsToRun) {
            executeBackup(job.first, job.second);
        }
        
        std::this_thread::sleep_for(std::chrono::minutes(1));
    }
}

void BackupScheduler::executeBackup(const std::string& vmId, const BackupConfig& config) {
    try {
        auto job = jobManager_->createBackupJob(config);
        if (job) {
            job->start();
        }
    } catch (const std::exception& e) {
        Logger::error("Failed to execute backup for VM " + vmId + ": " + e.what());
    }
}

bool BackupScheduler::shouldRunBackup(const BackupConfig& config) const {
    auto now = std::chrono::system_clock::now();
    auto now_time_t = std::chrono::system_clock::to_time_t(now);
    std::tm* now_tm = std::localtime(&now_time_t);
    
    if (config.scheduleType == "once") {
        return now_tm->tm_hour == config.schedule.hour && 
               now_tm->tm_min == config.schedule.minute;
    } else if (config.scheduleType == "daily") {
        return now_tm->tm_hour == config.schedule.hour && 
               now_tm->tm_min == config.schedule.minute;
    } else if (config.scheduleType == "weekly") {
        return now_tm->tm_wday == config.schedule.day &&
               now_tm->tm_hour == config.schedule.hour && 
               now_tm->tm_min == config.schedule.minute;
    } else if (config.scheduleType == "monthly") {
        return now_tm->tm_mday == config.schedule.day &&
               now_tm->tm_hour == config.schedule.hour && 
               now_tm->tm_min == config.schedule.minute;
    }
    
    return false;
}

std::optional<std::chrono::system_clock::time_point> BackupScheduler::getLastBackupTime(const std::string& vmId) const {
    auto backupPaths = getBackupPaths(vmId);
    if (backupPaths.empty()) {
        return std::nullopt;
    }

    // Get the most recent backup
    auto lastBackup = std::filesystem::last_write_time(backupPaths.front());
    
    // Convert file_time_type to system_clock::time_point
    auto sctp = std::chrono::time_point_cast<std::chrono::system_clock::duration>(
        lastBackup - std::filesystem::file_time_type::clock::now()
        + std::chrono::system_clock::now());
    
    return sctp;
} 