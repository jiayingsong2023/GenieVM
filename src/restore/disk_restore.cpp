#include "restore/disk_restore.hpp"
#include "common/logger.hpp"
#include <cstring>

namespace vmware {

DiskRestore::DiskRestore(const std::string& backupPath,
                        const std::string& targetPath)
    : backupPath_(backupPath)
    , targetPath_(targetPath)
    , initialized_(false)
{
}

DiskRestore::~DiskRestore() {
    if (initialized_) {
        closeDisks();
    }
}

bool DiskRestore::initialize() {
    VixError vixError = VIX_OK;
    
    // Initialize VDDK
    vixError = VixDiskLib_Init(1, 1, NULL, NULL, NULL, NULL);
    if (VIX_FAILED(vixError)) {
        logError("Failed to initialize VDDK");
        return false;
    }

    // Open backup disk
    if (!openBackupDisk()) {
        return false;
    }

    // Create target disk
    if (!createTargetDisk()) {
        return false;
    }

    initialized_ = true;
    return true;
}

bool DiskRestore::restoreFull() {
    if (!initialized_) {
        Logger::error("DiskRestore not initialized");
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

        if (!readBackupBlocks(sectorsProcessed, sectorsToRead, buffer.data())) {
            return false;
        }

        if (!writeTargetBlocks(sectorsProcessed, sectorsToRead, buffer.data())) {
            return false;
        }

        sectorsProcessed += sectorsToRead;
        
        // Log progress
        int progress = static_cast<int>((sectorsProcessed * 100) / totalSectors);
        Logger::info("Restore progress: " + std::to_string(progress) + "%");
    }

    return true;
}

bool DiskRestore::getDiskInfo(VixDiskLibDiskInfo& diskInfo) {
    VixError vixError = VixDiskLib_GetInfo(backupHandle_, &diskInfo);
    if (VIX_FAILED(vixError)) {
        logError("Failed to get disk info");
        return false;
    }
    return true;
}

bool DiskRestore::openBackupDisk() {
    VixError vixError = VixDiskLib_Open(
        backupPath_.c_str(),
        VIXDISKLIB_FLAG_OPEN_UNBUFFERED,
        &backupHandle_
    );

    if (VIX_FAILED(vixError)) {
        logError("Failed to open backup disk");
        return false;
    }

    return true;
}

bool DiskRestore::createTargetDisk() {
    VixError vixError = VixDiskLib_Create(
        targetPath_.c_str(),
        VIXDISKLIB_ADAPTER_SCSI_LSILOGIC,
        VIXDISKLIB_DISK_TYPE_THIN,
        VIXDISKLIB_DISK_SIZE_GB * 100, // 100GB default size
        &targetHandle_
    );

    if (VIX_FAILED(vixError)) {
        logError("Failed to create target disk");
        return false;
    }

    return true;
}

void DiskRestore::closeDisks() {
    if (backupHandle_) {
        VixDiskLib_Close(backupHandle_);
        backupHandle_ = nullptr;
    }
    if (targetHandle_) {
        VixDiskLib_Close(targetHandle_);
        targetHandle_ = nullptr;
    }
}

bool DiskRestore::readBackupBlocks(uint64_t startSector,
                                 uint32_t numSectors,
                                 uint8_t* buffer) {
    VixError vixError = VixDiskLib_Read(
        backupHandle_,
        startSector,
        numSectors,
        buffer
    );

    if (VIX_FAILED(vixError)) {
        logError("Failed to read backup blocks");
        return false;
    }

    return true;
}

bool DiskRestore::writeTargetBlocks(uint64_t startSector,
                                  uint32_t numSectors,
                                  const uint8_t* buffer) {
    VixError vixError = VixDiskLib_Write(
        targetHandle_,
        startSector,
        numSectors,
        buffer
    );

    if (VIX_FAILED(vixError)) {
        logError("Failed to write target blocks");
        return false;
    }

    return true;
}

void DiskRestore::logError(const std::string& operation) {
    char* errorMsg = Vix_GetErrorText(VIX_ERROR_CODE, nullptr);
    Logger::error(operation + ": " + std::string(errorMsg));
    Vix_FreeErrorText(errorMsg);
}

} // namespace vmware 