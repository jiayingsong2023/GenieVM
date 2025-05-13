#include "backup/backup_manager.hpp"
#include "common/logger.hpp"
#include "common/thread_utils.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <ctime>
#include <iomanip>

namespace vmware {

BackupManager::BackupManager(const std::string& host,
                           const std::string& username,
                           const std::string& password,
                           size_t maxConcurrentBackups)
    : host_(host)
    , username_(username)
    , password_(password)
    , initialized_(false)
{
    connection_ = std::make_unique<VMwareConnection>(host, username, password);
    scheduler_ = std::make_unique<Scheduler>();
    taskManager_ = std::make_unique<ParallelTaskManager>(maxConcurrentBackups);
}

BackupManager::~BackupManager() {
    if (initialized_) {
        stopScheduler();
        connection_->disconnect();
    }
}

bool BackupManager::initialize() {
    if (!connection_->connect()) {
        Logger::error("Failed to connect to vCenter");
        return false;
    }
    initialized_ = true;
    return true;
}

bool BackupManager::backupVM(const std::string& vmName,
                           const std::string& backupDir,
                           bool useCBT) {
    if (!initialized_) {
        Logger::error("BackupManager not initialized");
        return false;
    }

    // Create backup directory
    if (!createBackupDirectory(backupDir)) {
        return false;
    }

    // Create snapshot before backup
    if (!createBackupSnapshot(vmName)) {
        return false;
    }

    bool backupSuccess = false;
    try {
        // Enable CBT if requested
        if (useCBT && !enableCBT(vmName)) {
            Logger::error("Failed to enable CBT for VM: " + vmName);
            return false;
        }

        // Get VM disk paths
        std::vector<std::string> diskPaths;
        if (!connection_->getVMDiskPaths(vmName, diskPaths)) {
            Logger::error("Failed to get disk paths for VM: " + vmName);
            return false;
        }

        // Create a vector to store futures
        std::vector<std::future<void>> futures;

        // Backup each disk in parallel
        for (size_t i = 0; i < diskPaths.size(); ++i) {
            const auto& diskPath = diskPaths[i];
            std::string backupPath = backupDir + "/disk" + std::to_string(i) + ".vmdk";
            
            // Add backup task to parallel task manager
            futures.push_back(taskManager_->addTask(
                "backup_" + vmName + "_disk" + std::to_string(i),
                [this, diskPath, backupPath, useCBT]() {
                    DiskBackup diskBackup(diskPath, backupPath);
                    if (!diskBackup.initialize()) {
                        throw std::runtime_error("Failed to initialize disk backup for: " + diskPath);
                    }

                    if (useCBT) {
                        if (!diskBackup.backupIncremental()) {
                            throw std::runtime_error("Failed to perform incremental backup for: " + diskPath);
                        }
                    } else {
                        if (!diskBackup.backupFull()) {
                            throw std::runtime_error("Failed to perform full backup for: " + diskPath);
                        }
                    }
                }
            ));
        }

        // Wait for all backup tasks to complete
        for (auto& future : futures) {
            future.get();
        }

        // Disable CBT if it was enabled
        if (useCBT && !disableCBT(vmName)) {
            Logger::warning("Failed to disable CBT for VM: " + vmName);
        }

        backupSuccess = true;
    } catch (const std::exception& e) {
        Logger::error("Exception during backup: " + std::string(e.what()));
    }

    // Remove snapshot after backup (whether successful or not)
    if (!removeBackupSnapshot(vmName)) {
        Logger::warning("Failed to remove backup snapshot for VM: " + vmName);
    }

    return backupSuccess;
}

bool BackupManager::scheduleBackup(const std::string& vmName,
                                 const std::string& backupDir,
                                 const TimePoint& scheduledTime,
                                 bool useCBT) {
    if (!initialized_) {
        Logger::error("BackupManager not initialized");
        return false;
    }

    std::string taskId = generateTaskId(vmName);
    return scheduler_->scheduleTask(taskId, scheduledTime,
        [this, vmName, backupDir, useCBT]() {
            backupVM(vmName, backupDir, useCBT);
        });
}

bool BackupManager::schedulePeriodicBackup(const std::string& vmName,
                                         const std::string& backupDir,
                                         const Duration& interval,
                                         bool useCBT) {
    if (!initialized_) {
        Logger::error("BackupManager not initialized");
        return false;
    }

    std::string taskId = generateTaskId(vmName);
    return scheduler_->schedulePeriodicTask(taskId, interval,
        [this, vmName, backupDir, useCBT]() {
            backupVM(vmName, backupDir, useCBT);
        });
}

bool BackupManager::cancelScheduledBackup(const std::string& taskId) {
    return scheduler_->cancelTask(taskId);
}

void BackupManager::startScheduler() {
    scheduler_->start();
}

void BackupManager::stopScheduler() {
    scheduler_->stop();
}

std::string BackupManager::generateTaskId(const std::string& vmName) const {
    time_t now = vmware::thread_utils::get_current_time();
    std::stringstream ss;
    ss << "backup_" << vmName << "_" << std::put_time(std::localtime(&now), "%Y%m%d_%H%M%S");
    return ss.str();
}

std::vector<std::string> BackupManager::getAvailableVMs() {
    std::vector<std::string> vms;
    if (!initialized_) {
        Logger::error("BackupManager not initialized");
        return vms;
    }

    // Implementation to get list of VMs
    // This would typically use the vSphere API to enumerate VMs
    return vms;
}

bool BackupManager::enableCBT(const std::string& vmName) {
    return connection_->enableCBT(vmName);
}

bool BackupManager::disableCBT(const std::string& vmName) {
    return connection_->disableCBT(vmName);
}

bool BackupManager::createBackupDirectory(const std::string& backupDir) {
    try {
        std::filesystem::create_directories(backupDir);
        return true;
    } catch (const std::filesystem::filesystem_error& e) {
        Logger::error("Failed to create backup directory: " + std::string(e.what()));
        return false;
    }
}

void BackupManager::logBackupProgress(const std::string& message) {
    Logger::info(message);
}

std::string BackupManager::getBackupSnapshotName(const std::string& vmName) const {
    return "backup_" + vmName + "_" + std::to_string(std::time(nullptr));
}

bool BackupManager::createBackupSnapshot(const std::string& vmName) {
    std::string snapshotName = getBackupSnapshotName(vmName);
    std::string description = "Snapshot created for backup of " + vmName;
    
    if (!connection_->createSnapshot(vmName, snapshotName, description)) {
        Logger::error("Failed to create backup snapshot for VM: " + vmName);
        return false;
    }
    
    return true;
}

bool BackupManager::removeBackupSnapshot(const std::string& vmName) {
    std::string snapshotName = getBackupSnapshotName(vmName);
    
    if (!connection_->removeSnapshot(vmName, snapshotName)) {
        Logger::warning("Failed to remove backup snapshot for VM: " + vmName);
        return false;
    }
    
    return true;
}

} // namespace vmware 