#include "backup/disk_backup.hpp"
#include "common/logger.hpp"
#include <filesystem>

namespace vmware {

DiskBackup::DiskBackup(const std::string& sourcePath, const std::string& targetPath)
    : sourcePath_(sourcePath)
    , targetPath_(targetPath)
    , connection_(nullptr)
    , sourceDisk_(nullptr)
    , targetDisk_(nullptr) {
}

DiskBackup::~DiskBackup() {
    closeDisks();
    if (connection_) {
        VixDiskLib_Disconnect(connection_);
    }
}

bool DiskBackup::initialize() {
    VixError error = VIX_OK;
    VixDiskLibConnectParams connectParams = {0};
    
    // Initialize VDDK
    error = VixDiskLib_Init(1, 0, nullptr, nullptr, nullptr, nullptr);
    if (VIX_FAILED(error)) {
        Logger::error("Failed to initialize VDDK: " + std::to_string(error));
        return false;
    }

    // Connect to vSphere
    error = VixDiskLib_Connect(&connectParams, &connection_);
    if (VIX_FAILED(error)) {
        Logger::error("Failed to connect to vSphere: " + std::to_string(error));
        return false;
    }

    return true;
}

bool DiskBackup::openDisks() {
    VixError error = VIX_OK;
    
    // Open source disk
    error = VixDiskLib_Open(connection_,
                           sourcePath_.c_str(),
                           VIXDISKLIB_FLAG_OPEN_READ_ONLY,
                           &sourceDisk_);
    if (VIX_FAILED(error)) {
        Logger::error("Failed to open source disk: " + std::to_string(error));
        return false;
    }

    // Create target disk
    VixDiskLibCreateParams createParams = {0};
    createParams.diskType = VIXDISKLIB_DISK_STREAM_OPTIMIZED;
    createParams.adapterType = VIXDISKLIB_ADAPTER_SCSI_LSILOGIC;
    createParams.hwVersion = VIXDISKLIB_HWVERSION_WORKSTATION_5;

    error = VixDiskLib_Create(connection_,
                             targetPath_.c_str(),
                             &createParams,
                             nullptr,
                             nullptr);
    if (VIX_FAILED(error)) {
        Logger::error("Failed to create target disk: " + std::to_string(error));
        return false;
    }

    // Open target disk
    error = VixDiskLib_Open(connection_,
                           targetPath_.c_str(),
                           VIXDISKLIB_FLAG_OPEN_UNBUFFERED,
                           &targetDisk_);
    if (VIX_FAILED(error)) {
        Logger::error("Failed to open target disk: " + std::to_string(error));
        return false;
    }

    return true;
}

void DiskBackup::closeDisks() {
    if (sourceDisk_) {
        VixDiskLib_Close(sourceDisk_);
        sourceDisk_ = nullptr;
    }
    if (targetDisk_) {
        VixDiskLib_Close(targetDisk_);
        targetDisk_ = nullptr;
    }
}

bool DiskBackup::backupFull() {
    if (!openDisks()) {
        return false;
    }

    VixDiskLibDiskInfo diskInfo;
    VixError error = VixDiskLib_GetInfo(sourceDisk_, &diskInfo);
    if (VIX_FAILED(error)) {
        Logger::error("Failed to get disk info: " + std::to_string(error));
        return false;
    }

    // Copy all blocks
    error = VixDiskLib_Clone(sourceDisk_,
                            targetDisk_,
                            VIXDISKLIB_CLONE_MODE_FULL,
                            progressFunc,
                            this);
    if (VIX_FAILED(error)) {
        Logger::error("Failed to clone disk: " + std::to_string(error));
        return false;
    }

    closeDisks();
    return true;
}

bool DiskBackup::backupIncremental() {
    if (!openDisks()) {
        return false;
    }

    VixDiskLibBlockList* blockList = nullptr;
    VixError error = VixDiskLib_QueryChangedBlocks(sourceDisk_,
                                                  targetDisk_,
                                                  0,
                                                  &blockList);
    if (VIX_FAILED(error)) {
        Logger::error("Failed to query changed blocks: " + std::to_string(error));
        return false;
    }

    bool success = copyBlocks(blockList);
    VixDiskLib_FreeBlockList(blockList);
    closeDisks();
    return success;
}

bool DiskBackup::copyBlocks(VixDiskLibBlockList* blockList) {
    if (!blockList) {
        return false;
    }

    const size_t bufferSize = 1024 * 1024;  // 1MB buffer
    std::vector<uint8_t> buffer(bufferSize);
    
    for (size_t i = 0; i < blockList->numBlocks; ++i) {
        if (blockList->changedArea[i].length > 0) {
            VixError error = VixDiskLib_Read(sourceDisk_,
                                            blockList->changedArea[i].offset,
                                            blockList->changedArea[i].length,
                                            buffer.data());
            if (VIX_FAILED(error)) {
                Logger::error("Failed to read blocks: " + std::to_string(error));
                return false;
            }

            error = VixDiskLib_Write(targetDisk_,
                                    blockList->changedArea[i].offset,
                                    blockList->changedArea[i].length,
                                    buffer.data());
            if (VIX_FAILED(error)) {
                Logger::error("Failed to write blocks: " + std::to_string(error));
                return false;
            }

            double progress = static_cast<double>(i + 1) / blockList->numBlocks;
            reportProgress(progress);
        }
    }

    return true;
}

void DiskBackup::reportProgress(double progress) {
    if (progressCallback_) {
        progressCallback_(progress);
    }
}

void VIX_CALLBACK DiskBackup::progressFunc(void* data, int percent) {
    auto* backup = static_cast<DiskBackup*>(data);
    backup->reportProgress(percent / 100.0);
}

} // namespace vmware 