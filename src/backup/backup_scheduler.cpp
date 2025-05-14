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

namespace vmware {

BackupScheduler::BackupScheduler(std::shared_ptr<BackupManager> backupManager)
    : backupManager_(std::move(backupManager)) {
}

BackupScheduler::~BackupScheduler() = default;

void BackupScheduler::addSchedule(const std::string& vmId, const BackupConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // Check if schedule already exists
    if (schedules_.find(vmId) != schedules_.end()) {
        throw std::runtime_error("Schedule already exists for VM: " + vmId);
    }

    // Validate schedule time
    if (!config.scheduled || config.scheduledTime.empty()) {
        throw std::runtime_error("Invalid schedule time for VM: " + vmId);
    }

    // Add schedule
    schedules_[vmId] = config;
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
    if (!config.scheduled || config.scheduledTime.empty()) {
        throw std::runtime_error("Invalid schedule time for VM: " + vmId);
    }

    // Update schedule
    it->second = config;
    Logger::info("Updated backup schedule for VM: " + vmId);
}

BackupConfig BackupScheduler::getSchedule(const std::string& vmId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        throw std::runtime_error("No schedule found for VM: " + vmId);
    }

    return it->second;
}

void BackupScheduler::applyRetentionPolicy(const std::string& vmId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        throw std::runtime_error("No schedule found for VM: " + vmId);
    }

    const auto& config = it->second;
    if (config.retentionDays <= 0) {
        return;
    }

    // Get backup directory
    std::string backupDir = config.backupDir;
    if (!std::filesystem::exists(backupDir)) {
        Logger::error("Backup directory does not exist: " + backupDir);
        return;
    }

    // Get all backup files
    std::vector<std::filesystem::path> backupFiles;
    for (const auto& entry : std::filesystem::directory_iterator(backupDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".vmdk") {
            backupFiles.push_back(entry.path());
        }
    }

    // Sort by modification time
    std::sort(backupFiles.begin(), backupFiles.end(),
              [](const auto& a, const auto& b) {
                  return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
              });

    // Keep only the most recent backups within retention period
    int keptBackups = 0;
    for (const auto& file : backupFiles) {
        if (isBackupExpired(file.string(), config.retentionDays)) {
            std::filesystem::remove(file);
            Logger::info("Removed expired backup: " + file.string());
        } else {
            keptBackups++;
            if (keptBackups >= config.maxBackups) {
                std::filesystem::remove(file);
                Logger::info("Removed backup exceeding max count: " + file.string());
            }
        }
    }
}

void BackupScheduler::cleanupOldBackups(const std::string& vmId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    auto it = schedules_.find(vmId);
    if (it == schedules_.end()) {
        throw std::runtime_error("No schedule found for VM: " + vmId);
    }

    const auto& config = it->second;
    if (config.maxBackups <= 0) {
        return;
    }

    // Get backup directory
    std::string backupDir = config.backupDir;
    if (!std::filesystem::exists(backupDir)) {
        Logger::error("Backup directory does not exist: " + backupDir);
        return;
    }

    // Get all backup files
    std::vector<std::filesystem::path> backupFiles;
    for (const auto& entry : std::filesystem::directory_iterator(backupDir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".vmdk") {
            backupFiles.push_back(entry.path());
        }
    }

    // Sort by modification time
    std::sort(backupFiles.begin(), backupFiles.end(),
              [](const auto& a, const auto& b) {
                  return std::filesystem::last_write_time(a) > std::filesystem::last_write_time(b);
              });

    // Remove excess backups
    for (size_t i = config.maxBackups; i < backupFiles.size(); ++i) {
        std::filesystem::remove(backupFiles[i]);
        Logger::info("Removed excess backup: " + backupFiles[i].string());
    }
}

bool BackupScheduler::isBackupExpired(const std::string& backupPath, int retentionDays) const {
    try {
        auto lastWriteTime = std::filesystem::last_write_time(backupPath);
        auto now = std::chrono::system_clock::now();
        auto fileTime = std::chrono::system_clock::from_time_t(
            std::chrono::system_clock::to_time_t(lastWriteTime));
        auto age = std::chrono::duration_cast<std::chrono::hours>(now - fileTime);
        return age.count() / 24 >= retentionDays;
    } catch (const std::exception& e) {
        Logger::error("Error checking backup expiration: " + std::string(e.what()));
        return false;
    }
}

} // namespace vmware 