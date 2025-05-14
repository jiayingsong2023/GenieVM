#pragma once

#include <string>
#include <functional>
#include <memory>

struct VerificationResult {
    bool success;
    std::string errorMessage;
};

class BackupVerifier {
public:
    using ProgressCallback = std::function<void(double)>;

    BackupVerifier(const std::string& sourcePath, const std::string& backupPath);
    ~BackupVerifier();

    bool initialize();
    bool verifyFull();
    bool verifyIncremental();
    void setProgressCallback(ProgressCallback callback);
    VerificationResult getResult() const;

private:
    std::string sourcePath_;
    std::string backupPath_;
    ProgressCallback progressCallback_;
    VerificationResult result_;
}; 