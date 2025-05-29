#include "backup/vmware/vmware_backup_provider.hpp"
#include "backup/backup_job.hpp"
#include "common/parallel_task_manager.hpp"
#include "common/vmware_connection.hpp"
#include "common/backup_status.hpp"
#include "common/vsphere_rest_client.hpp"
#include <sstream>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <regex>
#include <openssl/evp.h>
#include <openssl/aes.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <set>
#include <iomanip>
#include <optional>
#include "vddk_wrapper/vddk_wrapper.h"
#include "common/logger.hpp"
#include <memory>
#include <algorithm>

namespace fs = std::filesystem;

// VDDK version constants
#define VIXDISKLIB_VERSION_MAJOR 8
#define VIXDISKLIB_VERSION_MINOR 0

// Helper function to convert VixError to string
std::string vixErrorToString(int32_t error) {
    char* errorText = VixDiskLib_GetErrorTextWrapper(error, nullptr, 0);
    std::string result(errorText ? errorText : "Unknown error");
    VixDiskLib_FreeErrorTextWrapper(errorText);
    return result;
}

// RAII wrapper for VixDiskLibHandle
class VDDKDiskHandle {
public:
    explicit VDDKDiskHandle(const std::string& path, uint32_t flags) {
        int32_t vixError = VixDiskLib_OpenWrapper(nullptr, path.c_str(), flags, &handle_);
        if (vixError != VIX_OK) {
            throw std::runtime_error("Failed to open disk: " + path);
        }
    }

    ~VDDKDiskHandle() {
        if (handle_) {
            VixDiskLib_CloseWrapper(&handle_);
        }
    }

    VDDKHandle get() const { return handle_; }

    // Delete copy operations
    VDDKDiskHandle(const VDDKDiskHandle&) = delete;
    VDDKDiskHandle& operator=(const VDDKDiskHandle&) = delete;

    bool isValid() const { return handle_ != nullptr; }

private:
    VDDKHandle handle_{nullptr};
};

// RAII wrapper for VixDiskLibInfo
class VDDKDiskInfo {
public:
    explicit VDDKDiskInfo(VDDKHandle handle) {
        int32_t vixError = VixDiskLib_GetInfoWrapper(handle, &info_);
        if (vixError != VIX_OK) {
            throw std::runtime_error("Failed to get disk info");
        }
    }

    ~VDDKDiskInfo() {
        if (info_) {
            VixDiskLib_FreeInfoWrapper(info_);
        }
    }

    VDDKInfo* get() const { return info_; }

    // Delete copy operations
    VDDKDiskInfo(const VDDKDiskInfo&) = delete;
    VDDKDiskInfo& operator=(const VDDKDiskInfo&) = delete;

private:
    VDDKInfo* info_{nullptr};
};

// RAII wrapper for VDDK connection
/*class VDDKConnectionManager {
public:
    VDDKConnectionManager() {
        int32_t vixError = VixDiskLib_InitWrapper(VIXDISKLIB_VERSION_MAJOR,
                                             VIXDISKLIB_VERSION_MINOR,
                                             nullptr);
        if (vixError != VIX_OK) {
            throw std::runtime_error("Failed to initialize VDDK");
        }
    }

    ~VDDKConnectionManager() {
        VixDiskLib_ExitWrapper();
    }

    // Delete copy operations
    VDDKConnectionManager(const VDDKConnectionManager&) = delete;
    VDDKConnectionManager& operator=(const VDDKConnectionManager&) = delete;
}; 
*/

//VMwareBackupProvider::VMwareBackupProvider()
//    : connection_(nullptr)
//    , progress_(0.0) {
//}

VMwareBackupProvider::VMwareBackupProvider(VMwareConnection* connection)
    : connection_(connection)
    , progress_(0.0) {
    // Just store the connection, don't create a new one
    if (!connection_) {
        throw std::runtime_error("Invalid connection pointer");
    }
}

/*
VMwareBackupProvider::VMwareBackupProvider(const std::string& connectionString)
    : connection_(new VMwareConnection())
    , progress_(0.0) {
    // Parse connection string format: "host:port:username:password"
    std::stringstream ss(connectionString);
    std::string host, port, username, password;
    std::getline(ss, host, ':');
    std::getline(ss, port, ':');
    std::getline(ss, username, ':');
    std::getline(ss, password, ':');

    if (host.empty() || username.empty() || password.empty()) {
        delete connection_;
        throw std::runtime_error("Invalid connection string format. Expected: host:port:username:password");
    }

    // Connect to vCenter
    if (!connection_->connect(host, username, password)) {
        std::string error = connection_->getLastError();
        delete connection_;
        throw std::runtime_error("Failed to connect to vCenter: " + error);
    }
}
*/

VMwareBackupProvider::~VMwareBackupProvider() {
    disconnect();
}

void VMwareBackupProvider::handleError(int32_t error) {
    char* errorText = VixDiskLib_GetErrorTextWrapper(error, nullptr, 0);
    if (errorText) {
        lastError_ = errorText;
        VixDiskLib_FreeErrorTextWrapper(errorText);
    } else {
        lastError_ = "Unknown VDDK error: " + std::to_string(error);
    }
}

bool VMwareBackupProvider::initialize() {
    int32_t vixError = VixDiskLib_InitWrapper(VIXDISKLIB_VERSION_MAJOR,
                                             VIXDISKLIB_VERSION_MINOR,
                                             nullptr);
    if (vixError != VIX_OK) {
        handleError(vixError);
        return false;
    }
    return true;
}

void VMwareBackupProvider::cleanup() {
    VixDiskLib_ExitWrapper();
}

bool VMwareBackupProvider::connect(const std::string& host, const std::string& username, const std::string& password) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connection_) {
        lastError_ = "Connection not initialized";
        return false;
    }
    return connection_->connect(host, username, password);
}

void VMwareBackupProvider::disconnect() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (connection_) {
        connection_->disconnect();
    }
}

bool VMwareBackupProvider::isConnected() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return connection_ && connection_->isConnected();
}

bool VMwareBackupProvider::verifyConnection() {
    if (!connection_ || !connection_->isConnected()) {
        lastError_ = "Not connected to vCenter";
        return false;
    }

    try {
        // Try to list VMs as a connection test
        std::vector<std::string> vms = connection_->listVMs();
        return !vms.empty();
    } catch (const std::exception& e) {
        lastError_ = std::string("Connection verification failed: ") + e.what();
        return false;
    }
}

void VMwareBackupProvider::cleanupActiveOperations() {
    std::lock_guard<std::mutex> lock(mutex_);
    activeOperations_.clear();
}

std::vector<std::string> VMwareBackupProvider::listVMs() const {
    std::vector<std::string> vms;
    if (!connection_ || !connection_->isConnected()) {
        const_cast<VMwareBackupProvider*>(this)->lastError_ = "Not connected";
        return vms;
    }
    return connection_->listVMs();
}

bool VMwareBackupProvider::getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths) {
    std::lock_guard<std::mutex> lock(mutex_);
    return connection_->getVMDiskPaths(vmId, diskPaths);
}

bool VMwareBackupProvider::getVMInfo(const std::string& vmId, std::string& name, std::string& status) const {
    if (!connection_ || !connection_->isConnected()) {
        const_cast<VMwareBackupProvider*>(this)->lastError_ = "Not connected";
        return false;
    }
    return connection_->getVMInfo(vmId, name, status);
}

bool VMwareBackupProvider::createSnapshot(const std::string& vmId, std::string& snapshotId) {
    if (!connection_ || !connection_->isConnected()) {
        Logger::error("Cannot create snapshot: Not connected to vCenter");
        lastError_ = "Not connected";
        return false;
    }

    // Generate unique snapshot name
    currentSnapshotName_ = "backup-snapshot-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    currentVmId_ = vmId;
    
    Logger::info("Creating snapshot for VM: " + vmId);
    Logger::debug("Generated snapshot name: " + currentSnapshotName_);
    
    // Get REST client and create snapshot
    auto* restClient = connection_->getRestClient();
    if (!restClient) {
        Logger::error("Failed to get REST client for snapshot creation");
        lastError_ = "Failed to get REST client";
        return false;
    }

    Logger::debug("Initiating snapshot creation via REST API");
    bool success = restClient->createSnapshot(vmId, currentSnapshotName_, "Snapshot created for backup");
    if (success) {
        snapshotId = currentSnapshotName_;
        Logger::info("Successfully created snapshot: " + snapshotId);
    } else {
        Logger::error("Failed to create snapshot: " + lastError_);
    }
    return success;
}

bool VMwareBackupProvider::removeSnapshot(const std::string& vmId, const std::string& snapshotId) {
    if (snapshotId.empty() || !connection_ || !connection_->isConnected()) {
        return false;
    }

    auto* restClient = connection_->getRestClient();
    if (!restClient) {
        lastError_ = "Failed to get REST client";
        return false;
    }

    bool success = restClient->removeSnapshot(vmId, snapshotId);
    if (success && snapshotId == currentSnapshotName_) {
        currentSnapshotName_.clear();
        currentVmId_.clear();
    }
    return success;
}

void VMwareBackupProvider::cleanupSnapshot() {
    if (!currentSnapshotName_.empty()) {
        removeSnapshot("", currentSnapshotName_);
    }
}

bool VMwareBackupProvider::startBackup(const std::string& vmId, const BackupConfig& config) {
    if (!connection_) {
        lastError_ = "Not connected";
        Logger::error("Backup failed: Not connected to vCenter");
        return false;
    }

    try {
        Logger::info("Starting backup process for VM: " + vmId);
        
        // Get VM info using REST client
        Logger::debug("Getting VM info...");
        nlohmann::json vmInfo;
        if (!connection_->getRestClient()->getVMInfo(vmId, vmInfo)) {
            lastError_ = "Failed to get VM info";
            Logger::error("Backup failed: " + lastError_);
            return false;
        }
        Logger::info("Successfully retrieved VM info");

        // Create backup directory if it doesn't exist
        Logger::debug("Creating backup directory: " + config.backupPath);
        std::filesystem::create_directories(config.backupPath);
        Logger::info("Backup directory created/verified");

        // Create snapshot using REST client
        std::string snapshotName = "backup_" + std::to_string(std::time(nullptr));
        Logger::info("Creating snapshot: " + snapshotName);
        if (!connection_->getRestClient()->createSnapshot(vmId, snapshotName, "Backup snapshot")) {
            lastError_ = "Failed to create snapshot";
            Logger::error("Backup failed: " + lastError_);
            return false;
        }
        Logger::info("Snapshot created successfully");

        // Get VM disk paths
        Logger::debug("Getting VM disk paths...");
        std::vector<std::string> diskPaths;
        if (!connection_->getVMDiskPaths(vmId, diskPaths)) {
            Logger::error("Failed to get VM disk paths, cleaning up snapshot...");
            connection_->getRestClient()->removeSnapshot(vmId, snapshotName); // Cleanup snapshot
            lastError_ = "Failed to get VM disk paths";
            Logger::error("Backup failed: " + lastError_);
            return false;
        }
        Logger::info("Found " + std::to_string(diskPaths.size()) + " disk(s) to backup");

        // Backup each disk
        for (size_t i = 0; i < diskPaths.size(); i++) {
            const auto& diskPath = diskPaths[i];
            Logger::info("Starting backup of disk " + std::to_string(i + 1) + "/" + 
                        std::to_string(diskPaths.size()) + ": " + diskPath);
            
            std::string backupPath = config.backupPath + "/" + std::filesystem::path(diskPath).filename().string();
            Logger::debug("Backup path: " + backupPath);
            
            // Use VDDK to backup the disk
            Logger::debug("Getting VDDK connection...");
            VDDKConnection vddkConn = connection_->getVDDKConnection();
            if (!vddkConn) {
                Logger::error("Failed to get VDDK connection, cleaning up snapshot...");
                connection_->getRestClient()->removeSnapshot(vmId, snapshotName); // Cleanup snapshot
                lastError_ = "Failed to get VDDK connection";
                Logger::error("Backup failed: " + lastError_);
                return false;
            }

            // Open source disk
            Logger::debug("Opening source disk...");
            VDDKHandle srcDisk;
            int32_t vixError = VixDiskLib_OpenWrapper(vddkConn, diskPath.c_str(), VIXDISKLIB_FLAG_OPEN_READ_ONLY, &srcDisk);
            if (vixError != VIX_OK) {
                Logger::error("Failed to open source disk, cleaning up snapshot...");
                connection_->getRestClient()->removeSnapshot(vmId, snapshotName); // Cleanup snapshot
                lastError_ = "Failed to open source disk: " + vixErrorToString(vixError);
                Logger::error("Backup failed: " + lastError_);
                return false;
            }
            Logger::info("Source disk opened successfully");

            // Create target disk
            Logger::debug("Creating target disk...");
            VixDiskLibCreateParams createParams;
            memset(&createParams, 0, sizeof(createParams));
            createParams.diskType = static_cast<VixDiskLibDiskType>(VIXDISKLIB_DISK_MONOLITHIC_SPARSE);
            createParams.adapterType = static_cast<VixDiskLibAdapterType>(VIXDISKLIB_ADAPTER_SCSI_BUSLOGIC);
            createParams.hwVersion = VIXDISKLIB_HWVERSION_WORKSTATION_5;

            vixError = VixDiskLib_CreateWrapper(vddkConn, backupPath.c_str(), &createParams, nullptr, nullptr);
            if (vixError != VIX_OK) {
                Logger::error("Failed to create destination disk, cleaning up...");
                VixDiskLib_CloseWrapper(&srcDisk);
                connection_->getRestClient()->removeSnapshot(vmId, snapshotName); // Cleanup snapshot
                lastError_ = "Failed to create destination disk: " + vixErrorToString(vixError);
                Logger::error("Backup failed: " + lastError_);
                return false;
            }
            Logger::info("Target disk created successfully");

            // Open destination disk
            Logger::debug("Opening destination disk...");
            VDDKHandle dstDisk;
            vixError = VixDiskLib_OpenWrapper(vddkConn, backupPath.c_str(), VIXDISKLIB_FLAG_OPEN_UNBUFFERED, &dstDisk);
            if (vixError != VIX_OK) {
                Logger::error("Failed to open destination disk, cleaning up...");
                VixDiskLib_CloseWrapper(&srcDisk);
                connection_->getRestClient()->removeSnapshot(vmId, snapshotName); // Cleanup snapshot
                lastError_ = "Failed to open destination disk: " + vixErrorToString(vixError);
                Logger::error("Backup failed: " + lastError_);
                return false;
            }
            Logger::info("Destination disk opened successfully");

            // Copy disk contents
            Logger::info("Starting disk copy operation...");
            vixError = VixDiskLib_CloneWrapper(vddkConn, backupPath.c_str(), vddkConn, diskPath.c_str(), &createParams, nullptr, nullptr, FALSE);
            if (vixError != VIX_OK) {
                Logger::error("Failed to copy disk contents, cleaning up...");
                VixDiskLib_CloseWrapper(&srcDisk);
                VixDiskLib_CloseWrapper(&dstDisk);
                connection_->getRestClient()->removeSnapshot(vmId, snapshotName); // Cleanup snapshot
                lastError_ = "Failed to copy disk contents: " + vixErrorToString(vixError);
                Logger::error("Backup failed: " + lastError_);
                return false;
            }
            Logger::info("Disk copy completed successfully");

            // Close disks
            Logger::debug("Closing disk handles...");
            VixDiskLib_CloseWrapper(&srcDisk);
            VixDiskLib_CloseWrapper(&dstDisk);
            Logger::info("Disk " + std::to_string(i + 1) + " backup completed successfully");
        }

        // Remove snapshot after successful backup
        Logger::info("Removing backup snapshot...");
        if (!connection_->getRestClient()->removeSnapshot(vmId, snapshotName)) {
            lastError_ = "Warning: Failed to remove snapshot";
            Logger::warning(lastError_);
            // Continue anyway as the backup was successful
        } else {
            Logger::info("Snapshot removed successfully");
        }

        // Save backup metadata
        Logger::info("Saving backup metadata...");
        if (!saveBackupMetadata(config.backupPath, vmId, diskPaths)) {
            lastError_ = "Warning: Failed to save backup metadata";
            Logger::warning(lastError_);
            // Continue anyway as the backup was successful
        } else {
            Logger::info("Backup metadata saved successfully");
        }

        Logger::info("Backup completed successfully for VM: " + vmId);
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Start backup failed: ") + e.what();
        Logger::error("Backup failed: " + lastError_);
        return false;
    }
}

bool VMwareBackupProvider::cancelBackup(const std::string& vmId) {
    // TODO: Implement cancel functionality
    lastError_ = "Cancel not implemented yet";
    return false;
}

bool VMwareBackupProvider::pauseBackup(const std::string& vmId) {
    // TODO: Implement pause functionality
    lastError_ = "Pause not implemented yet";
    return false;
}

bool VMwareBackupProvider::resumeBackup(const std::string& vmId) {
    // TODO: Implement resume functionality
    lastError_ = "Resume not implemented yet";
    return false;
}

bool VMwareBackupProvider::getBackupStatus(const std::string& backupId, BackupStatus& status) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = activeOperations_.find(backupId);
    if (it == activeOperations_.end()) {
        lastError_ = "Backup not found: " + backupId;
        return false;
    }

    status.state = BackupState::InProgress;
    status.progress = it->second->getProgress();
    status.status = it->second->getStatus();
    return true;
}

bool VMwareBackupProvider::startRestore(const std::string& vmId, const std::string& backupId) {
    if (!connection_ || !connection_->isConnected()) {
        lastError_ = "Not connected";
        return false;
    }

    try {
        updateProgress(0.0, "Starting restore");

        // Load backup metadata
        auto metadata = getLatestBackupInfo(vmId);
        if (!metadata) {
            lastError_ = "Failed to get backup metadata";
            return false;
        }

        // Get VM info
        std::vector<std::string> diskPaths;
        if (!connection_->getVMDiskPaths(vmId, diskPaths)) {
            lastError_ = "Failed to get VM disk paths";
            return false;
        }

        // Create restore config
        RestoreConfig config;
        config.restorePath = backupId;
        config.backupId = backupId;
        config.verifyAfterRestore = true;

        // Restore each disk
        for (const auto& diskPath : diskPaths) {
            updateProgress(0.0, "Restoring disk: " + diskPath);

            if (!restoreDisk(vmId, diskPath, config)) {
                lastError_ = "Failed to restore disk: " + diskPath;
                return false;
            }
        }

        updateProgress(100.0, "Success");
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Start restore failed: ") + e.what();
        return false;
    }
}

bool VMwareBackupProvider::cancelRestore(const std::string& restoreId) {
    if (!connection_ || !connection_->isConnected()) {
        lastError_ = "Not connected";
        return false;
    }

    try {
        updateProgress(0.0, "Cancelling restore");
        updateProgress(100.0, "Success");
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Cancel restore failed: ") + e.what();
        return false;
    }
}

bool VMwareBackupProvider::pauseRestore(const std::string& restoreId) {
    lastError_ = "Pause operation not supported";
    return false;
}

bool VMwareBackupProvider::resumeRestore(const std::string& restoreId) {
    lastError_ = "Resume operation not supported";
    return false;
}

bool VMwareBackupProvider::getRestoreStatus(const std::string& restoreId, RestoreStatus& status) {
    try {
        // Get backup metadata
        std::string metadataPath = restoreId + "/metadata.json";
        if (!std::filesystem::exists(metadataPath)) {
            status.state = RestoreState::Failed;
            status.error = "Backup not found";
            return false;
        }

        std::ifstream file(metadataPath);
        if (!file.is_open()) {
            status.state = RestoreState::Failed;
            status.error = "Failed to open metadata file";
            return false;
        }

        nlohmann::json j;
        file >> j;

        status.state = RestoreState::Completed;
        status.progress = 100.0;
        status.status = "Completed";
        return true;
    } catch (const std::exception& e) {
        status.state = RestoreState::Failed;
        status.error = e.what();
        return false;
    }
}

bool VMwareBackupProvider::verifyBackup(const std::string& backupId) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        std::filesystem::path backupDir = std::filesystem::current_path() / "backups" / backupId;
        if (!std::filesystem::exists(backupDir)) {
            lastError_ = "Backup not found: " + backupId;
            return false;
        }

        // Check metadata file
        auto metadataPath = backupDir / "metadata.json";
        if (!std::filesystem::exists(metadataPath)) {
            lastError_ = "Invalid backup: missing metadata";
            return false;
        }

        // Verify each disk in the backup
        for (const auto& entry : std::filesystem::directory_iterator(backupDir)) {
            if (entry.is_regular_file() && entry.path().extension() == ".vmdk") {
                if (!verifyDisk(entry.path().string())) {
                    lastError_ = "Failed to verify disk: " + entry.path().string();
                    return false;
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Verify backup failed: ") + e.what();
        return false;
    }
}

bool VMwareBackupProvider::verifyBackupIntegrity(const std::string& backupId) {
    try {
        if (!std::filesystem::exists(backupId)) {
            lastError_ = std::string("Backup not found: ") + backupId;
            return false;
        }

        // Verify each disk file
        for (const auto& entry : std::filesystem::directory_iterator(backupId)) {
            if (entry.path().extension() == ".vmdk") {
                if (!std::filesystem::exists(entry.path())) {
                    lastError_ = std::string("Disk file not found: ") + entry.path().string();
                    return false;
                }
            }
        }

        // Verify checksum
        std::string currentChecksum = calculateChecksum(backupId);
        auto metadata = getLatestBackupInfo(backupId);
        if (!metadata || currentChecksum != metadata->checksum) {
            lastError_ = "Checksum mismatch";
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Failed to verify backup integrity: ") + e.what();
        return false;
    }
}

bool VMwareBackupProvider::saveBackupMetadata(const std::string& backupId, const std::string& vmId,
                                            const std::vector<std::string>& diskPaths) {
    try {
        std::string metadataPath = backupId + "/metadata.json";
        std::ofstream file(metadataPath);
        if (!file.is_open()) {
            lastError_ = "Failed to open metadata file";
            return false;
        }

        nlohmann::json j;
        j["backupId"] = backupId;
        j["vmId"] = vmId;
        j["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        j["type"] = static_cast<int>(BackupType::FULL);
        j["size"] = 0;
        j["disks"] = diskPaths;
        j["checksum"] = calculateChecksum(backupId);

        file << j.dump(4);
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Failed to save metadata: ") + e.what();
        return false;
    }
}

std::optional<BackupMetadata> VMwareBackupProvider::getLatestBackupInfo(const std::string& vmId) {
    try {
        std::string backupPath = vmId;
        if (!std::filesystem::exists(backupPath)) {
            lastError_ = std::string("Backup not found for VM: ") + vmId;
            return std::nullopt;
        }

        std::string metadataPath = backupPath + "/metadata.json";
        std::ifstream file(metadataPath);
        if (!file.is_open()) {
            lastError_ = std::string("Failed to open metadata file: ") + metadataPath;
            return std::nullopt;
        }

        nlohmann::json j;
        file >> j;

        BackupMetadata info;
        info.backupId = j["backupId"];
        info.vmId = j["vmId"];
        info.timestamp = j["timestamp"];
        info.type = static_cast<BackupType>(j["type"].get<int>());
        info.size = j["size"];
        info.disks = j["disks"].get<std::vector<std::string>>();
        info.checksum = j["checksum"];

        return info;
    } catch (const std::exception& e) {
        lastError_ = std::string("Failed to get backup info: ") + e.what();
        return std::nullopt;
    }
}

std::string VMwareBackupProvider::calculateChecksum(const std::string& filePath) {
    const size_t bufferSize = 1 << 12; // 4KB
    std::vector<unsigned char> buffer(bufferSize);
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLen = 0;
    std::ifstream file(filePath, std::ios::binary);
    if (!file.is_open()) {
        return "";
    }

    EVP_MD_CTX* mdctx = EVP_MD_CTX_new();
    if (!mdctx) {
        file.close();
        return "";
    }
    if (EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr) != 1) {
        EVP_MD_CTX_free(mdctx);
        file.close();
        return "";
    }

    while (file.good()) {
        file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        std::streamsize count = file.gcount();
        if (count > 0) {
            if (EVP_DigestUpdate(mdctx, buffer.data(), count) != 1) {
                EVP_MD_CTX_free(mdctx);
                file.close();
                return "";
            }
        }
    }

    if (EVP_DigestFinal_ex(mdctx, hash, &hashLen) != 1) {
        EVP_MD_CTX_free(mdctx);
        file.close();
        return "";
    }
    EVP_MD_CTX_free(mdctx);
    file.close();

    std::ostringstream oss;
    for (unsigned int i = 0; i < hashLen; ++i) {
        oss << std::hex << std::setw(2) << std::setfill('0') << (int)hash[i];
    }
    return oss.str();
}

bool VMwareBackupProvider::backupDisk(const std::string& vmId, const std::string& diskPath, const BackupConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    try {
        // Create backup directory if it doesn't exist
        std::filesystem::create_directories(config.backupPath);

        // Get VDDK connection
        VDDKConnection vddkConn = connection_->getVDDKConnection();
        if (!vddkConn) {
            lastError_ = "Failed to get VDDK connection";
            Logger::error(lastError_);
            return false;
        }

        // Validate disk path format
        if (diskPath.empty() || diskPath[0] != '[' || diskPath.find(']') == std::string::npos) {
            lastError_ = "Invalid disk path format. Expected format: [datastore] path/to/vmdk";
            Logger::error(lastError_);
            return false;
        }

        Logger::debug("Using disk path: " + diskPath);

        // Open source disk
        VDDKHandle sourceHandle;
        int32_t result = VixDiskLib_OpenWrapper(vddkConn,
                                              diskPath.c_str(),
                                              VIXDISKLIB_FLAG_OPEN_READ_ONLY,
                                              &sourceHandle);
        if (result != VIX_OK) {
            lastError_ = "Failed to open source disk: " + vixErrorToString(result);
            Logger::error(lastError_);
            return false;
        }

        // Create backup file path
        std::string backupDiskPath = config.backupPath + "/" + std::filesystem::path(diskPath).filename().string();
        Logger::debug("Creating backup disk at: " + backupDiskPath);

        // Create target disk
        VixDiskLibCreateParams createParams;
        memset(&createParams, 0, sizeof(createParams));
        createParams.diskType = static_cast<VixDiskLibDiskType>(VIXDISKLIB_DISK_MONOLITHIC_SPARSE);
        createParams.adapterType = static_cast<VixDiskLibAdapterType>(VIXDISKLIB_ADAPTER_SCSI_LSILOGIC);
        createParams.hwVersion = VIXDISKLIB_HWVERSION_WORKSTATION_5;

        result = VixDiskLib_CreateWrapper(vddkConn,
                                        backupDiskPath.c_str(),
                                        &createParams,
                                        nullptr,
                                        nullptr);
        if (result != VIX_OK) {
            VixDiskLib_CloseWrapper(&sourceHandle);
            lastError_ = "Failed to create backup disk: " + vixErrorToString(result);
            Logger::error(lastError_);
            return false;
        }

        // Open backup disk
        VDDKHandle backupHandle;
        result = VixDiskLib_OpenWrapper(vddkConn,
                                      backupDiskPath.c_str(),
                                      VIXDISKLIB_FLAG_OPEN_UNBUFFERED,
                                      &backupHandle);
        if (result != VIX_OK) {
            VixDiskLib_CloseWrapper(&sourceHandle);
            lastError_ = "Failed to open backup disk: " + vixErrorToString(result);
            Logger::error(lastError_);
            return false;
        }

        // Get disk info
        VDDKInfo* diskInfo = nullptr;
        result = VixDiskLib_GetInfoWrapper(sourceHandle, &diskInfo);
        if (result != VIX_OK) {
            VixDiskLib_CloseWrapper(&sourceHandle);
            VixDiskLib_CloseWrapper(&backupHandle);
            lastError_ = "Failed to get disk info: " + vixErrorToString(result);
            Logger::error(lastError_);
            return false;
        }

        // Copy disk contents
        Logger::info("Starting disk copy operation...");
        result = VixDiskLib_CloneWrapper(vddkConn, backupDiskPath.c_str(), vddkConn, diskPath.c_str(), &createParams, nullptr, nullptr, FALSE);
        if (result != VIX_OK) {
            VixDiskLib_FreeInfoWrapper(diskInfo);
            VixDiskLib_CloseWrapper(&sourceHandle);
            VixDiskLib_CloseWrapper(&backupHandle);
            lastError_ = "Failed to copy disk contents: " + vixErrorToString(result);
            Logger::error(lastError_);
            return false;
        }

        // Cleanup
        VixDiskLib_FreeInfoWrapper(diskInfo);
        VixDiskLib_CloseWrapper(&sourceHandle);
        VixDiskLib_CloseWrapper(&backupHandle);

        Logger::info("Successfully backed up disk: " + diskPath);
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Backup failed: ") + e.what();
        Logger::error(lastError_);
        return false;
    }
}

bool VMwareBackupProvider::restoreDisk(const std::string& vmId, const std::string& diskPath, const RestoreConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    try {
        // Open backup disk
        VDDKHandle backupHandle;
        int32_t result = VixDiskLib_OpenWrapper(connection_->getVDDKConnection(),
                                              config.backupId.c_str(),
                                              VIXDISKLIB_FLAG_OPEN_READ_ONLY,
                                              &backupHandle);
        if (result != VIX_OK) {
            lastError_ = "Failed to open backup disk";
            return false;
        }

        // Open target disk
        VDDKHandle targetHandle;
        result = VixDiskLib_OpenWrapper(connection_->getVDDKConnection(),
                                      diskPath.c_str(),
                                      VIXDISKLIB_FLAG_OPEN_UNBUFFERED,
                                      &targetHandle);
        if (result != VIX_OK) {
            VixDiskLib_CloseWrapper(&backupHandle);
            lastError_ = "Failed to open target disk";
            return false;
        }

        // Get disk info
        VDDKInfo* diskInfo = nullptr;
        result = VixDiskLib_GetInfoWrapper(backupHandle, &diskInfo);
        if (result != VIX_OK) {
            VixDiskLib_CloseWrapper(&backupHandle);
            VixDiskLib_CloseWrapper(&targetHandle);
            lastError_ = "Failed to get disk info";
            return false;
        }

        // Copy disk data
        const size_t bufferSize = 1024 * 1024;  // 1MB buffer
        std::vector<uint8_t> buffer(bufferSize);
        uint64_t totalSectors = diskInfo->capacity;
        uint64_t sectorsProcessed = 0;

        while (sectorsProcessed < totalSectors) {
            uint64_t sectorsToRead = std::min(static_cast<uint64_t>(bufferSize / VIXDISKLIB_SECTOR_SIZE),
                                            totalSectors - sectorsProcessed);

            result = VixDiskLib_ReadWrapper(backupHandle,
                                          sectorsProcessed,
                                          sectorsToRead,
                                          buffer.data());
            if (result != VIX_OK) {
                VixDiskLib_FreeInfoWrapper(diskInfo);
                VixDiskLib_CloseWrapper(&backupHandle);
                VixDiskLib_CloseWrapper(&targetHandle);
                lastError_ = "Failed to read backup disk";
                return false;
            }

            result = VixDiskLib_WriteWrapper(targetHandle,
                                           sectorsProcessed,
                                           sectorsToRead,
                                           buffer.data());
            if (result != VIX_OK) {
                VixDiskLib_FreeInfoWrapper(diskInfo);
                VixDiskLib_CloseWrapper(&backupHandle);
                VixDiskLib_CloseWrapper(&targetHandle);
                lastError_ = "Failed to write target disk";
                return false;
            }

            sectorsProcessed += sectorsToRead;
            progress_ = static_cast<double>(sectorsProcessed) / totalSectors * 100.0;
        }

        // Cleanup
        VixDiskLib_FreeInfoWrapper(diskInfo);
        VixDiskLib_CloseWrapper(&backupHandle);
        VixDiskLib_CloseWrapper(&targetHandle);

        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Restore failed: ") + e.what();
        return false;
    }
}

bool VMwareBackupProvider::verifyDisk(const std::string& diskPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    try {
        // Open disk
        VDDKHandle diskHandle;
        int32_t result = VixDiskLib_OpenWrapper(connection_->getVDDKConnection(),
                                              diskPath.c_str(),
                                              VIXDISKLIB_FLAG_OPEN_READ_ONLY,
                                              &diskHandle);
        if (result != VIX_OK) {
            lastError_ = "Failed to open disk";
            return false;
        }

        // Get disk info
        VDDKInfo* diskInfo = nullptr;
        result = VixDiskLib_GetInfoWrapper(diskHandle, &diskInfo);
        if (result != VIX_OK) {
            VixDiskLib_CloseWrapper(&diskHandle);
            lastError_ = "Failed to get disk info";
            return false;
        }

        // Verify disk size
        if (diskInfo->capacity == 0) {
            VixDiskLib_FreeInfoWrapper(diskInfo);
            VixDiskLib_CloseWrapper(&diskHandle);
            lastError_ = "Invalid disk size";
            return false;
        }

        // Cleanup
        VixDiskLib_FreeInfoWrapper(diskInfo);
        VixDiskLib_CloseWrapper(&diskHandle);

        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Verify failed: ") + e.what();
        return false;
    }
}

bool VMwareBackupProvider::getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                                          std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    try {
        // Open disk
        VDDKHandle diskHandle;
        int32_t result = VixDiskLib_OpenWrapper(connection_->getVDDKConnection(),
                                              diskPath.c_str(),
                                              VIXDISKLIB_FLAG_OPEN_READ_ONLY,
                                              &diskHandle);
        if (result != VIX_OK) {
            lastError_ = "Failed to open disk";
            return false;
        }

        // Get disk info
        VDDKInfo* diskInfo = nullptr;
        result = VixDiskLib_GetInfoWrapper(diskHandle, &diskInfo);
        if (result != VIX_OK) {
            VixDiskLib_CloseWrapper(&diskHandle);
            lastError_ = "Failed to get disk info";
            return false;
        }

        // Query allocated blocks
        VDDKBlockList* blockList = nullptr;
        result = VixDiskLib_QueryAllocatedBlocksWrapper(diskHandle,
                                                      0,
                                                      diskInfo->capacity,
                                                      &blockList);
        if (result != VIX_OK) {
            VixDiskLib_FreeInfoWrapper(diskInfo);
            VixDiskLib_CloseWrapper(&diskHandle);
            lastError_ = "Failed to query allocated blocks";
            return false;
        }

        // Convert block list to changed blocks
        changedBlocks.clear();
        for (uint32_t i = 0; i < blockList->numBlocks; ++i) {
            changedBlocks.emplace_back(blockList->blocks[i].offset,
                                     blockList->blocks[i].length);
        }

        // Cleanup
        VixDiskLib_FreeBlockListWrapper(blockList);
        VixDiskLib_FreeInfoWrapper(diskInfo);
        VixDiskLib_CloseWrapper(&diskHandle);

        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Get changed blocks failed: ") + e.what();
        return false;
    }
}

bool VMwareBackupProvider::listBackups(std::vector<std::string>& backupIds) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        // Clear the output vector
        backupIds.clear();

        // Get the backup directory
        std::filesystem::path backupDir = std::filesystem::current_path() / "backups";

        // Check if the backup directory exists
        if (!std::filesystem::exists(backupDir)) {
            lastError_ = "Backup directory does not exist: " + backupDir.string();
            return false;
        }

        // Iterate through the backup directory
        for (const auto& entry : std::filesystem::directory_iterator(backupDir)) {
            if (entry.is_directory()) {
                // Check if it's a valid backup directory by looking for metadata
                auto metadataPath = entry.path() / "metadata.json";
                if (std::filesystem::exists(metadataPath)) {
                    backupIds.push_back(entry.path().filename().string());
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("List backups failed: ") + e.what();
        return false;
    }
}

bool VMwareBackupProvider::deleteBackup(const std::string& backupId) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        std::filesystem::path backupDir = std::filesystem::current_path() / "backups" / backupId;
        if (!std::filesystem::exists(backupDir)) {
            lastError_ = "Backup not found: " + backupId;
            return false;
        }

        std::filesystem::remove_all(backupDir);
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Delete backup failed: ") + e.what();
        return false;
    }
}

bool VMwareBackupProvider::validateDiskPath(const std::string& diskPath) const {
    if (diskPath.empty()) {
        return false;
    }
    // Check if the path exists and is accessible
    return std::filesystem::exists(diskPath);
}

bool VMwareBackupProvider::validateBackupPath(const std::string& backupPath) const {
    if (backupPath.empty()) {
        return false;
    }
    // Check if the backup directory exists or can be created
    std::filesystem::path path(backupPath);
    if (!std::filesystem::exists(path)) {
        try {
            return std::filesystem::create_directories(path);
        } catch (const std::exception&) {
            return false;
        }
    }
    return true;
}

bool VMwareBackupProvider::validateRestorePath(const std::string& restorePath) const {
    if (restorePath.empty()) {
        return false;
    }
    // Check if the restore path exists and is writable
    return std::filesystem::exists(restorePath) && 
           (std::filesystem::status(restorePath).permissions() & std::filesystem::perms::owner_write) != std::filesystem::perms::none;
}

bool VMwareBackupProvider::initializeCBT(const std::string& diskPath) {
    try {
        // For older VDDK versions, CBT is managed at the VM level through vSphere API
        // We'll use the REST client to enable CBT
        if (!connection_ || !connection_->isConnected()) {
            lastError_ = "Not connected to vCenter";
            return false;
        }

        auto* restClient = connection_->getRestClient();
        if (!restClient) {
            lastError_ = "Failed to get REST client";
            return false;
        }

        // Enable CBT through vSphere API
        return restClient->enableCBT(diskPath);
    } catch (const std::exception& e) {
        lastError_ = std::string("Failed to initialize CBT: ") + e.what();
        return false;
    }
}

bool VMwareBackupProvider::cleanupCBT(const std::string& diskPath) {
    try {
        // For older VDDK versions, CBT is managed at the VM level through vSphere API
        // We'll use the REST client to disable CBT
        if (!connection_ || !connection_->isConnected()) {
            lastError_ = "Not connected to vCenter";
            return false;
        }

        auto* restClient = connection_->getRestClient();
        if (!restClient) {
            lastError_ = "Failed to get REST client";
            return false;
        }

        // Disable CBT through vSphere API
        return restClient->disableCBT(diskPath);
    } catch (const std::exception& e) {
        lastError_ = std::string("Failed to cleanup CBT: ") + e.what();
        return false;
    }
}

void VMwareBackupProvider::updateProgress(double progress, const std::string& status) {
    progress_ = progress;
    if (progressCallback_) {
        progressCallback_(static_cast<int>(progress));
    }
    if (statusCallback_) {
        statusCallback_(status);
    }
}

/*
bool VMwareBackupProvider::initializeVDDK() {
    try {
        updateProgress(0.0, "Initializing VDDK");

        // Initialize VDDK
        VixError vixError = VixDiskLib_InitWrapper(VIXDISKLIB_VERSION_MAJOR,
                                             VIXDISKLIB_VERSION_MINOR,
                                             nullptr);
        if (VIX_FAILED(vixError)) {
            lastError_ = "Failed to initialize VDDK";
            return false;
        }

        updateProgress(100.0, "Success");
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Initialize VDDK failed: ") + e.what();
        return false;
    }
}
*/

double VMwareBackupProvider::getProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_;
}

std::string VMwareBackupProvider::getLastError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return lastError_;
}

void VMwareBackupProvider::clearLastError() {
    std::lock_guard<std::mutex> lock(mutex_);
    lastError_.clear();
}

void VMwareBackupProvider::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = std::move(callback);
}

void VMwareBackupProvider::setStatusCallback(StatusCallback callback) {
    statusCallback_ = std::move(callback);
}

bool VMwareBackupProvider::enableCBT(const std::string& vmId) {
    lastError_ = "CBT operations not supported";
    return false;
}

bool VMwareBackupProvider::disableCBT(const std::string& vmId) {
    lastError_ = "CBT operations not supported";
    return false;
}

bool VMwareBackupProvider::isCBTEnabled(const std::string& vmId) const {
    const_cast<VMwareBackupProvider*>(this)->lastError_ = "CBT operations not supported";
    return false;
}

bool VMwareBackupProvider::verifyRestore(const std::string& vmId, const std::string& backupId) {
    try {
        updateProgress(0.0, "Verifying restore");

        // Get backup metadata
        auto metadata = getLatestBackupInfo(vmId);
        if (!metadata) {
            lastError_ = "Failed to get backup metadata";
            return false;
        }

        // Get VM disk paths
        std::vector<std::string> diskPaths;
        if (!connection_->getVMDiskPaths(vmId, diskPaths)) {
            lastError_ = "Failed to get VM disk paths";
            return false;
        }

        // Verify each disk
        for (const auto& diskPath : diskPaths) {
            updateProgress(0.0, "Verifying disk: " + diskPath);

            // Open backup disk
            std::string backupDiskPath = backupId + "/" + std::filesystem::path(diskPath).filename().string();
            VDDKDiskHandle srcHandle(backupDiskPath, VIXDISKLIB_FLAG_OPEN_READ_ONLY);
            VDDKDiskInfo srcInfo(srcHandle.get());

            // Open target disk
            VDDKDiskHandle dstHandle(diskPath, VIXDISKLIB_FLAG_OPEN_READ_ONLY);
            VDDKDiskInfo dstInfo(dstHandle.get());

            // Compare disk sizes
            if (srcInfo.get()->capacity != dstInfo.get()->capacity) {
                lastError_ = "Disk size mismatch";
                return false;
            }

            // Compare disk contents
            const size_t bufferSize = 1024 * 1024;  // 1MB buffer
            std::vector<uint8_t> srcBuffer(bufferSize);
            std::vector<uint8_t> dstBuffer(bufferSize);
            uint64_t totalSectors = srcInfo.get()->capacity;
            uint64_t sectorsProcessed = 0;

            while (sectorsProcessed < totalSectors) {
                uint64_t sectorsToRead = std::min(static_cast<uint64_t>(bufferSize / VIXDISKLIB_SECTOR_SIZE),
                                                totalSectors - sectorsProcessed);

                VixError vixError = VixDiskLib_ReadWrapper(srcHandle.get(),
                                                          sectorsProcessed,
                                                          sectorsToRead,
                                                          srcBuffer.data());
                if (VIX_FAILED(vixError)) {
                    lastError_ = "Failed to read source disk";
                    return false;
                }

                vixError = VixDiskLib_ReadWrapper(dstHandle.get(),
                                         sectorsProcessed,
                                         sectorsToRead,
                                         dstBuffer.data());
                if (VIX_FAILED(vixError)) {
                    lastError_ = "Failed to read target disk";
                    return false;
                }

                if (memcmp(srcBuffer.data(), dstBuffer.data(), sectorsToRead * VIXDISKLIB_SECTOR_SIZE) != 0) {
                    lastError_ = "Disk content mismatch";
                    return false;
                }

                sectorsProcessed += sectorsToRead;
                double progress = static_cast<double>(sectorsProcessed) / totalSectors * 100.0;
                updateProgress(progress, "Verifying disk: " + diskPath);
            }
        }

        updateProgress(100.0, "Success");
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Verify restore failed: ") + e.what();
        return false;
    }
} 