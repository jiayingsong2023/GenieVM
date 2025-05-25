#include "backup/backup_scheduler.hpp"
#include "backup/backup_job.hpp"
#include "backup/job_manager.hpp"
#include "backup/vm_config.hpp"
#include "common/logger.hpp"
#include "common/backup_status.hpp"
#include "common/parallel_task_manager.hpp"
#include <chrono>
#include <thread>
#include <filesystem>
#include <algorithm>
#include <sstream>
#include <iomanip>

using namespace std::chrono_literals;

BackupScheduler::BackupScheduler(std::shared_ptr<JobManager> jobManager,
                               std::shared_ptr<BackupProvider> provider)
    : jobManager_(std::move(jobManager))
    , provider_(std::move(provider))
    , running_(false)
    , stopRequested_(false) {
}

BackupScheduler::~BackupScheduler() {
    stop();
}

bool BackupScheduler::initialize() {
    if (!jobManager_) {
        Logger::error("No job manager provided");
        return false;
    }
    if (!provider_) {
        Logger::error("No backup provider provided");
        return false;
    }
    return true;
}

bool BackupScheduler::scheduleBackup(const std::string& vmId, const BackupConfig& config) {
    if (!jobManager_) {
        Logger::error("No job manager provided");
        return false;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    schedules_[vmId] = config;
    return true;
}

bool BackupScheduler::cancelBackup(const std::string& backupId) {
    std::lock_guard<std::mutex> lock(mutex_);
    return schedules_.erase(backupId) > 0;
}

bool BackupScheduler::pauseBackup(const std::string& backupId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = schedules_.find(backupId);
    if (it != schedules_.end()) {
        // TODO: Implement pause functionality
        return true;
    }
    return false;
}

bool BackupScheduler::resumeBackup(const std::string& backupId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = schedules_.find(backupId);
    if (it != schedules_.end()) {
        // TODO: Implement resume functionality
        return true;
    }
    return false;
}

std::vector<BackupConfig> BackupScheduler::getScheduledBackups() const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    std::vector<BackupConfig> configs;
    configs.reserve(schedules_.size());
    for (const auto& [_, config] : schedules_) {
        configs.push_back(config);
    }
    return configs;
}

BackupConfig BackupScheduler::getBackupConfig(const std::string& backupId) const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    auto it = schedules_.find(backupId);
    if (it != schedules_.end()) {
        return it->second;
    }
    return BackupConfig{};
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
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        Logger::error("No schedule found for VM: " + vmId);
        return;
    }

    const auto& config = it->second;
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

    const auto& config = it->second;
    if (config.retentionDays <= 0) {
        return;
    }

    try {
        auto backupPaths = getBackupPaths(vmId);
        auto now = std::chrono::system_clock::now();
        auto retentionPeriod = std::chrono::hours(24 * config.retentionDays);

        for (const auto& path : backupPaths) {
            auto lastWriteTime = std::filesystem::last_write_time(path);
            auto fileTime = std::chrono::system_clock::from_time_t(
                std::chrono::system_clock::to_time_t(
                    std::chrono::system_clock::now() - 
                    std::chrono::duration_cast<std::chrono::system_clock::duration>(
                        lastWriteTime.time_since_epoch()
                    )
                )
            );
            
            if (now - fileTime > retentionPeriod) {
                try {
                    std::filesystem::remove_all(path);
                    Logger::info("Removed old backup: " + path);
                } catch (const std::exception& e) {
                    Logger::error("Failed to remove old backup " + path + ": " + e.what());
                }
            }
        }
    } catch (const std::exception& e) {
        Logger::error("Error cleaning up old backups: " + std::string(e.what()));
    }
}

std::vector<std::string> BackupScheduler::getBackupPaths(const std::string& vmId) const {
    std::vector<std::string> paths;
    try {
        auto backupDir = std::filesystem::path(vmId);
        if (std::filesystem::exists(backupDir)) {
            for (const auto& entry : std::filesystem::directory_iterator(backupDir)) {
                if (entry.is_directory()) {
                    paths.push_back(entry.path().string());
                }
            }
        }
    } catch (const std::exception& e) {
        Logger::error("Failed to read backup directory: " + std::string(e.what()));
    }
    return paths;
}

bool BackupScheduler::isBackupExpired(const std::string& vmId, int retentionDays) const {
    std::lock_guard<std::mutex> lock(const_cast<std::mutex&>(mutex_));
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        return false;
    }

    try {
        auto backupPaths = getBackupPaths(vmId);
        if (backupPaths.empty()) {
            return true;
        }

        auto now = std::chrono::system_clock::now();
        auto retentionPeriod = std::chrono::hours(24 * retentionDays);

        for (const auto& path : backupPaths) {
            auto lastWriteTime = std::filesystem::last_write_time(path);
            auto fileTime = std::chrono::system_clock::from_time_t(
                std::chrono::system_clock::to_time_t(
                    std::chrono::system_clock::now() - 
                    std::chrono::duration_cast<std::chrono::system_clock::duration>(
                        lastWriteTime.time_since_epoch()
                    )
                )
            );
            
            if (now - fileTime <= retentionPeriod) {
                return false;
            }
        }
        return true;
    } catch (const std::exception& e) {
        Logger::error("Error checking backup expiration: " + std::string(e.what()));
        return false;
    }
}

bool BackupScheduler::isBackupNeeded(const std::string& vmId, const BackupConfig& config) const {
    if (!provider_) {
        return false;
    }

    try {
        std::vector<std::pair<uint64_t, uint64_t>> changedBlocks;
        if (provider_->getChangedBlocks(vmId, "", changedBlocks)) {
            return !changedBlocks.empty();
        }
        return true;
    } catch (const std::exception& e) {
        Logger::error("Error checking if backup is needed: " + std::string(e.what()));
        return false;
    }
}

std::string BackupScheduler::getBackupPath(const std::string& vmId, const BackupConfig& config) const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::stringstream ss;
    ss << config.backupDir << "/" << vmId << "_" << std::put_time(std::localtime(&time), "%Y%m%d_%H%M%S");
    return ss.str();
}

void BackupScheduler::start() {
    if (running_) {
        return;
    }

    running_ = true;
    stopRequested_ = false;
    schedulerThread_ = std::thread([this]() {
        while (!stopRequested_) {
            checkSchedules();
            std::this_thread::sleep_for(60s);
        }
    });
}

void BackupScheduler::stop() {
    if (!running_) {
        return;
    }

    stopRequested_ = true;
    if (schedulerThread_.joinable()) {
        schedulerThread_.join();
    }
    running_ = false;
}

void BackupScheduler::checkSchedules() {
    std::lock_guard<std::mutex> lock(mutex_);
    auto now = std::chrono::system_clock::now();

    for (const auto& [vmId, config] : schedules_) {
        if (shouldRunBackup(config)) {
            executeBackup(vmId, config);
        }
    }
}

bool BackupScheduler::shouldRunBackup(const BackupConfig& config) const {
    auto now = std::chrono::system_clock::now();
    auto nextRun = getNextRunTime(config);
    return now >= nextRun;
}

void BackupScheduler::executeBackup(const std::string& vmId, const BackupConfig& config) {
    try {
        // Create a task manager for the backup job
        auto taskManager = std::make_shared<ParallelTaskManager>();
        auto job = std::make_shared<BackupJob>(provider_, taskManager, config);
        
        // Use the job manager to manage the job
        jobManager_->addJob(job);
        
        job->setProgressCallback([this](int progress) {
            // TODO: Implement progress tracking
        });

        job->setStatusCallback([this](const std::string& status) {
            // TODO: Implement status tracking
        });

        if (!job->start()) {
            Logger::error("Failed to start backup job for VM: " + vmId);
            return;
        }

        // Wait for job completion
        while (job->getState() == Job::State::RUNNING) {
            std::this_thread::sleep_for(1s);
        }

        if (job->getState() == Job::State::FAILED) {
            Logger::error("Backup job failed for VM: " + vmId);
        }
    } catch (const std::exception& e) {
        Logger::error("Error executing backup: " + std::string(e.what()));
    }
}

std::chrono::system_clock::time_point BackupScheduler::getNextRunTime(const BackupConfig& config) const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    std::tm* tm = std::localtime(&time);

    if (config.scheduleType == "daily") {
        if (config.schedule.hour == 0 && config.schedule.minute == 0) {
            return now + std::chrono::hours(24);
        }
        
        tm->tm_hour = config.schedule.hour;
        tm->tm_min = config.schedule.minute;
        tm->tm_sec = 0;
        auto next = std::chrono::system_clock::from_time_t(std::mktime(tm));
        if (next <= now) {
            next += std::chrono::hours(24);
        }
        return next;
    }
    else if (config.scheduleType == "weekly") {
        if (config.schedule.day < 0 || config.schedule.day > 6) {
            return now + std::chrono::hours(24 * 7);
        }
        
        int daysUntilNext = (config.schedule.day - tm->tm_wday + 7) % 7;
        if (daysUntilNext == 0 && config.schedule.hour == 0 && config.schedule.minute == 0) {
            daysUntilNext = 7;
        }
        return now + std::chrono::hours(24 * daysUntilNext);
    }
    else if (config.scheduleType == "interval") {
        // Default to 24 hours if no interval is specified
        return now + std::chrono::hours(24);
    }
    
    // Default case
    return now + std::chrono::hours(24);
} 