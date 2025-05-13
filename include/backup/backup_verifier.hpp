#pragma once

#include <string>
#include <functional>
#include <memory>

namespace vmware {

struct VerificationResult {
    bool success;
    std::string errorMessage;
    size_t bytesVerified;
    size_t totalBytes;
};

class BackupVerifier {
public:
    BackupVerifier(const std::string& sourcePath, const std::string& backupPath);
    ~BackupVerifier();

    bool initialize();
    bool verifyFull();
    bool verifyIncremental();
    void setProgressCallback(std::function<void(double)> callback);
    const VerificationResult& getResult() const { return result_; }

private:
    std::string sourcePath_;
    std::string backupPath_;
    std::function<void(double)> progressCallback_;
    VerificationResult result_;
    std::unique_ptr<class BackupVerifierImpl> impl_;
};

} // namespace vmware 