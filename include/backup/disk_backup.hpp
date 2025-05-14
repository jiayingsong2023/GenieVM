#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include "common/vmware_connection.hpp"
#include <vixDiskLib.h>

class DiskBackup {
public:
    using ProgressCallback = std::function<void(double)>;

    DiskBackup(std::shared_ptr<VMwareConnection> connection);
    ~DiskBackup();

    bool initialize();
    bool startBackup(const std::string& vmId, const std::string& backupPath);
    bool stopBackup();
    bool pauseBackup();
    bool resumeBackup();
    void setProgressCallback(ProgressCallback callback);

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
    std::string vmId_;
    std::string backupPath_;
    std::string sourcePath_;
    ProgressCallback progressCallback_;
    bool isRunning_;
    bool isPaused_;

    // VDDK handles
    VixDiskLibHandle sourceDisk_;
    VixDiskLibHandle backupDisk_;
}; 