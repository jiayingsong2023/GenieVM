#include "restore/disk_restore.hpp"
#include "common/logger.hpp"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstring>
#include <chrono>
#include <thread>

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

    // TODO: Implement actual restore logic using VMwareConnection
    if (progressCallback_) {
        progressCallback_(0.0);
    }

    return true;
}

bool DiskRestore::stopRestore() {
    if (!isRunning_) {
        return true;
    }

    isRunning_ = false;
    isPaused_ = false;

    // TODO: Implement cleanup logic
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

    // TODO: Implement restore verification
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
