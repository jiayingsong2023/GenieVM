#include "backup/backup_verifier.hpp"
#include "common/logger.hpp"
#include <fstream>
#include <filesystem>
#include <vector>
#include <cstring>

BackupVerifier::BackupVerifier(const std::string& sourcePath, const std::string& backupPath)
    : sourcePath_(sourcePath)
    , backupPath_(backupPath)
    , result_({false, ""}) {
}

BackupVerifier::~BackupVerifier() = default;

bool BackupVerifier::initialize() {
    if (!std::filesystem::exists(sourcePath_)) {
        result_.errorMessage = "Source path does not exist: " + sourcePath_;
        return false;
    }

    if (!std::filesystem::exists(backupPath_)) {
        result_.errorMessage = "Backup path does not exist: " + backupPath_;
        return false;
    }

    return true;
}

bool BackupVerifier::verifyFull() {
    try {
        std::ifstream source(sourcePath_, std::ios::binary);
        std::ifstream backup(backupPath_, std::ios::binary);

        if (!source || !backup) {
            result_.errorMessage = "Failed to open files for verification";
            return false;
        }

        // Get file sizes
        source.seekg(0, std::ios::end);
        backup.seekg(0, std::ios::end);
        size_t sourceSize = source.tellg();
        size_t backupSize = backup.tellg();

        if (sourceSize != backupSize) {
            result_.errorMessage = "File sizes do not match";
            return false;
        }

        // Reset file positions
        source.seekg(0);
        backup.seekg(0);

        // Compare files block by block
        const size_t blockSize = 1024 * 1024;  // 1MB blocks
        std::vector<char> sourceBuffer(blockSize);
        std::vector<char> backupBuffer(blockSize);
        size_t totalBytes = 0;

        while (source && backup) {
            source.read(sourceBuffer.data(), blockSize);
            backup.read(backupBuffer.data(), blockSize);

            size_t sourceBytes = source.gcount();
            size_t backupBytes = backup.gcount();

            if (sourceBytes != backupBytes) {
                result_.errorMessage = "Files differ in content";
                return false;
            }

            if (memcmp(sourceBuffer.data(), backupBuffer.data(), sourceBytes) != 0) {
                result_.errorMessage = "Files differ in content";
                return false;
            }

            totalBytes += sourceBytes;
            if (progressCallback_) {
                progressCallback_(static_cast<double>(totalBytes) / sourceSize);
            }
        }

        result_.success = true;
        return true;
    } catch (const std::exception& e) {
        result_.errorMessage = "Verification failed: " + std::string(e.what());
        return false;
    }
}

bool BackupVerifier::verifyIncremental() {
    result_.errorMessage = "Incremental verification not implemented";
    return false;
}

void BackupVerifier::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = callback;
}

VerificationResult BackupVerifier::getResult() const {
    return result_;
}
