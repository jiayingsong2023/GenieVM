#pragma once

#include "backup/vm_config.hpp"
#include "common/vmware_connection.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <vixDiskLib.h>

class DiskBackup {
public:
    using ProgressCallback = std::function<void(double)>;

    DiskBackup(std::shared_ptr<VMwareConnection> connection);
    ~DiskBackup();

    bool initialize();
    bool startBackup(const std::string& vmId, const BackupConfig& config);
    bool cancelBackup(const std::string& backupId);
    bool pauseBackup(const std::string& backupId);
    bool resumeBackup(const std::string& backupId);
    bool getBackupStatus(const std::string& backupId, std::string& status, double& progress) const;

    void setProgressCallback(ProgressCallback callback);
    void setStatusCallback(std::function<void(const std::string&)> callback);

    // Disk operations
    bool openDisks();
    void closeDisks();
    bool backupFull();
    bool backupIncremental();
    bool restore();

private:
    static char progressFunc(void* data, int percent);
    bool copyBlocks(VixDiskLibBlockList* blockList);

    std::shared_ptr<VMwareConnection> connection_;
    std::string lastError_;
    double progress_;
    ProgressCallback progressCallback_;
    std::function<void(const std::string&)> statusCallback_;

    // VDDK handles
    VixDiskLibHandle sourceDisk_;
    VixDiskLibHandle backupDisk_;

    // State variables
    bool isRunning_;
    bool isPaused_;
    std::string sourcePath_;
    std::string backupPath_;
}; 