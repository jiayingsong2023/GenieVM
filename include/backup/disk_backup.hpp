#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include "vixDiskLib.h"
#include "common/logger.hpp"

namespace vmware {

class DiskBackup {
public:
    DiskBackup(const std::string& sourcePath, const std::string& targetPath);
    ~DiskBackup();

    // Initialize VDDK connection
    bool initialize();
    
    // Backup operations
    bool backupFull();
    bool backupIncremental();
    
    // Progress callback
    using ProgressCallback = std::function<void(double)>;
    void setProgressCallback(ProgressCallback callback) { progressCallback_ = callback; }

    // Get disk information
    bool getDiskInfo(VixDiskLibDiskInfo& diskInfo);

private:
    std::string sourcePath_;
    std::string targetPath_;
    VixDiskLibConnection connection_;
    VixDiskLibHandle sourceDisk_;
    VixDiskLibHandle targetDisk_;
    ProgressCallback progressCallback_;

    bool openDisks();
    void closeDisks();
    bool copyBlocks(VixDiskLibBlockList* blockList);
    void reportProgress(double progress);
    static void VDDK_CALLBACK progressFunc(void* data, int percent);

    // Helper methods
    bool readDiskBlocks(uint64_t startSector,
                       uint32_t numSectors,
                       uint8_t* buffer);
    bool writeBackupBlocks(uint64_t startSector,
                          uint32_t numSectors,
                          const uint8_t* buffer);
    bool getChangedBlocks(VixDiskLibBlockList& blockList);
    void logError(const std::string& operation);
};

} // namespace vmware 