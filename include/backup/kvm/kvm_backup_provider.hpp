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

    // Initialization
    bool initialize();

    // Connection management
    bool connect(const std::string& host, const std::string& username, const std::string& password) override;
    void disconnect() override;
    bool isConnected() const override;

    // VM operations
    std::vector<std::string> listVMs() const;
    bool getVMInfo(const std::string& vmId, std::string& name, std::string& status) const;
    bool getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths) const override;
    bool backupDisk(const std::string& vmId, const std::string& diskPath, const BackupConfig& config) override;
    bool restoreDisk(const std::string& vmId, const std::string& diskPath, const RestoreConfig& config) override;
    bool verifyDisk(const std::string& diskPath) override;
    bool getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                         std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) override;

    // CBT operations
    bool enableCBT(const std::string& vmId);
    bool disableCBT(const std::string& vmId);
    bool isCBTEnabled(const std::string& vmId) const;

    // Backup management
    bool listBackups(std::vector<std::string>& backupIds) override;
    bool deleteBackup(const std::string& backupId) override;
    bool verifyBackup(const std::string& backupId) override;

    // Error handling
    std::string getLastError() const override { return lastError_; }
    void clearLastError() override;

    // Progress tracking
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
    bool verifyDiskIntegrity(const std::string& diskPath);
    std::string calculateChecksum(const std::string& filePath);
    bool createSnapshot(const std::string& vmId, const std::string& snapshotId);
    bool removeSnapshot(const std::string& vmId, const std::string& snapshotId);
}; 