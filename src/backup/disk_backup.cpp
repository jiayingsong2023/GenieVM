#include "backup/disk_backup.hpp"
#include "common/logger.hpp"
#include <vixDiskLib.h>
#include <stdexcept>
#include <cstring>
#include <filesystem>
#include <vector>

DiskBackup::DiskBackup(std::shared_ptr<VMwareConnection> connection)
    : connection_(connection)
    , progress_(0.0)
    , sourceDisk_(nullptr)
    , backupDisk_(nullptr)
    , isRunning_(false)
    , isPaused_(false) {
}

DiskBackup::~DiskBackup() {
    if (isRunning_) {
        cancelBackup("");
    }
    closeDisks();
}

bool DiskBackup::initialize() {
    if (!connection_) {
        lastError_ = "No connection available";
        return false;
    }
    return connection_->initializeVDDK();
}

bool DiskBackup::startBackup(const std::string& vmId, const BackupConfig& config) {
    if (isRunning_) {
        lastError_ = "Backup already in progress";
        return false;
    }

    sourcePath_ = config.sourcePath;
    backupPath_ = config.backupPath;

    if (!openDisks()) {
        return false;
    }

    isRunning_ = true;
    isPaused_ = false;

    bool result = false;
    if (config.incremental) {
        result = backupIncremental();
    } else {
        result = backupFull();
    }

    if (!result) {
        isRunning_ = false;
        closeDisks();
    }

    return result;
}

bool DiskBackup::cancelBackup(const std::string& backupId) {
    if (!isRunning_) {
        return true;
    }

    isRunning_ = false;
    closeDisks();
    return true;
}

bool DiskBackup::pauseBackup(const std::string& backupId) {
    if (!isRunning_ || isPaused_) {
        return false;
    }

    isPaused_ = true;
    return true;
}

bool DiskBackup::resumeBackup(const std::string& backupId) {
    if (!isRunning_ || !isPaused_) {
        return false;
    }

    isPaused_ = false;
    return true;
}

bool DiskBackup::getBackupStatus(const std::string& backupId, std::string& status, double& progress) const {
    if (!isRunning_) {
        status = "Not running";
        progress = 0.0;
        return true;
    }

    if (isPaused_) {
        status = "Paused";
    } else {
        status = "Running";
    }

    progress = progress_;
    return true;
}

void DiskBackup::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = std::move(callback);
}

void DiskBackup::setStatusCallback(std::function<void(const std::string&)> callback) {
    statusCallback_ = std::move(callback);
}

bool DiskBackup::openDisks() {
    VixDiskLibCreateParams createParams;
    memset(&createParams, 0, sizeof(createParams));
    createParams.adapterType = VIXDISKLIB_ADAPTER_IDE;
    createParams.diskType = VIXDISKLIB_DISK_MONOLITHIC_FLAT;

    VixError error = VixDiskLib_Open(connection_->getVDDKConnection(),
                                    const_cast<char*>(sourcePath_.c_str()),
                                    VIXDISKLIB_FLAG_OPEN_UNBUFFERED,
                                    &sourceDisk_);
    if (VIX_FAILED(error)) {
        lastError_ = "Failed to open source disk";
        return false;
    }

    error = VixDiskLib_Create(connection_->getVDDKConnection(),
                             const_cast<char*>(backupPath_.c_str()),
                             &createParams,
                             progressFunc,
                             this);
    if (VIX_FAILED(error)) {
        VixDiskLib_Close(sourceDisk_);
        sourceDisk_ = nullptr;
        lastError_ = "Failed to create backup disk";
        return false;
    }

    error = VixDiskLib_Open(connection_->getVDDKConnection(),
                           const_cast<char*>(backupPath_.c_str()),
                           VIXDISKLIB_FLAG_OPEN_UNBUFFERED,
                           &backupDisk_);
    if (VIX_FAILED(error)) {
        VixDiskLib_Close(sourceDisk_);
        sourceDisk_ = nullptr;
        lastError_ = "Failed to open backup disk";
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
    VixDiskLibCreateParams createParams;
    memset(&createParams, 0, sizeof(createParams));
    createParams.adapterType = VIXDISKLIB_ADAPTER_IDE;
    createParams.diskType = VIXDISKLIB_DISK_MONOLITHIC_FLAT;

    VixError error = VixDiskLib_Clone(connection_->getVDDKConnection(),
                                     sourcePath_.c_str(),
                                     connection_->getVDDKConnection(),
                                     backupPath_.c_str(),
                                     &createParams,
                                     progressFunc,
                                     this,
                                     TRUE);
    if (VIX_FAILED(error)) {
        lastError_ = "Failed to clone disk";
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
        lastError_ = "Failed to query allocated blocks";
        return false;
    }

    bool result = copyBlocks(blockList);
    VixDiskLib_FreeBlockList(blockList);
    return result;
}

bool DiskBackup::copyBlocks(VixDiskLibBlockList* blockList) {
    if (!blockList) {
        return false;
    }

    for (size_t i = 0; i < blockList->numBlocks; i++) {
        if (blockList->blocks[i].length > 0) {
            // Allocate buffer for reading
            std::vector<uint8_t> buffer(blockList->blocks[i].length * VIXDISKLIB_SECTOR_SIZE);
            
            VixError error = VixDiskLib_Read(sourceDisk_,
                                            blockList->blocks[i].offset,
                                            blockList->blocks[i].length,
                                            buffer.data());
            if (VIX_FAILED(error)) {
                lastError_ = "Failed to read blocks";
                return false;
            }

            error = VixDiskLib_Write(backupDisk_,
                                    blockList->blocks[i].offset,
                                    blockList->blocks[i].length,
                                    buffer.data());
            if (VIX_FAILED(error)) {
                lastError_ = "Failed to write blocks";
                return false;
            }
        }
    }

    return true;
}

bool DiskBackup::restore() {
    VixDiskLibCreateParams createParams;
    memset(&createParams, 0, sizeof(createParams));
    createParams.adapterType = VIXDISKLIB_ADAPTER_IDE;
    createParams.diskType = VIXDISKLIB_DISK_MONOLITHIC_FLAT;

    VixError error = VixDiskLib_Open(connection_->getVDDKConnection(),
                                    const_cast<char*>(backupPath_.c_str()),
                                    VIXDISKLIB_FLAG_OPEN_UNBUFFERED,
                                    &backupDisk_);
    if (VIX_FAILED(error)) {
        lastError_ = "Failed to open backup disk";
        return false;
    }

    error = VixDiskLib_Open(connection_->getVDDKConnection(),
                           const_cast<char*>(sourcePath_.c_str()),
                           VIXDISKLIB_FLAG_OPEN_UNBUFFERED,
                           &sourceDisk_);
    if (VIX_FAILED(error)) {
        VixDiskLib_Close(backupDisk_);
        backupDisk_ = nullptr;
        lastError_ = "Failed to open source disk";
        return false;
    }

    error = VixDiskLib_Clone(connection_->getVDDKConnection(),
                            backupPath_.c_str(),
                            connection_->getVDDKConnection(),
                            sourcePath_.c_str(),
                            &createParams,
                            progressFunc,
                            this,
                            TRUE);
    if (VIX_FAILED(error)) {
        lastError_ = "Failed to restore disk";
        return false;
    }

    return true;
}

char DiskBackup::progressFunc(void* data, int percent) {
    auto* backup = static_cast<DiskBackup*>(data);
    backup->progress_ = static_cast<double>(percent) / 100.0;
    if (backup->progressCallback_) {
        backup->progressCallback_(backup->progress_);
    }
    return 0;
} 