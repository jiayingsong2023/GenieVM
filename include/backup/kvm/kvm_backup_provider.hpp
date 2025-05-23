#pragma once

#include "backup/backup_provider.hpp"
#include "backup/kvm/cbt_factory.hpp"
#include "backup/backup_job.hpp"
#include "backup/restore_job.hpp"
#include "common/backup_status.hpp"
#include <memory>
#include <libvirt/libvirt.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <optional>
#include <mutex>

class KVMBackupProvider : public BackupProvider, public std::enable_shared_from_this<KVMBackupProvider> {
public:
    KVMBackupProvider();
    ~KVMBackupProvider() override;

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

    // Snapshot operations
    bool createSnapshot(const std::string& vmId, const std::string& snapshotId);
    bool removeSnapshot(const std::string& vmId, const std::string& snapshotId);

    // Callbacks
    void setProgressCallback(ProgressCallback callback) override;
    void setStatusCallback(StatusCallback callback) override;

    // Error handling
    std::string getLastError() const override { return lastError_; }
    void clearLastError() override;

    // Additional methods
    bool verifyBackup(const std::string& backupId) override;
    double getProgress() const override;

private:
    virConnectPtr connection_;
    std::unique_ptr<CBTFactory> cbtFactory_;
    std::unordered_map<std::string, std::shared_ptr<BackupJob>> backupJobs_;
    std::unordered_map<std::string, std::shared_ptr<RestoreJob>> restoreJobs_;
    ProgressCallback progressCallback_;
    StatusCallback statusCallback_;
    mutable std::string lastError_;
    double progress_{0.0};
    mutable std::mutex mutex_;

    // Helper methods
    bool initializeCBT(const std::string& vmId);
    bool cleanupCBT(const std::string& vmId);
    std::string getDiskFormat(const std::string& diskPath) const;
    std::optional<BackupMetadata> getLatestBackupInfo(const std::string& backupId);
    bool verifyDiskIntegrity(const std::string& diskPath);
    std::string calculateChecksum(const std::string& filePath);
}; 