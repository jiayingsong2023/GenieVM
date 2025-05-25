#pragma once

#include "common/backup_status.hpp"
#include "backup/vm_config.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>

// Type definitions
using ProgressCallback = std::function<void(int)>;
using StatusCallback = std::function<void(const std::string&)>;

class BackupProvider {
public:
    virtual ~BackupProvider() = default;

    // Connection management
    virtual bool connect(const std::string& host, const std::string& username, const std::string& password) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    // VM operations
    virtual bool getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths) const = 0;
    virtual bool backupDisk(const std::string& vmId, const std::string& diskPath, const BackupConfig& config) = 0;
    virtual bool restoreDisk(const std::string& vmId, const std::string& diskPath, const RestoreConfig& config) = 0;
    virtual bool verifyDisk(const std::string& diskPath) = 0;
    virtual bool getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                                std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) = 0;

    // Backup management
    virtual bool listBackups(std::vector<std::string>& backupIds) = 0;
    virtual bool deleteBackup(const std::string& backupId) = 0;
    virtual bool verifyBackup(const std::string& backupId) = 0;

    // Error handling
    virtual std::string getLastError() const = 0;
    virtual void clearLastError() = 0;

    // Progress tracking
    virtual double getProgress() const = 0;
}; 