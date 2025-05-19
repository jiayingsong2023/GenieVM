#pragma once

#include "common/vmware_connection.hpp"
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <vixDiskLib.h>

class DiskRestore {
public:
    using ProgressCallback = std::function<void(double)>;

    DiskRestore(std::shared_ptr<VMwareConnection> connection);
    ~DiskRestore();

    bool initialize();
    bool startRestore(const std::string& vmId, const std::string& backupPath);
    bool stopRestore();
    bool pauseRestore();
    bool resumeRestore();
    bool verifyRestore();
    void setProgressCallback(ProgressCallback callback);

private:
    // Helper methods
    bool restoreFull();
    bool getDiskInfo(VixDiskLibInfo*& diskInfo);
    bool openBackupDisk();
    bool createTargetDisk();
    void closeDisks();
    bool readBackupBlocks(uint64_t startSector, uint32_t numSectors, uint8_t* buffer);
    bool writeTargetBlocks(uint64_t startSector, uint32_t numSectors, const uint8_t* buffer);
    void logError(const std::string& operation, VixError vixError);

    std::shared_ptr<VMwareConnection> connection_;
    std::string vmId_;
    std::string backupPath_;
    std::string targetPath_;
    ProgressCallback progressCallback_;
    bool isRunning_;
    bool isPaused_;
    bool initialized_;

    // Disk handles
    VixDiskLibHandle backupHandle_;
    VixDiskLibHandle targetHandle_;
    VixDiskLibConnection connectionHandle_;
}; 
