#include "restore/restore_manager.hpp"
#include "common/logger.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>

RestoreManager::RestoreManager(const std::string& host,
                             const std::string& username,
                             const std::string& password)
    : host_(host)
    , username_(username)
    , password_(password)
    , vmId_("")
    , initialized_(false) {
}

RestoreManager::~RestoreManager() {
    // Cleanup will be handled by unique_ptr
}

bool RestoreManager::initialize() {
    try {
        connection_ = std::make_unique<VMwareConnection>();
        if (!connection_->connect(host_, username_, password_)) {
            Logger::error("Failed to connect to vCenter");
            return false;
        }
        initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        Logger::error("Exception during initialization: " + std::string(e.what()));
        return false;
    }
}

bool RestoreManager::restoreVM(const std::string& vmName,
                             const std::string& backupDir,
                             const std::string& datastore,
                             const std::string& resourcePool) {
    if (!initialized_) {
        Logger::error("RestoreManager not initialized");
        return false;
    }

    if (!validateBackup(backupDir)) {
        Logger::error("Invalid backup directory: " + backupDir);
        return false;
    }

    try {
        // Create the VM
        if (!createVM(vmName, datastore, resourcePool)) {
            Logger::error("Failed to create VM: " + vmName);
            return false;
        }

        // Get list of disk files from backup
        std::vector<std::string> diskPaths;
        for (const auto& entry : std::filesystem::directory_iterator(backupDir)) {
            if (entry.path().extension() == ".vmdk") {
                diskPaths.push_back(entry.path().string());
            }
        }

        // Attach disks to the VM
        if (!attachDisks(vmName, diskPaths)) {
            Logger::error("Failed to attach disks to VM: " + vmName);
            return false;
        }

        Logger::info("Successfully restored VM: " + vmName);
        return true;
    } catch (const std::exception& e) {
        Logger::error("Exception during VM restore: " + std::string(e.what()));
        return false;
    }
}

std::vector<std::string> RestoreManager::getAvailableBackups(const std::string& backupDir) {
    std::vector<std::string> backups;
    try {
        for (const auto& entry : std::filesystem::directory_iterator(backupDir)) {
            if (entry.is_directory()) {
                // Check if directory contains a valid backup
                if (validateBackup(entry.path().string())) {
                    backups.push_back(entry.path().filename().string());
                }
            }
        }
    } catch (const std::exception& e) {
        Logger::error("Exception while getting available backups: " + std::string(e.what()));
    }
    return backups;
}

bool RestoreManager::createVM(const std::string& vmName,
                            const std::string& datastore,
                            const std::string& resourcePool) {
    if (!initialized_) {
        Logger::error("RestoreManager not initialized");
        return false;
    }

    try {
        // Create VM configuration
        nlohmann::json vmConfig = {
            {"name", vmName},
            {"datastore", datastore},
            {"resource_pool", resourcePool}
        };

        // Create VM
        nlohmann::json response;
        if (!connection_->createVM(vmConfig, response)) {
            Logger::error("Failed to create VM: " + connection_->getLastError());
            return false;
        }

        // Store VM ID
        vmId_ = response["value"].get<std::string>();
        return true;
    } catch (const std::exception& e) {
        Logger::error("Exception in createVM: " + std::string(e.what()));
        return false;
    }
}

bool RestoreManager::attachDisks(const std::string& vmName,
                               const std::vector<std::string>& diskPaths) {
    if (!initialized_) {
        Logger::error("RestoreManager not initialized");
        return false;
    }

    if (vmId_.empty()) {
        Logger::error("VM ID not set");
        return false;
    }

    try {
        for (const auto& diskPath : diskPaths) {
            // Create disk configuration
            nlohmann::json diskConfig = {
                {"path", diskPath},
                {"type", "scsi"}
            };

            // Attach disk
            nlohmann::json response;
            if (!connection_->attachDisk(vmId_, diskConfig, response)) {
                Logger::error("Failed to attach disk: " + connection_->getLastError());
                return false;
            }
        }
        return true;
    } catch (const std::exception& e) {
        Logger::error("Exception in attachDisks: " + std::string(e.what()));
        return false;
    }
}

bool RestoreManager::validateBackup(const std::string& backupDir) {
    try {
        // Check if backup directory exists
        if (!std::filesystem::exists(backupDir)) {
            return false;
        }

        // Check for required files
        bool hasVmx = false;
        bool hasVmdk = false;
        for (const auto& entry : std::filesystem::directory_iterator(backupDir)) {
            if (entry.path().extension() == ".vmx") {
                hasVmx = true;
            } else if (entry.path().extension() == ".vmdk") {
                hasVmdk = true;
            }
        }

        return hasVmx && hasVmdk;
    } catch (const std::exception& e) {
        Logger::error("Exception while validating backup: " + std::string(e.what()));
        return false;
    }
}

void RestoreManager::logRestoreProgress(const std::string& message) {
    Logger::info("Restore progress: " + message);
} 