#pragma once

#include "backup/backup_provider.hpp"
#include "backup/backup_job.hpp"
#include "common/vmware_connection.hpp"
#include "common/backup_status.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <vixDiskLib.h>
#include <unordered_map>
#include <set>

// Type definitions
using ProgressCallback = std::function<void(int)>;
using StatusCallback = std::function<void(const std::string&)>;

class VMwareBackupProvider : public BackupProvider, public std::enable_shared_from_this<VMwareBackupProvider> {
public:
    explicit VMwareBackupProvider(std::shared_ptr<VMwareConnection> connection);
    ~VMwareBackupProvider() override;

    // Connection management
    bool initialize() override;
    bool connect(const std::string& host, const std::string& username, const std::string& password) override;
    void disconnect() override;
    bool isConnected() const override;

    // VM management
    std::vector<std::string> listVMs() const override;
    bool getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths) const override;
    bool getVMInfo(const std::string& vmId, std::string& name, std::string& status) const override;

    // Backup operations
    bool startBackup(const std::string& vmId, const BackupConfig& config) override;
    bool cancelBackup(const std::string& vmId) override;
    bool pauseBackup(const std::string& vmId) override;
    bool resumeBackup(const std::string& vmId) override;
    BackupStatus getBackupStatus(const std::string& vmId) override;

    // Restore operations
    bool startRestore(const std::string& vmId, const std::string& backupId) override;
    bool cancelRestore(const std::string& restoreId) override;
    bool pauseRestore(const std::string& restoreId) override;
    bool resumeRestore(const std::string& restoreId) override;
    RestoreStatus getRestoreStatus(const std::string& restoreId) const override;

    // CBT operations
    bool enableCBT(const std::string& vmId) override;
    bool disableCBT(const std::string& vmId) override;
    bool isCBTEnabled(const std::string& vmId) const override;
    bool getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                         std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) const override;

    // Callbacks
    void setProgressCallback(ProgressCallback callback) override;
    void setStatusCallback(StatusCallback callback) override;

    // Error handling
    std::string getLastError() const override;
    void clearLastError() override;

    // Additional methods
    bool verifyBackup(const std::string& backupId) override;
    double getProgress() const override;

private:
    std::shared_ptr<VMwareConnection> connection_;
    double progress_;
    mutable std::string lastError_;
    ProgressCallback progressCallback_;
    StatusCallback statusCallback_;
    std::unordered_map<std::string, std::shared_ptr<BackupJob>> activeOperations_;
    mutable std::mutex mutex_;

    // Snapshot management
    std::string currentSnapshotName_;
    std::string currentVmId_;

    // Helper methods
    bool verifyConnection();
    void cleanupActiveOperations();
    bool initializeVDDK();
    void updateProgress(double progress, const std::string& status);
    bool saveBackupMetadata(const std::string& backupId, const std::string& vmId,
                           const std::vector<std::string>& diskPaths);
    std::optional<BackupMetadata> getLatestBackupInfo(const std::string& vmId);
    std::string calculateChecksum(const std::string& filePath);
    bool verifyBackupIntegrity(const std::string& backupId);
    bool backupDisk(const std::string& vmId, const std::string& diskPath, const std::string& backupPath);
    bool restoreDisk(const std::string& vmId, const std::string& diskPath, const std::string& backupPath);
    bool verifyRestore(const std::string& vmId, const std::string& backupId);

    // Snapshot management methods
    bool createSnapshot(const std::string& vmId);
    bool removeSnapshot();
    void cleanupSnapshot();
}; 