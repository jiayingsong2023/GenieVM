#ifndef VMWARE_BACKUP_PROVIDER_HPP
#define VMWARE_BACKUP_PROVIDER_HPP

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include <optional>
#include <map>
#include "backup/backup_provider.hpp"
#include "common/vmware_connection.hpp"
#include "vddk_wrapper/vddk_wrapper.h"
#include "common/logger.hpp"
#include "common/backup_status.hpp"  // Added to get BackupMetadata definition

// Forward declarations
class BackupJob;
class ParallelTaskManager;

class VMwareBackupProvider : public BackupProvider, public std::enable_shared_from_this<VMwareBackupProvider> {
public:
    VMwareBackupProvider();
    explicit VMwareBackupProvider(VMwareConnection* connection);  // Does not take ownership
    explicit VMwareBackupProvider(const std::string& connectionString);
    ~VMwareBackupProvider() override;

    // Connection management
    bool connect(const std::string& host, const std::string& username, const std::string& password) override;
    void disconnect() override;
    bool isConnected() const override;
    bool initialize();
    void cleanup();
    bool verifyConnection();
    void cleanupActiveOperations();

    // VM operations
    std::vector<std::string> listVMs() const;
    bool getVMInfo(const std::string& vmId, std::string& name, std::string& status) const;
    bool getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths) override;
    bool backupDisk(const std::string& vmId, const std::string& diskPath, const BackupConfig& config) override;
    bool verifyDisk(const std::string& diskPath) override;
    bool listBackups(std::vector<std::string>& backupDirs) override;
    bool deleteBackup(const std::string& backupDir) override;
    bool verifyBackup(const std::string& backupId) override;
    bool restoreDisk(const std::string& vmId, const std::string& diskPath, const RestoreConfig& config);
    bool getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                         std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) override;

    // Snapshot management
    bool createSnapshot(const std::string& vmId, std::string& snapshotId) override;
    bool removeSnapshot(const std::string& vmId, const std::string& snapshotId) override;
    void cleanupSnapshot();

    // Backup management
    bool startBackup(const std::string& vmId, const BackupConfig& config);
    bool cancelBackup(const std::string& vmId);
    bool pauseBackup(const std::string& backupId);
    bool resumeBackup(const std::string& backupId);
    bool getBackupStatus(const std::string& backupId, BackupStatus& status);
    bool verifyBackupIntegrity(const std::string& backupId);
    bool saveBackupMetadata(const std::string& backupId, const std::string& vmId,
                           const std::vector<std::string>& diskPaths);
    std::optional<BackupMetadata> getLatestBackupInfo(const std::string& vmId);
    std::string calculateChecksum(const std::string& filePath);

    // Restore management
    bool startRestore(const std::string& vmId, const std::string& backupId);
    bool cancelRestore(const std::string& restoreId);
    bool pauseRestore(const std::string& restoreId);
    bool resumeRestore(const std::string& restoreId);
    bool getRestoreStatus(const std::string& restoreId, RestoreStatus& status);
    bool verifyRestore(const std::string& vmId, const std::string& backupId);

    // Progress tracking
    double getProgress() const override;
    std::string getLastError() const override;
    void clearLastError() override;
    void setProgressCallback(ProgressCallback callback);
    void setStatusCallback(StatusCallback callback);

    // CBT operations
    bool enableCBT(const std::string& vmId);
    bool disableCBT(const std::string& vmId);
    bool isCBTEnabled(const std::string& vmId) const;
    bool initializeCBT(const std::string& diskPath);
    bool cleanupCBT(const std::string& diskPath);

    // Validation
    bool validateDiskPath(const std::string& diskPath) const;
    bool validateBackupPath(const std::string& backupPath) const;
    bool validateRestorePath(const std::string& restorePath) const;

private:
    VMwareConnection* connection_;  // Not owned by VMwareBackupProvider
    mutable std::mutex mutex_;  // Made mutable for const member functions
    double progress_;
    std::string lastError_;
    ProgressCallback progressCallback_;
    StatusCallback statusCallback_;
    std::map<std::string, std::unique_ptr<BackupJob>> activeOperations_;
    std::string currentSnapshotName_;
    std::string currentVmId_;  // Added missing member

    void updateProgress(double progress, const std::string& status);
    void handleError(int32_t error);
    bool initializeVDDK();
};

#endif // VMWARE_BACKUP_PROVIDER_HPP 