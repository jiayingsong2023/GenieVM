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
    virtual bool initialize() = 0;
    virtual bool connect(const std::string& host, const std::string& username, const std::string& password) = 0;
    virtual void disconnect() = 0;
    virtual bool isConnected() const = 0;

    // VM management
    virtual std::vector<std::string> listVMs() const = 0;
    virtual bool getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths) const = 0;
    virtual bool getVMInfo(const std::string& vmId, std::string& name, std::string& status) const = 0;

    // Backup operations
    virtual bool startBackup(const std::string& vmId, const BackupConfig& config) = 0;
    virtual bool cancelBackup(const std::string& vmId) = 0;
    virtual bool pauseBackup(const std::string& vmId) = 0;
    virtual bool resumeBackup(const std::string& vmId) = 0;
    virtual BackupStatus getBackupStatus(const std::string& vmId) = 0;

    // Restore operations
    virtual bool startRestore(const std::string& vmId, const std::string& backupId) = 0;
    virtual bool cancelRestore(const std::string& restoreId) = 0;
    virtual bool pauseRestore(const std::string& restoreId) = 0;
    virtual bool resumeRestore(const std::string& restoreId) = 0;
    virtual RestoreStatus getRestoreStatus(const std::string& restoreId) const = 0;

    // CBT operations
    virtual bool enableCBT(const std::string& vmId) = 0;
    virtual bool disableCBT(const std::string& vmId) = 0;
    virtual bool isCBTEnabled(const std::string& vmId) const = 0;
    virtual bool getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                                std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) const = 0;

    // Callbacks
    virtual void setProgressCallback(ProgressCallback callback) = 0;
    virtual void setStatusCallback(StatusCallback callback) = 0;

    // Error handling
    virtual std::string getLastError() const = 0;
    virtual void clearLastError() = 0;

    // Additional methods
    virtual bool verifyBackup(const std::string& backupId) = 0;
    virtual double getProgress() const = 0;
}; 