#pragma once

#include "backup/backup_provider.hpp"
#include "backup/kvm/cbt_factory.hpp"
#include "backup/backup_job.hpp"
#include "backup/restore_job.hpp"
#include "common/backup_status.hpp"
#include "backup/vm_config.hpp"
#include <memory>
#include <libvirt/libvirt.h>
#include <string>
#include <vector>
#include <map>
#include <unordered_map>
#include <optional>
#include <mutex>

class KVMBackupProvider : public BackupProvider {
public:
    KVMBackupProvider();
    ~KVMBackupProvider() override;

    bool connect(const std::string& host, const std::string& username, const std::string& password) override;
    void disconnect() override;
    bool isConnected() const override;

    bool getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths) override;
    bool createSnapshot(const std::string& vmId, std::string& snapshotId) override;
    bool removeSnapshot(const std::string& vmId, const std::string& snapshotId) override;
    bool getChangedBlocks(const std::string& vmId, const std::string& diskPath, std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) override;
    bool backupDisk(const std::string& vmId, const std::string& diskPath, const BackupConfig& config) override;
    bool verifyDisk(const std::string& diskPath) override;
    bool listBackups(std::vector<std::string>& backupDirs) override;
    bool deleteBackup(const std::string& backupDir) override;
    bool verifyBackup(const std::string& backupId) override;
    bool restoreDisk(const std::string& vmId, const std::string& diskPath, const RestoreConfig& config) override;
    std::string getLastError() const override;
    void clearLastError() override;
    double getProgress() const override;

private:
    virConnectPtr conn_ = nullptr;
    std::unique_ptr<CBTFactory> cbtFactory_;
    std::unordered_map<std::string, std::shared_ptr<BackupJob>> backupJobs_;
    std::unordered_map<std::string, std::shared_ptr<RestoreJob>> restoreJobs_;
    ProgressCallback progressCallback_;
    StatusCallback statusCallback_;
    std::string connectionString_;
    bool connected_;
    std::string lastError_;
    double progress_ = 0.0;
    mutable std::mutex mutex_;

    // Helper methods
    bool initializeCBT(const std::string& vmId);
    bool cleanupCBT(const std::string& vmId);
    std::string getDiskFormat(const std::string& diskPath) const;
    bool verifyDiskIntegrity(const std::string& diskPath);
    std::string calculateChecksum(const std::string& filePath);
}; 