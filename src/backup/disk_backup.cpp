#include "backup/disk_backup.hpp"
#include <cstring>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <algorithm>
#include <filesystem>
#include <iostream>

namespace fs = std::filesystem;

// VDDK constants
#define VIXDISKLIB_VERSION_MAJOR 8
#define VIXDISKLIB_VERSION_MINOR 0

// VDDK structure definitions
struct VDDKCreateParamsImpl {
    uint32_t adapterType;
    uint32_t diskType;
};

struct VDDKInfoImpl {
    uint64_t capacity;
};

DiskBackup::DiskBackup()
    : sourceHandle_(nullptr)
    , targetHandle_(nullptr) {
    // Initialize VDDK
    VixError err = VixDiskLib_InitWrapper(VIXDISKLIB_VERSION_MAJOR,
                                        VIXDISKLIB_VERSION_MINOR,
                                        nullptr);
    if (VIX_FAILED(err)) {
        throw std::runtime_error("Failed to initialize VDDK");
    }
}

DiskBackup::~DiskBackup() {
    closeDisks();
    VixDiskLib_ExitWrapper();
}

bool DiskBackup::openDisks(const std::string& sourcePath, const std::string& targetPath, uint64_t flags) {
    // Open source disk
    VixError err = VixDiskLib_OpenWrapper(nullptr, sourcePath.c_str(), flags, &sourceHandle_);
    if (VIX_FAILED(err)) {
        std::cerr << "Failed to open source disk: " << sourcePath << std::endl;
        return false;
    }

    // Create target disk
    VixDiskLibCreateParams createParams;
    memset(&createParams, 0, sizeof(createParams));
    createParams.diskType = static_cast<VixDiskLibDiskType>(VIXDISKLIB_DISK_MONOLITHIC_SPARSE);
    createParams.adapterType = static_cast<VixDiskLibAdapterType>(VIXDISKLIB_ADAPTER_SCSI_BUSLOGIC);
    createParams.hwVersion = VIXDISKLIB_HWVERSION_WORKSTATION_5;

    err = VixDiskLib_CreateWrapper(nullptr, targetPath.c_str(), &createParams,
                                 nullptr, nullptr);
    if (VIX_FAILED(err)) {
        std::cerr << "Failed to create target disk: " << targetPath << std::endl;
        VixDiskLib_CloseWrapper(&sourceHandle_);
        return false;
    }

    // Open target disk
    err = VixDiskLib_OpenWrapper(nullptr, targetPath.c_str(), flags, &targetHandle_);
    if (VIX_FAILED(err)) {
        std::cerr << "Failed to open target disk: " << targetPath << std::endl;
        VixDiskLib_CloseWrapper(&sourceHandle_);
        return false;
    }

    return true;
}

bool DiskBackup::backupDisk(std::function<void(int)> progressCallback) {
    if (!sourceHandle_ || !targetHandle_) {
        std::cerr << "Source or target disk not opened" << std::endl;
        return false;
    }

    // Get disk info
    VixDiskLibInfo* diskInfo = nullptr;
    VixError err = VixDiskLib_GetInfoWrapper(sourceHandle_, &diskInfo);
    if (VIX_FAILED(err)) {
        std::cerr << "Failed to get disk info" << std::endl;
        return false;
    }

    // Get disk capacity
    uint64_t capacity = diskInfo->capacity;
    uint64_t totalSectors = capacity / VIXDISKLIB_SECTOR_SIZE;
    uint64_t sectorsProcessed = 0;

    // Read and write in chunks
    const uint64_t CHUNK_SIZE = 1024 * 1024; // 1MB chunks
    std::vector<uint8_t> buffer(CHUNK_SIZE);
    uint64_t sectorsPerChunk = CHUNK_SIZE / VIXDISKLIB_SECTOR_SIZE;

    while (sectorsProcessed < totalSectors) {
        uint64_t sectorsToProcess = std::min(sectorsPerChunk, totalSectors - sectorsProcessed);
        
        // Read from source
        err = VixDiskLib_ReadWrapper(sourceHandle_, sectorsProcessed, sectorsToProcess, buffer.data());
        if (VIX_FAILED(err)) {
            std::cerr << "Failed to read from source disk" << std::endl;
            VixDiskLib_FreeInfoWrapper(diskInfo);
            return false;
        }

        // Write to target
        err = VixDiskLib_WriteWrapper(targetHandle_, sectorsProcessed, sectorsToProcess, buffer.data());
        if (VIX_FAILED(err)) {
            std::cerr << "Failed to write to target disk" << std::endl;
            VixDiskLib_FreeInfoWrapper(diskInfo);
            return false;
        }

        sectorsProcessed += sectorsToProcess;
        
        // Report progress
        int progress = static_cast<int>((sectorsProcessed * 100) / totalSectors);
        if (progressCallback) {
            progressCallback(progress);
        }
    }

    VixDiskLib_FreeInfoWrapper(diskInfo);
    return true;
}

bool DiskBackup::verifyBackup() {
    if (!sourceHandle_ || !targetHandle_) {
        std::cerr << "Source or target disk not opened" << std::endl;
        return false;
    }

    // Get disk info for both source and target
    VixDiskLibInfo* sourceInfo = nullptr;
    VixDiskLibInfo* targetInfo = nullptr;
    
    VixError err = VixDiskLib_GetInfoWrapper(sourceHandle_, &sourceInfo);
    if (VIX_FAILED(err)) {
        std::cerr << "Failed to get source disk info" << std::endl;
        return false;
    }

    err = VixDiskLib_GetInfoWrapper(targetHandle_, &targetInfo);
    if (VIX_FAILED(err)) {
        std::cerr << "Failed to get target disk info" << std::endl;
        VixDiskLib_FreeInfoWrapper(sourceInfo);
        return false;
    }

    // Compare disk capacities
    if (sourceInfo->capacity != targetInfo->capacity) {
        std::cerr << "Disk capacity mismatch" << std::endl;
        VixDiskLib_FreeInfoWrapper(sourceInfo);
        VixDiskLib_FreeInfoWrapper(targetInfo);
        return false;
    }

    VixDiskLib_FreeInfoWrapper(sourceInfo);
    VixDiskLib_FreeInfoWrapper(targetInfo);
    return true;
}

void DiskBackup::closeDisks() {
    if (sourceHandle_) {
        VixDiskLib_CloseWrapper(&sourceHandle_);
        sourceHandle_ = nullptr;
    }
    if (targetHandle_) {
        VixDiskLib_CloseWrapper(&targetHandle_);
        targetHandle_ = nullptr;
    }
}

std::string DiskBackup::getLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
} 