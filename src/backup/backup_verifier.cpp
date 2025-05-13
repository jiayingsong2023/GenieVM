#include "backup/backup_verifier.hpp"
#include <filesystem>
#include <cstring>
#include <openssl/sha.h>

namespace vmware {

BackupVerifier::BackupVerifier(const std::string& sourcePath, const std::string& backupPath)
    : sourcePath_(sourcePath)
    , backupPath_(backupPath)
    , connection_(nullptr)
    , sourceDisk_(nullptr)
    , backupDisk_(nullptr) {
    result_ = {false, "", {}, 0, 0, 0};
}

BackupVerifier::~BackupVerifier() {
    cleanup();
}

bool BackupVerifier::initialize() {
    VixError error = VIX_OK;
    VixDiskLibConnectParams connectParams = {0};
    
    // Initialize VDDK
    error = VixDiskLib_Init(1, 0, nullptr, nullptr, nullptr, nullptr);
    if (VIX_FAILED(error)) {
        result_.errorMessage = "Failed to initialize VDDK: " + std::to_string(error);
        return false;
    }

    // Connect to vSphere
    error = VixDiskLib_Connect(&connectParams, &connection_);
    if (VIX_FAILED(error)) {
        result_.errorMessage = "Failed to connect to vSphere: " + std::to_string(error);
        return false;
    }

    return true;
}

void BackupVerifier::cleanup() {
    closeDisks();
    if (connection_) {
        VixDiskLib_Disconnect(connection_);
        connection_ = nullptr;
    }
}

bool BackupVerifier::openDisks() {
    VixError error = VIX_OK;
    
    // Open source disk
    error = VixDiskLib_Open(connection_,
                           sourcePath_.c_str(),
                           VIXDISKLIB_FLAG_OPEN_READ_ONLY,
                           &sourceDisk_);
    if (VIX_FAILED(error)) {
        result_.errorMessage = "Failed to open source disk: " + std::to_string(error);
        return false;
    }

    // Open backup disk
    error = VixDiskLib_Open(connection_,
                           backupPath_.c_str(),
                           VIXDISKLIB_FLAG_OPEN_READ_ONLY,
                           &backupDisk_);
    if (VIX_FAILED(error)) {
        result_.errorMessage = "Failed to open backup disk: " + std::to_string(error);
        return false;
    }

    return true;
}

void BackupVerifier::closeDisks() {
    if (sourceDisk_) {
        VixDiskLib_Close(sourceDisk_);
        sourceDisk_ = nullptr;
    }
    if (backupDisk_) {
        VixDiskLib_Close(backupDisk_);
        backupDisk_ = nullptr;
    }
}

bool BackupVerifier::verifyFull() {
    if (!initialize() || !openDisks()) {
        return false;
    }

    VixDiskLibDiskInfo diskInfo;
    VixError error = VixDiskLib_GetInfo(sourceDisk_, &diskInfo);
    if (VIX_FAILED(error)) {
        result_.errorMessage = "Failed to get disk info: " + std::to_string(error);
        return false;
    }

    // Verify all blocks
    const size_t bufferSize = 1024 * 1024;  // 1MB buffer
    std::vector<uint8_t> sourceBuffer(bufferSize);
    std::vector<uint8_t> backupBuffer(bufferSize);
    
    uint64_t totalSectors = diskInfo.capacity;
    uint64_t sectorsProcessed = 0;
    result_.totalBlocks = totalSectors;
    result_.verifiedBlocks = 0;
    result_.failedBlockCount = 0;

    while (sectorsProcessed < totalSectors && result_.failedBlockCount < 100) {  // Limit failed blocks for performance
        uint32_t sectorsToRead = std::min(
            static_cast<uint32_t>(bufferSize / VIXDISKLIB_SECTOR_SIZE),
            static_cast<uint32_t>(totalSectors - sectorsProcessed)
        );

        // Read from source
        error = VixDiskLib_Read(sourceDisk_,
                               sectorsProcessed,
                               sectorsToRead,
                               sourceBuffer.data());
        if (VIX_FAILED(error)) {
            result_.errorMessage = "Failed to read source disk: " + std::to_string(error);
            return false;
        }

        // Read from backup
        error = VixDiskLib_Read(backupDisk_,
                               sectorsProcessed,
                               sectorsToRead,
                               backupBuffer.data());
        if (VIX_FAILED(error)) {
            result_.errorMessage = "Failed to read backup disk: " + std::to_string(error);
            return false;
        }

        // Compare blocks
        if (memcmp(sourceBuffer.data(), backupBuffer.data(), sectorsToRead * VIXDISKLIB_SECTOR_SIZE) != 0) {
            result_.failedBlocks.push_back("Block at offset " + std::to_string(sectorsProcessed));
            result_.failedBlockCount++;
        }

        sectorsProcessed += sectorsToRead;
        result_.verifiedBlocks = sectorsProcessed;
        
        double progress = static_cast<double>(sectorsProcessed) / totalSectors;
        reportProgress(progress);
    }

    result_.success = result_.failedBlockCount == 0;
    if (!result_.success) {
        result_.errorMessage = "Verification failed: " + std::to_string(result_.failedBlockCount) + " blocks differ";
    }

    closeDisks();
    return result_.success;
}

bool BackupVerifier::verifyIncremental() {
    if (!initialize() || !openDisks()) {
        return false;
    }

    VixDiskLibBlockList* blockList = nullptr;
    VixError error = VixDiskLib_QueryChangedBlocks(sourceDisk_,
                                                  backupDisk_,
                                                  0,
                                                  &blockList);
    if (VIX_FAILED(error)) {
        result_.errorMessage = "Failed to query changed blocks: " + std::to_string(error);
        return false;
    }

    result_.totalBlocks = blockList->numBlocks;
    result_.verifiedBlocks = 0;
    result_.failedBlockCount = 0;

    bool success = verifyBlocks(blockList);
    VixDiskLib_FreeBlockList(blockList);
    closeDisks();

    result_.success = success && result_.failedBlockCount == 0;
    if (!result_.success) {
        result_.errorMessage = "Verification failed: " + std::to_string(result_.failedBlockCount) + " blocks differ";
    }

    return result_.success;
}

bool BackupVerifier::verifyBlocks(VixDiskLibBlockList* blockList) {
    if (!blockList) {
        return false;
    }

    const size_t bufferSize = 1024 * 1024;  // 1MB buffer
    std::vector<uint8_t> sourceBuffer(bufferSize);
    std::vector<uint8_t> backupBuffer(bufferSize);
    
    for (size_t i = 0; i < blockList->numBlocks && result_.failedBlockCount < 100; ++i) {
        if (blockList->changedArea[i].length > 0) {
            uint64_t offset = blockList->changedArea[i].offset;
            uint32_t length = blockList->changedArea[i].length;

            // Read from source
            VixError error = VixDiskLib_Read(sourceDisk_,
                                            offset,
                                            length,
                                            sourceBuffer.data());
            if (VIX_FAILED(error)) {
                result_.errorMessage = "Failed to read source disk: " + std::to_string(error);
                return false;
            }

            // Read from backup
            error = VixDiskLib_Read(backupDisk_,
                                   offset,
                                   length,
                                   backupBuffer.data());
            if (VIX_FAILED(error)) {
                result_.errorMessage = "Failed to read backup disk: " + std::to_string(error);
                return false;
            }

            // Compare blocks
            if (memcmp(sourceBuffer.data(), backupBuffer.data(), length * VIXDISKLIB_SECTOR_SIZE) != 0) {
                result_.failedBlocks.push_back("Block at offset " + std::to_string(offset));
                result_.failedBlockCount++;
            }

            result_.verifiedBlocks++;
            double progress = static_cast<double>(i + 1) / blockList->numBlocks;
            reportProgress(progress);
        }
    }

    return true;
}

void BackupVerifier::reportProgress(double progress) {
    if (progressCallback_) {
        progressCallback_(progress);
    }
}

void VIX_CALLBACK BackupVerifier::progressFunc(void* data, int percent) {
    auto* verifier = static_cast<BackupVerifier*>(data);
    verifier->reportProgress(percent / 100.0);
}

std::vector<uint8_t> BackupVerifier::calculateChecksum(const uint8_t* data, size_t length) {
    std::vector<uint8_t> hash(SHA256_DIGEST_LENGTH);
    SHA256_CTX sha256;
    SHA256_Init(&sha256);
    SHA256_Update(&sha256, data, length);
    SHA256_Final(hash.data(), &sha256);
    return hash;
}

bool BackupVerifier::verifyBlockChecksum(uint64_t offset, uint32_t length) {
    const size_t bufferSize = 1024 * 1024;  // 1MB buffer
    std::vector<uint8_t> sourceBuffer(bufferSize);
    std::vector<uint8_t> backupBuffer(bufferSize);

    // Read from source
    VixError error = VixDiskLib_Read(sourceDisk_,
                                    offset,
                                    length,
                                    sourceBuffer.data());
    if (VIX_FAILED(error)) {
        return false;
    }

    // Read from backup
    error = VixDiskLib_Read(backupDisk_,
                           offset,
                           length,
                           backupBuffer.data());
    if (VIX_FAILED(error)) {
        return false;
    }

    // Calculate and compare checksums
    auto sourceChecksum = calculateChecksum(sourceBuffer.data(), length * VIXDISKLIB_SECTOR_SIZE);
    auto backupChecksum = calculateChecksum(backupBuffer.data(), length * VIXDISKLIB_SECTOR_SIZE);

    return sourceChecksum == backupChecksum;
}

} // namespace vmware 