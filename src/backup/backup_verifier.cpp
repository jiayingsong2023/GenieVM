#include "backup/backup_verifier.hpp"
#include "common/logger.hpp"
#include <fstream>
#include <filesystem>
#include <chrono>
#include <vector>
#include <cstring>

namespace vmware {

class BackupVerifierImpl {
public:
    BackupVerifierImpl(const std::string& sourcePath, const std::string& backupPath)
        : sourcePath_(sourcePath), backupPath_(backupPath) {}

    bool initialize() {
        try {
            if (!std::filesystem::exists(sourcePath_)) {
                LOG_ERROR("Source path does not exist: {}", sourcePath_);
                return false;
            }
            if (!std::filesystem::exists(backupPath_)) {
                LOG_ERROR("Backup path does not exist: {}", backupPath_);
                return false;
            }
            return true;
        } catch (const std::exception& e) {
            LOG_ERROR("Failed to initialize verifier: {}", e.what());
            return false;
        }
    }

    bool verifyFull(std::function<void(double)> progressCallback, VerificationResult& result) {
        try {
            std::ifstream sourceFile(sourcePath_, std::ios::binary);
            std::ifstream backupFile(backupPath_, std::ios::binary);

            if (!sourceFile || !backupFile) {
                result.errorMessage = "Failed to open files for verification";
                return false;
            }

            // Get file sizes
            sourceFile.seekg(0, std::ios::end);
            backupFile.seekg(0, std::ios::end);
            size_t sourceSize = sourceFile.tellg();
            size_t backupSize = backupFile.tellg();

            if (sourceSize != backupSize) {
                result.errorMessage = "File sizes do not match";
                return false;
            }

            // Reset file positions
            sourceFile.seekg(0);
            backupFile.seekg(0);

            // Compare files
            const size_t bufferSize = 1024 * 1024; // 1MB buffer
            std::vector<char> sourceBuffer(bufferSize);
            std::vector<char> backupBuffer(bufferSize);
            size_t bytesVerified = 0;

            while (sourceFile && backupFile) {
                sourceFile.read(sourceBuffer.data(), bufferSize);
                backupFile.read(backupBuffer.data(), bufferSize);

                size_t sourceRead = sourceFile.gcount();
                size_t backupRead = backupFile.gcount();

                if (sourceRead != backupRead) {
                    result.errorMessage = "Read sizes do not match";
                    return false;
                }

                if (memcmp(sourceBuffer.data(), backupBuffer.data(), sourceRead) != 0) {
                    result.errorMessage = "Data mismatch detected";
                    return false;
                }

                bytesVerified += sourceRead;
                if (progressCallback) {
                    progressCallback(static_cast<double>(bytesVerified) / sourceSize);
                }
            }

            result.success = true;
            result.bytesVerified = bytesVerified;
            result.totalBytes = sourceSize;
            return true;
        } catch (const std::exception& e) {
            result.errorMessage = std::string("Verification failed: ") + e.what();
            return false;
        }
    }

private:
    std::string sourcePath_;
    std::string backupPath_;
};

BackupVerifier::BackupVerifier(const std::string& sourcePath, const std::string& backupPath)
    : sourcePath_(sourcePath), backupPath_(backupPath), impl_(std::make_unique<BackupVerifierImpl>(sourcePath, backupPath)) {
    result_.success = false;
    result_.bytesVerified = 0;
    result_.totalBytes = 0;
}

BackupVerifier::~BackupVerifier() = default;

bool BackupVerifier::initialize() {
    return impl_->initialize();
}

bool BackupVerifier::verifyFull() {
    return impl_->verifyFull(progressCallback_, result_);
}

bool BackupVerifier::verifyIncremental() {
    // TODO: Implement incremental verification
    result_.errorMessage = "Incremental verification not implemented";
    return false;
}

void BackupVerifier::setProgressCallback(std::function<void(double)> callback) {
    progressCallback_ = std::move(callback);
}

} // namespace vmware
