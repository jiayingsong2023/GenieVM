#include "backup/disk_backup.hpp"
#include "common/logger.hpp"
#include <cstring>

namespace vmware {

DiskBackup::DiskBackup(const std::string& diskPath,
                      const std::string& backupPath)
    : diskPath_(diskPath)
    , backupPath_(backupPath)
    , initialized_(false)
{
}

DiskBackup::~DiskBackup() {
    if (initialized_) {
        closeDisk();
    }
}

bool DiskBackup::initialize() {
    VixError vixError = VIX_OK;
    
    // Initialize VDDK
    vixError = VixDiskLib_Init(1, 1, NULL, NULL, NULL, NULL);
    if (VIX_FAILED(vixError)) {
        logError("Failed to initialize VDDK");
        return false;
    }

    // Connect to the disk
    if (!openDisk()) {
        return false;
    }

    initialized_ = true;
    return true;
}

bool DiskBackup::backupFull() {
    if (!initialized_) {
        Logger::error("DiskBackup not initialized");
        return false;
    }

    VixDiskLibDiskInfo diskInfo;
    if (!getDiskInfo(diskInfo)) {
        return false;
    }

    const uint32_t SECTOR_SIZE = 512;
    const uint32_t BUFFER_SIZE = 1024 * 1024; // 1MB buffer
    std::vector<uint8_t> buffer(BUFFER_SIZE);
    
    uint64_t totalSectors = diskInfo.capacity;
    uint64_t sectorsProcessed = 0;

    while (sectorsProcessed < totalSectors) {
        uint32_t sectorsToRead = std::min(
            static_cast<uint32_t>((BUFFER_SIZE / SECTOR_SIZE)),
            static_cast<uint32_t>(totalSectors - sectorsProcessed)
        );

        if (!readDiskBlocks(sectorsProcessed, sectorsToRead, buffer.data())) {
            return false;
        }

        if (!writeBackupBlocks(sectorsProcessed, sectorsToRead, buffer.data())) {
            return false;
        }

        sectorsProcessed += sectorsToRead;
        
        // Log progress
        int progress = static_cast<int>((sectorsProcessed * 100) / totalSectors);
        Logger::info("Backup progress: " + std::to_string(progress) + "%");
    }

    return true;
}

bool DiskBackup::backupIncremental() {
    if (!initialized_) {
        Logger::error("DiskBackup not initialized");
        return false;
    }

    VixDiskLibBlockList blockList;
    if (!getChangedBlocks(blockList)) {
        return false;
    }

    const uint32_t SECTOR_SIZE = 512;
    const uint32_t BUFFER_SIZE = 1024 * 1024; // 1MB buffer
    std::vector<uint8_t> buffer(BUFFER_SIZE);

    for (size_t i = 0; i < blockList.numBlocks; ++i) {
        uint64_t startSector = blockList.blocks[i].offset;
        uint32_t numSectors = blockList.blocks[i].length;

        if (!readDiskBlocks(startSector, numSectors, buffer.data())) {
            return false;
        }

        if (!writeBackupBlocks(startSector, numSectors, buffer.data())) {
            return false;
        }

        // Log progress
        int progress = static_cast<int>((i * 100) / blockList.numBlocks);
        Logger::info("Incremental backup progress: " + std::to_string(progress) + "%");
    }

    return true;
}

bool DiskBackup::getDiskInfo(VixDiskLibDiskInfo& diskInfo) {
    VixError vixError = VixDiskLib_GetInfo(diskHandle_, &diskInfo);
    if (VIX_FAILED(vixError)) {
        logError("Failed to get disk info");
        return false;
    }
    return true;
}

bool DiskBackup::openDisk() {
    VixError vixError = VixDiskLib_Open(
        diskPath_.c_str(),
        VIXDISKLIB_FLAG_OPEN_UNBUFFERED,
        &diskHandle_
    );

    if (VIX_FAILED(vixError)) {
        logError("Failed to open disk");
        return false;
    }

    return true;
}

void DiskBackup::closeDisk() {
    if (diskHandle_) {
        VixDiskLib_Close(diskHandle_);
        diskHandle_ = nullptr;
    }
}

bool DiskBackup::readDiskBlocks(uint64_t startSector,
                              uint32_t numSectors,
                              uint8_t* buffer) {
    VixError vixError = VixDiskLib_Read(
        diskHandle_,
        startSector,
        numSectors,
        buffer
    );

    if (VIX_FAILED(vixError)) {
        logError("Failed to read disk blocks");
        return false;
    }

    return true;
}

bool DiskBackup::writeBackupBlocks(uint64_t startSector,
                                 uint32_t numSectors,
                                 const uint8_t* buffer) {
    VixError vixError = VixDiskLib_Write(
        diskHandle_,
        startSector,
        numSectors,
        buffer
    );

    if (VIX_FAILED(vixError)) {
        logError("Failed to write backup blocks");
        return false;
    }

    return true;
}

bool DiskBackup::getChangedBlocks(VixDiskLibBlockList& blockList) {
    VixError vixError = VixDiskLib_QueryChangedBlocks(
        diskHandle_,
        &blockList
    );

    if (VIX_FAILED(vixError)) {
        logError("Failed to get changed blocks");
        return false;
    }

    return true;
}

void DiskBackup::logError(const std::string& operation) {
    char* errorMsg = Vix_GetErrorText(VIX_ERROR_CODE, nullptr);
    Logger::error(operation + ": " + std::string(errorMsg));
    Vix_FreeErrorText(errorMsg);
}

} // namespace vmware 