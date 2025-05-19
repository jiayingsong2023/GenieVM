#pragma once

#include "backup/backup_provider.hpp"
#include "common/vmware_connection.hpp"
#include <memory>
#include <string>
#include <vector>

class VMwareBackupProvider : public BackupProvider {
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
    bool cancelBackup(const std::string& backupId) override;
    bool pauseBackup(const std::string& backupId) override;
    bool resumeBackup(const std::string& backupId) override;
    bool getBackupStatus(const std::string& backupId, std::string& status, double& progress) const override;

    // Restore operations
    bool startRestore(const std::string& vmId, const std::string& backupId) override;
    bool cancelRestore(const std::string& restoreId) override;
    bool pauseRestore(const std::string& restoreId) override;
    bool resumeRestore(const std::string& restoreId) override;
    bool getRestoreStatus(const std::string& restoreId, std::string& status, double& progress) const override;

    // CBT operations
    bool enableCBT(const std::string& vmId) override;
    bool disableCBT(const std::string& vmId) override;
    bool isCBTEnabled(const std::string& vmId) const override;
    bool getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                         std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) const override;

    // Progress and status callbacks
    void setProgressCallback(ProgressCallback callback) override;
    void setStatusCallback(StatusCallback callback) override;

    // Error handling
    std::string getLastError() const override;
    void clearLastError() override;

    // Backup verification
    bool verifyBackup(const std::string& backupId) override;
    bool calculateBackupChecksum(const std::string& backupPath, std::string& checksum);

    // Additional methods
    double getProgress() const override;

private:
    std::shared_ptr<VMwareConnection> connection_;
    mutable std::string lastError_;
    double progress_;
    ProgressCallback progressCallback_;
    StatusCallback statusCallback_;
}; 