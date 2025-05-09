#pragma once

#include <string>
#include <memory>
#include <vixDiskLib.h>
#include "common/logger.hpp"

namespace vmware {

class DiskBackup {
public:
    DiskBackup(const std::string& diskPath,
              const std::string& backupPath);
    ~DiskBackup();

    // Initialize VDDK connection
    bool initialize();

    // Perform full disk backup
    bool backupFull();

    // Perform incremental backup using CBT
    bool backupIncremental();

    // Get disk information
    bool getDiskInfo(VixDiskLibDiskInfo& diskInfo);

private:
    std::string diskPath_;
    std::string backupPath_;
    VixDiskLibConnection connection_;
    VixDiskLibHandle diskHandle_;
    bool initialized_;

    // Helper methods
    bool openDisk();
    void closeDisk();
    bool readDiskBlocks(uint64_t startSector,
                       uint32_t numSectors,
                       uint8_t* buffer);
    bool writeBackupBlocks(uint64_t startSector,
                          uint32_t numSectors,
                          const uint8_t* buffer);
    bool getChangedBlocks(VixDiskLibBlockList& blockList);
    void logError(const std::string& operation);
}; 