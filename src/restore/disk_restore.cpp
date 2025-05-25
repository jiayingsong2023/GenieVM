#include "restore/disk_restore.hpp"
#include "common/logger.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>
#include <chrono>
#include <thread>
#include "vddk_wrapper/vddk_wrapper.h"

DiskRestore::DiskRestore(std::shared_ptr<VMwareConnection> connection)
    : connection_(connection)
    , isRunning_(false)
    , isPaused_(false)
    , initialized_(false)
    , backupHandle_(nullptr)
    , targetHandle_(nullptr)
    , connectionHandle_(nullptr) {
}

DiskRestore::~DiskRestore() {
    if (isRunning_) {
        stopRestore();
    }
}

bool DiskRestore::initialize() {
    if (!connection_) {
        Logger::error("No connection provided");
        return false;
    }
    initialized_ = true;
    return true;
}

bool DiskRestore::startRestore(const std::string& vmId, const std::string& backupPath) {
    if (isRunning_) {
        Logger::error("Restore already in progress");
        return false;
    }

    if (!std::filesystem::exists(backupPath)) {
        Logger::error("Backup path does not exist: " + backupPath);
        return false;
    }

    vmId_ = vmId;
    backupPath_ = backupPath;
    isRunning_ = true;
    isPaused_ = false;

    // Initialize VDDK connection
    if (!connection_->initializeVDDK()) {
        Logger::error("Failed to initialize VDDK");
        return false;
    }

    // Get VDDK connection handle
    connectionHandle_ = connection_->getVDDKConnection();
    if (!connectionHandle_) {
        Logger::error("Failed to get VDDK connection handle");
        return false;
    }

    // Open backup disk
    if (!openBackupDisk()) {
        Logger::error("Failed to open backup disk");
        return false;
    }

    // Create target disk
    if (!createTargetDisk()) {
        Logger::error("Failed to create target disk");
        closeDisks();
        return false;
    }

    // Start full restore
    if (!restoreFull()) {
        Logger::error("Failed to perform full restore");
        closeDisks();
        return false;
    }

    // Verify restore
    if (!verifyRestore()) {
        Logger::error("Restore verification failed");
        closeDisks();
        return false;
    }

    if (progressCallback_) {
        progressCallback_(1.0);
    }

    return true;
}

bool DiskRestore::stopRestore() {
    if (!isRunning_) {
        return true;
    }

    isRunning_ = false;
    isPaused_ = false;

    // Close all disk handles
    closeDisks();

    // Cleanup VDDK connection
    connection_->disconnectFromDisk();
    connection_->cleanupVDDK();

    // Clear state
    vmId_.clear();
    backupPath_.clear();
    targetPath_.clear();
    initialized_ = false;

    if (progressCallback_) {
        progressCallback_(1.0);
    }

    return true;
}

bool DiskRestore::pauseRestore() {
    if (!isRunning_ || isPaused_) {
        return false;
    }

    isPaused_ = true;
    return true;
}

bool DiskRestore::resumeRestore() {
    if (!isRunning_ || !isPaused_) {
        return false;
    }

    isPaused_ = false;
    return true;
}

void DiskRestore::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = callback;
}

bool DiskRestore::verifyRestore() {
    if (!isRunning_) {
        Logger::error("No restore in progress");
        return false;
    }

    // Get disk info for both backup and target
    VixDiskLibInfo* backupInfo = nullptr;
    VixDiskLibInfo* targetInfo = nullptr;

    if (!getDiskInfo(backupInfo)) {
        Logger::error("Failed to get backup disk info");
        return false;
    }

    // Open target disk to verify
    VixError vixError = VixDiskLib_OpenWrapper(
        connectionHandle_,
        targetPath_.c_str(),
        VIXDISKLIB_FLAG_OPEN_UNBUFFERED,
        &targetHandle_
    );

    if (vixError != VIX_OK) {
        logError("Failed to open target disk for verification", vixError);
        VixDiskLib_FreeInfoWrapper(backupInfo);
        return false;
    }

    vixError = VixDiskLib_GetInfoWrapper(targetHandle_, &targetInfo);
    if (vixError != VIX_OK) {
        logError("Failed to get target disk info", vixError);
        VixDiskLib_FreeInfoWrapper(backupInfo);
        VixDiskLib_CloseWrapper(&targetHandle_);
        return false;
    }

    // Compare disk sizes
    bool sizeMatch = (backupInfo->capacity == targetInfo->capacity);
    VixDiskLib_FreeInfoWrapper(backupInfo);
    VixDiskLib_FreeInfoWrapper(targetInfo);
    VixDiskLib_CloseWrapper(&targetHandle_);

    if (!sizeMatch) {
        Logger::error("Disk size mismatch during verification");
        return false;
    }

    return true;
}

bool DiskRestore::restoreFull() {
    if (!initialized_) {
        Logger::error("DiskRestore not initialized");
        return false;
    }

    VixDiskLibInfo* diskInfo = nullptr;
    if (!getDiskInfo(diskInfo)) {
        return false;
    }

    const uint32_t SECTOR_SIZE = 512;
    const uint32_t BUFFER_SIZE = 1024 * 1024; // 1MB buffer
    std::vector<uint8_t> buffer(BUFFER_SIZE);
    
    uint64_t totalSectors = diskInfo->capacity;
    uint64_t sectorsProcessed = 0;

    while (sectorsProcessed < totalSectors) {
        uint32_t sectorsToRead = std::min(
            static_cast<uint32_t>((BUFFER_SIZE / SECTOR_SIZE)),
            static_cast<uint32_t>(totalSectors - sectorsProcessed)
        );

        if (!readBackupBlocks(sectorsProcessed, sectorsToRead, buffer.data())) {
            VixDiskLib_FreeInfo(diskInfo);
            return false;
        }

        if (!writeTargetBlocks(sectorsProcessed, sectorsToRead, buffer.data())) {
            VixDiskLib_FreeInfo(diskInfo);
            return false;
        }

        sectorsProcessed += sectorsToRead;
        
        // Log progress
        int progress = static_cast<int>((sectorsProcessed * 100) / totalSectors);
        Logger::info("Restore progress: " + std::to_string(progress) + "%");
    }

    VixDiskLib_FreeInfo(diskInfo);
    return true;
}

bool DiskRestore::getDiskInfo(VixDiskLibInfo*& diskInfo) {
    VixError vixError = VixDiskLib_GetInfo(backupHandle_, &diskInfo);
    if (VIX_FAILED(vixError)) {
        logError("Failed to get disk info", vixError);
        return false;
    }
    return true;
}

bool DiskRestore::openBackupDisk() {
    VixError vixError = VixDiskLib_Open(
        connectionHandle_,
        backupPath_.c_str(),
        VIXDISKLIB_FLAG_OPEN_UNBUFFERED,
        &backupHandle_
    );

    if (VIX_FAILED(vixError)) {
        logError("Failed to open backup disk", vixError);
        return false;
    }

    return true;
}

bool DiskRestore::createTargetDisk() {
    VixDiskLibCreateParams createParams;
    memset(&createParams, 0, sizeof(createParams));
    createParams.adapterType = VIXDISKLIB_ADAPTER_SCSI_LSILOGIC;
    createParams.diskType = VIXDISKLIB_DISK_VMFS_THIN;
    createParams.capacity = 100 * 1024 * 1024 * 1024ULL; // 100GB in bytes

    VixError vixError = VixDiskLib_Create(
        connectionHandle_,
        targetPath_.c_str(),
        &createParams,
        nullptr,  // progress callback
        nullptr   // progress callback data
    );

    if (VIX_FAILED(vixError)) {
        logError("Failed to create target disk", vixError);
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
        logError("Failed to read backup blocks", vixError);
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
        logError("Failed to write target blocks", vixError);
        return false;
    }

    return true;
}

void DiskRestore::logError(const std::string& operation, VixError vixError) {
    char* errorMsg = VixDiskLib_GetErrorText(vixError, nullptr);
    Logger::error(operation + ": " + std::string(errorMsg));
    VixDiskLib_FreeErrorText(errorMsg);
}

bool DiskRestore::restoreDisk(const std::string& backupPath, const std::string& targetPath) {
    VixDiskLibInfo* backupInfo = nullptr;
    VixDiskLibInfo* targetInfo = nullptr;

    VixError vixError = VixDiskLib_OpenWrapper(connection_->getVDDKConnection(),
                                             backupPath.c_str(),
                                             VIXDISKLIB_FLAG_OPEN_UNBUFFERED,
                                             &backupHandle_);
    if (vixError != VIX_OK) {
        handleError(vixError);
        return false;
    }

    vixError = VixDiskLib_GetInfoWrapper(backupHandle_, &backupInfo);
    if (vixError != VIX_OK) {
        VixDiskLib_FreeInfoWrapper(backupInfo);
        VixDiskLib_CloseWrapper(&backupHandle_);
        handleError(vixError);
        return false;
    }

    vixError = VixDiskLib_OpenWrapper(connection_->getVDDKConnection(),
                                    targetPath.c_str(),
                                    VIXDISKLIB_FLAG_OPEN_UNBUFFERED,
                                    &targetHandle_);
    if (vixError != VIX_OK) {
        VixDiskLib_FreeInfoWrapper(backupInfo);
        VixDiskLib_CloseWrapper(&backupHandle_);
        handleError(vixError);
        return false;
    }

    vixError = VixDiskLib_GetInfoWrapper(targetHandle_, &targetInfo);
    if (vixError != VIX_OK) {
        VixDiskLib_FreeInfoWrapper(backupInfo);
        VixDiskLib_FreeInfoWrapper(targetInfo);
        VixDiskLib_CloseWrapper(&backupHandle_);
        VixDiskLib_CloseWrapper(&targetHandle_);
        handleError(vixError);
        return false;
    }

    // Verify disk sizes match
    if (backupInfo->capacity != targetInfo->capacity) {
        VixDiskLib_FreeInfoWrapper(backupInfo);
        VixDiskLib_FreeInfoWrapper(targetInfo);
        VixDiskLib_CloseWrapper(&backupHandle_);
        VixDiskLib_CloseWrapper(&targetHandle_);
        error_ = "Disk sizes do not match";
        return false;
    }

    VixDiskLib_FreeInfoWrapper(backupInfo);
    VixDiskLib_FreeInfoWrapper(targetInfo);

    // Restore the disk
    VixDiskLibBlockList* blockList = nullptr;
    vixError = VixDiskLib_QueryAllocatedBlocksWrapper(backupHandle_,
                                                    0,
                                                    VIXDISKLIB_SECTOR_SIZE,
                                                    &blockList);
    if (vixError != VIX_OK) {
        VixDiskLib_CloseWrapper(&backupHandle_);
        VixDiskLib_CloseWrapper(&targetHandle_);
        handleError(vixError);
        return false;
    }

    // Copy allocated blocks
    for (int i = 0; i < blockList->numBlocks; i++) {
        std::vector<uint8_t> buffer(blockList->blocks[i].length * VIXDISKLIB_SECTOR_SIZE);
        
        vixError = VixDiskLib_ReadWrapper(backupHandle_,
                                        blockList->blocks[i].offset,
                                        blockList->blocks[i].length,
                                        buffer.data());
        if (vixError != VIX_OK) {
            VixDiskLib_FreeBlockListWrapper(blockList);
            VixDiskLib_CloseWrapper(&backupHandle_);
            VixDiskLib_CloseWrapper(&targetHandle_);
            handleError(vixError);
            return false;
        }

        vixError = VixDiskLib_WriteWrapper(targetHandle_,
                                         blockList->blocks[i].offset,
                                         blockList->blocks[i].length,
                                         buffer.data());
        if (vixError != VIX_OK) {
            VixDiskLib_FreeBlockListWrapper(blockList);
            VixDiskLib_CloseWrapper(&backupHandle_);
            VixDiskLib_CloseWrapper(&targetHandle_);
            handleError(vixError);
            return false;
        }
    }

    VixDiskLib_FreeBlockListWrapper(blockList);
    VixDiskLib_CloseWrapper(&backupHandle_);
    VixDiskLib_CloseWrapper(&targetHandle_);
    return true;
}

void DiskRestore::handleError(VixError error) {
    char* errorMsg = VixDiskLib_GetErrorTextWrapper(error, nullptr, 0);
    if (errorMsg) {
        error_ = errorMsg;
        VixDiskLib_FreeErrorTextWrapper(errorMsg);
    } else {
        error_ = "Unknown VDDK error: " + std::to_string(error);
    }
}
