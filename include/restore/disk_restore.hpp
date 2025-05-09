#pragma once

#include <string>
#include <memory>
#include <vixDiskLib.h>
#include "common/logger.hpp"

namespace vmware {

class DiskRestore {
public:
    DiskRestore(const std::string& backupPath,
               const std::string& targetPath);
    ~DiskRestore();

    // Initialize VDDK connection
    bool initialize();

    // Perform full disk restore
    bool restoreFull();

    // Get disk information
    bool getDiskInfo(VixDiskLibDiskInfo& diskInfo);

private:
    std::string backupPath_;
    std::string targetPath_;
    VixDiskLibConnection connection_;
    VixDiskLibHandle backupHandle_;
    VixDiskLibHandle targetHandle_;
    bool initialized_;

    // Helper methods
    bool openBackupDisk();
    bool createTargetDisk();
    void closeDisks();
    bool readBackupBlocks(uint64_t startSector,
                         uint32_t numSectors,
                         uint8_t* buffer);
    bool writeTargetBlocks(uint64_t startSector,
                          uint32_t numSectors,
                          const uint8_t* buffer);
    void logError(const std::string& operation);
}; 