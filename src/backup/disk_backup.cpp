#include "backup/disk_backup.hpp"
#include "common/logger.hpp"
#include <vixDiskLib.h>
#include <cstring>
#include <filesystem>

DiskBackup::DiskBackup(std::shared_ptr<VMwareConnection> connection)
    : connection_(connection)
    , isRunning_(false)
    , isPaused_(false)
    , sourceDisk_(nullptr)
    , backupDisk_(nullptr) {
}

DiskBackup::~DiskBackup() {
    if (isRunning_) {
        stopBackup();
    }
    closeDisks();
}

bool DiskBackup::initialize() {
    if (!connection_) {
        Logger::error("No connection provided");
        return false;
    }
    return true;
}

bool DiskBackup::startBackup(const std::string& vmId, const std::string& backupPath) {
    if (isRunning_) {
        Logger::error("Backup already in progress");
        return false;
    }

    if (!std::filesystem::exists(backupPath)) {
        try {
            std::filesystem::create_directories(backupPath);
        } catch (const std::exception& e) {
            Logger::error("Failed to create backup directory: " + std::string(e.what()));
            return false;
        }
    }

    vmId_ = vmId;
    backupPath_ = backupPath;
    isRunning_ = true;
    isPaused_ = false;

    // TODO: Implement actual backup logic using VMwareConnection
    if (progressCallback_) {
        progressCallback_(0.0);
    }

    return true;
}

bool DiskBackup::stopBackup() {
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

bool DiskBackup::pauseBackup() {
    if (!isRunning_ || isPaused_) {
        return false;
    }

    isPaused_ = true;
    return true;
}

bool DiskBackup::resumeBackup() {
    if (!isRunning_ || !isPaused_) {
        return false;
    }

    isPaused_ = false;
    return true;
}

void DiskBackup::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = callback;
}

bool DiskBackup::openDisks() {
    if (!connection_->connectToDisk(sourcePath_)) {
        return false;
    }

    VixError error = VixDiskLib_Open(connection_->getVixConnection(),
                                   const_cast<char*>(sourcePath_.c_str()),
                                   VIXDISKLIB_FLAG_OPEN_READ_ONLY,
                                   &sourceDisk_);
    if (VIX_FAILED(error)) {
        Logger::error("Failed to open source disk: " + std::string(VixDiskLib_GetErrorText(error, nullptr)));
        return false;
    }

    VixDiskLibCreateParams createParams;
    memset(&createParams, 0, sizeof(createParams));
    createParams.diskType = VIXDISKLIB_DISK_MONOLITHIC_SPARSE;
    createParams.adapterType = VIXDISKLIB_ADAPTER_SCSI_LSILOGIC;
    createParams.hwVersion = VIXDISKLIB_HWVERSION_WORKSTATION_5;

    error = VixDiskLib_Create(connection_->getVixConnection(),
                             const_cast<char*>(backupPath_.c_str()),
                             &createParams,
                             nullptr,
                             nullptr);
    if (VIX_FAILED(error)) {
        Logger::error("Failed to create backup disk: " + std::string(VixDiskLib_GetErrorText(error, nullptr)));
        return false;
    }

    error = VixDiskLib_Open(connection_->getVixConnection(),
                           const_cast<char*>(backupPath_.c_str()),
                           VIXDISKLIB_FLAG_OPEN_UNBUFFERED,
                           &backupDisk_);
    if (VIX_FAILED(error)) {
        Logger::error("Failed to open backup disk: " + std::string(VixDiskLib_GetErrorText(error, nullptr)));
        return false;
    }

    return true;
}

void DiskBackup::closeDisks() {
    if (sourceDisk_) {
        VixDiskLib_Close(sourceDisk_);
        sourceDisk_ = nullptr;
    }
    if (backupDisk_) {
        VixDiskLib_Close(backupDisk_);
        backupDisk_ = nullptr;
    }
    connection_->disconnectFromDisk();
}

bool DiskBackup::backupFull() {
    VixDiskLibInfo* diskInfo = nullptr;
    VixError error = VixDiskLib_GetInfo(sourceDisk_, &diskInfo);
    if (VIX_FAILED(error)) {
        Logger::error("Failed to get disk info: " + std::string(VixDiskLib_GetErrorText(error, nullptr)));
        return false;
    }

    // Create a new disk with the same parameters
    VixDiskLibCreateParams createParams;
    memset(&createParams, 0, sizeof(createParams));
    createParams.diskType = VIXDISKLIB_DISK_MONOLITHIC_SPARSE;
    createParams.adapterType = VIXDISKLIB_ADAPTER_SCSI_LSILOGIC;
    createParams.hwVersion = VIXDISKLIB_HWVERSION_WORKSTATION_5;

    error = VixDiskLib_Clone(connection_->getVixConnection(),
                            backupPath_.c_str(),
                            connection_->getVixConnection(),
                            sourcePath_.c_str(),
                            &createParams,
                            progressFunc,
                            this,
                            TRUE);
    if (VIX_FAILED(error)) {
        Logger::error("Failed to clone disk: " + std::string(VixDiskLib_GetErrorText(error, nullptr)));
        return false;
    }

    return true;
}

bool DiskBackup::backupIncremental() {
    VixDiskLibBlockList* blockList = nullptr;
    VixError error = VixDiskLib_QueryAllocatedBlocks(sourceDisk_,
                                                    0,  // start sector
                                                    0,  // end sector
                                                    0,  // sector size
                                                    &blockList);
    if (VIX_FAILED(error)) {
        Logger::error("Failed to query changed blocks: " + std::string(VixDiskLib_GetErrorText(error, nullptr)));
        return false;
    }

    bool success = copyBlocks(blockList);
    VixDiskLib_FreeBlockList(blockList);
    return success;
}

bool DiskBackup::copyBlocks(VixDiskLibBlockList* blockList) {
    for (size_t i = 0; i < blockList->numBlocks; i++) {
        if (blockList->blocks[i].length > 0) {
            VixError error = VixDiskLib_Read(sourceDisk_,
                                            blockList->blocks[i].offset,
                                            blockList->blocks[i].length,
                                            nullptr);
            if (VIX_FAILED(error)) {
                Logger::error("Failed to read blocks: " + std::string(VixDiskLib_GetErrorText(error, nullptr)));
                return false;
            }

            error = VixDiskLib_Write(backupDisk_,
                                    blockList->blocks[i].offset,
                                    blockList->blocks[i].length,
                                    nullptr);
            if (VIX_FAILED(error)) {
                Logger::error("Failed to write blocks: " + std::string(VixDiskLib_GetErrorText(error, nullptr)));
                return false;
            }
        }
    }
    return true;
}

char DiskBackup::progressFunc(void* data, int percent) {
    auto* backup = static_cast<DiskBackup*>(data);
    if (backup && backup->progressCallback_) {
        backup->progressCallback_(percent);
    }
    return 0;  // Return 0 to continue the operation
}

bool DiskBackup::restore() {
    // Open the backup disk for reading
    VixError error = VixDiskLib_Open(connection_->getVixConnection(),
                                    const_cast<char*>(backupPath_.c_str()),
                                    VIXDISKLIB_FLAG_OPEN_READ_ONLY,
                                    &sourceDisk_);
    if (VIX_FAILED(error)) {
        Logger::error("Failed to open backup disk: " + std::string(VixDiskLib_GetErrorText(error, nullptr)));
        return false;
    }

    // Open the target disk for writing
    error = VixDiskLib_Open(connection_->getVixConnection(),
                           const_cast<char*>(sourcePath_.c_str()),
                           VIXDISKLIB_FLAG_OPEN_UNBUFFERED,
                           &backupDisk_);
    if (VIX_FAILED(error)) {
        Logger::error("Failed to open target disk: " + std::string(VixDiskLib_GetErrorText(error, nullptr)));
        return false;
    }

    // Get disk info
    VixDiskLibInfo* diskInfo = nullptr;
    error = VixDiskLib_GetInfo(sourceDisk_, &diskInfo);
    if (VIX_FAILED(error)) {
        Logger::error("Failed to get disk info: " + std::string(VixDiskLib_GetErrorText(error, nullptr)));
        return false;
    }

    // Clone the disk
    error = VixDiskLib_Clone(connection_->getVixConnection(),
                            sourcePath_.c_str(),
                            connection_->getVixConnection(),
                            backupPath_.c_str(),
                            nullptr,  // Use default create params
                            progressFunc,
                            this,
                            TRUE);
    if (VIX_FAILED(error)) {
        Logger::error("Failed to restore disk: " + std::string(VixDiskLib_GetErrorText(error, nullptr)));
        return false;
    }

    return true;
} 