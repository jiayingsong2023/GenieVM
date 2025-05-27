#include "backup/kvm/kvm_backup_provider.hpp"
#include "common/logger.hpp"
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <sstream>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include <openssl/evp.h>
#include <openssl/err.h>
#include <chrono>
#include <thread>

KVMBackupProvider::KVMBackupProvider()
    : conn_(nullptr)
    , lastError_("")
    , progress_(0.0) {
}

KVMBackupProvider::~KVMBackupProvider() {
    disconnect();
}

bool KVMBackupProvider::connect(const std::string& host, const std::string& username, const std::string& password) {
    if (isConnected()) {
        disconnect();
    }

    std::string uri = "qemu+ssh://" + username + "@" + host + "/system";
    conn_ = virConnectOpenAuth(uri.c_str(),
                             virConnectAuthPtrDefault,
                             VIR_CONNECT_RO);

    if (!conn_) {
        lastError_ = "Failed to connect to KVM host: " + std::string(virGetLastErrorMessage());
        return false;
    }

    return true;
}

void KVMBackupProvider::disconnect() {
    if (conn_) {
        virConnectClose(conn_);
        conn_ = nullptr;
    }
}

bool KVMBackupProvider::isConnected() const {
    return conn_ != nullptr;
}

bool KVMBackupProvider::getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths) {
    if (!isConnected()) {
        lastError_ = "Not connected to KVM host";
        return false;
    }
    // Dummy implementation
    diskPaths.push_back("/var/lib/libvirt/images/" + vmId + ".qcow2");
    return true;
}

bool KVMBackupProvider::createSnapshot(const std::string& vmId, std::string& snapshotId) {
    if (!isConnected()) {
        lastError_ = "Not connected to KVM host";
        return false;
    }
    snapshotId = "backup_snapshot";
    return true;
}

bool KVMBackupProvider::removeSnapshot(const std::string& vmId, const std::string& snapshotId) {
    if (!isConnected()) {
        lastError_ = "Not connected to KVM host";
        return false;
    }
    return true;
}

bool KVMBackupProvider::getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                                       std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) {
    changedBlocks.push_back(std::make_pair(0, 1024 * 1024 * 1024)); // Dummy 1GB block
    return true;
}

std::string KVMBackupProvider::getLastError() const {
    return lastError_;
}

bool KVMBackupProvider::backupDisk(const std::string& vmId, const std::string& diskPath, const BackupConfig& config) {
    progress_ = 0.0;
    while (progress_ < 100.0) {
        progress_ += 10.0;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}

bool KVMBackupProvider::verifyDisk(const std::string& diskPath) {
    return true;
}

bool KVMBackupProvider::listBackups(std::vector<std::string>& backupDirs) {
    return true;
}

bool KVMBackupProvider::deleteBackup(const std::string& backupDir) {
    return true;
}

bool KVMBackupProvider::verifyBackup(const std::string& backupId) {
    return true;
}

bool KVMBackupProvider::restoreDisk(const std::string& vmId, const std::string& diskPath, const RestoreConfig& config) {
    progress_ = 0.0;
    while (progress_ < 100.0) {
        progress_ += 10.0;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return true;
}

void KVMBackupProvider::clearLastError() {
    lastError_.clear();
}

double KVMBackupProvider::getProgress() const {
    return progress_;
}

bool KVMBackupProvider::initializeCBT(const std::string& vmId) {
    std::vector<std::string> diskPaths;
    if (!getVMDiskPaths(vmId, diskPaths)) {
        return false;
    }

    for (const auto& diskPath : diskPaths) {
        auto cbt = cbtFactory_->createCBT(diskPath);
        if (!cbt || !cbt->enable()) {
            lastError_ = "Failed to initialize CBT for disk: " + diskPath;
            return false;
        }
    }

    return true;
}

bool KVMBackupProvider::cleanupCBT(const std::string& vmId) {
    std::vector<std::string> diskPaths;
    if (!getVMDiskPaths(vmId, diskPaths)) {
        return false;
    }

    for (const auto& diskPath : diskPaths) {
        auto cbt = cbtFactory_->createCBT(diskPath);
        if (!cbt || !cbt->disable()) {
            lastError_ = "Failed to cleanup CBT for disk: " + diskPath;
            return false;
        }
    }

    return true;
}

std::string KVMBackupProvider::getDiskFormat(const std::string& diskPath) const {
    // Check file extension
    std::filesystem::path path(diskPath);
    std::string ext = path.extension().string();
    
    if (ext == ".qcow2") {
        return "qcow2";
    } else if (ext == ".raw") {
        return "raw";
    }
    
    // Default to qcow2
    return "qcow2";
}

bool KVMBackupProvider::verifyDiskIntegrity(const std::string& diskPath) {
    try {
        // For qcow2 format, we can use qemu-img check
        if (getDiskFormat(diskPath) == "qcow2") {
            std::string cmd = "qemu-img check " + diskPath;
            int result = std::system(cmd.c_str());
            return result == 0;
        }
        
        // For raw format, we can do basic file checks
        std::ifstream file(diskPath, std::ios::binary);
        if (!file.is_open()) {
            return false;
        }

        // Check if file is readable and has valid size
        file.seekg(0, std::ios::end);
        std::streamsize size = file.tellg();
        return size > 0;
    } catch (const std::exception& e) {
        lastError_ = std::string("Failed to verify disk integrity: ") + e.what();
        return false;
    }
}

std::string KVMBackupProvider::calculateChecksum(const std::string& filePath) {
    try {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            return "";
        }

        // Initialize OpenSSL context
        EVP_MD_CTX* ctx = EVP_MD_CTX_new();
        if (!ctx) {
            lastError_ = "Failed to create OpenSSL context";
            return "";
        }

        // Initialize digest
        if (EVP_DigestInit_ex(ctx, EVP_sha256(), nullptr) != 1) {
            EVP_MD_CTX_free(ctx);
            lastError_ = "Failed to initialize digest";
            return "";
        }

        // Read and update digest
        char buffer[4096];
        while (file.good()) {
            file.read(buffer, sizeof(buffer));
            if (file.gcount() > 0) {
                if (EVP_DigestUpdate(ctx, buffer, file.gcount()) != 1) {
                    EVP_MD_CTX_free(ctx);
                    lastError_ = "Failed to update digest";
                    return "";
                }
            }
        }

        // Finalize digest
        unsigned char hash[EVP_MAX_MD_SIZE];
        unsigned int hashLen;
        if (EVP_DigestFinal_ex(ctx, hash, &hashLen) != 1) {
            EVP_MD_CTX_free(ctx);
            lastError_ = "Failed to finalize digest";
            return "";
        }

        // Clean up
        EVP_MD_CTX_free(ctx);

        // Convert hash to hex string
        std::stringstream ss;
        for (unsigned int i = 0; i < hashLen; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        return ss.str();
    } catch (const std::exception& e) {
        lastError_ = std::string("Failed to calculate checksum: ") + e.what();
        return "";
    }
} 