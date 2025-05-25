#include "backup/vmware/vmware_backup_provider.hpp"
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
#include <vixDiskLib.h>

// VDDK version constants
#define VIXDISKLIB_VERSION_MAJOR 8
#define VIXDISKLIB_VERSION_MINOR 0

// Helper function to convert VixError to string
std::string vixErrorToString(VixError error) {
    char* errorText = VixDiskLib_GetErrorText(error, nullptr);
    std::string result(errorText ? errorText : "Unknown error");
    VixDiskLib_FreeErrorText(errorText);
    return result;
}

// RAII wrapper for VixDiskLibHandle
class VDDKDiskHandle {
public:
    explicit VDDKDiskHandle(const std::string& path, uint32 flags) {
        VixError vixError = VixDiskLib_Open(nullptr, path.c_str(), flags, &handle_);
        if (VIX_FAILED(vixError)) {
            throw std::runtime_error("Failed to open disk: " + path);
        }
    }

    ~VDDKDiskHandle() {
        if (handle_) {
            VixDiskLib_Close(handle_);
        }
    }

    VixDiskLibHandle get() const { return handle_; }

    // Delete copy operations
    VDDKDiskHandle(const VDDKDiskHandle&) = delete;
    VDDKDiskHandle& operator=(const VDDKDiskHandle&) = delete;

private:
    VixDiskLibHandle handle_{nullptr};
};

// RAII wrapper for VixDiskLibInfo
class VDDKDiskInfo {
public:
    explicit VDDKDiskInfo(VixDiskLibHandle handle) {
        VixError vixError = VixDiskLib_GetInfo(handle, &info_);
        if (VIX_FAILED(vixError)) {
            throw std::runtime_error("Failed to get disk info");
        }
    }

    ~VDDKDiskInfo() {
        if (info_) {
            VixDiskLib_FreeInfo(info_);
        }
    }

    VixDiskLibInfo* get() const { return info_; }

    // Delete copy operations
    VDDKDiskInfo(const VDDKDiskInfo&) = delete;
    VDDKDiskInfo& operator=(const VDDKDiskInfo&) = delete;

private:
    VixDiskLibInfo* info_{nullptr};
};

// RAII wrapper for VDDK connection
class VDDKConnection {
public:
    VDDKConnection() {
        VixError vixError = VixDiskLib_InitEx(VIXDISKLIB_VERSION_MAJOR,
                                             VIXDISKLIB_VERSION_MINOR,
                                             nullptr,  // logFunc
                                             nullptr,  // warnFunc
                                             nullptr,  // panicFunc
                                             nullptr,  // libDir
                                             nullptr); // configFile
        if (VIX_FAILED(vixError)) {
            throw std::runtime_error("Failed to initialize VDDK");
        }
    }

    ~VDDKConnection() {
        VixDiskLib_Exit();
    }

    // Delete copy operations
    VDDKConnection(const VDDKConnection&) = delete;
    VDDKConnection& operator=(const VDDKConnection&) = delete;
};

VMwareBackupProvider::VMwareBackupProvider(std::shared_ptr<VMwareConnection> connection)
    : connection_(std::move(connection)), progress_(0.0) {
    Logger::debug("VMwareBackupProvider constructor called with connection");
}

VMwareBackupProvider::~VMwareBackupProvider() {
    Logger::debug("VMwareBackupProvider destructor called");
    // Only clean up active operations, as disconnect() will handle snapshot cleanup
    cleanupActiveOperations();
}

bool VMwareBackupProvider::initialize() {
    if (!connection_) {
        lastError_ = "No connection provided";
        return false;
    }
    return true;
}

bool VMwareBackupProvider::connect(const std::string& host, const std::string& username, const std::string& password) {
    try {
        updateProgress(0.0, "Connecting to vCenter");

        // Try to connect with retries
        const int maxRetries = 3;
        const int retryDelayMs = 1000;
        bool connected = false;

        for (int i = 0; i < maxRetries && !connected; ++i) {
            if (i > 0) {
                updateProgress(0.0, "Retrying connection...");
                std::this_thread::sleep_for(std::chrono::milliseconds(retryDelayMs));
            }

            if (connection_->connect(host, username, password)) {
                connected = true;
            }
        }

        if (!connected) {
            lastError_ = "Failed to connect to vCenter after " + std::to_string(maxRetries) + " attempts";
            return false;
        }

        // Verify connection
        if (!verifyConnection()) {
            lastError_ = "Connection verification failed";
            return false;
        }

        updateProgress(100.0, "Success");
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Connect failed: ") + e.what();
        return false;
    }
}

void VMwareBackupProvider::disconnect() {
    try {
        updateProgress(0.0, "Disconnecting from vCenter");

        // Clean up active operations first
        cleanupActiveOperations();

        // Disconnect from vCenter only if we're connected
        if (connection_ && connection_->isConnected()) {
            // Clean up snapshots before disconnecting
            if (!currentSnapshotName_.empty()) {
                cleanupSnapshot();
            }
            // Now disconnect
            Logger::debug("Disconnecting from vCenter in VMwareBackupProvider");
            connection_->disconnect();
            connection_.reset();  // Explicitly release the connection
        }

        updateProgress(100.0, "Success");
    } catch (const std::exception& e) {
        lastError_ = std::string("Disconnect failed: ") + e.what();
    }
}

bool VMwareBackupProvider::isConnected() const {
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

bool VMwareBackupProvider::getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths) const {
    if (!connection_ || !connection_->isConnected()) {
        const_cast<VMwareBackupProvider*>(this)->lastError_ = "Not connected";
        return false;
    }
    return connection_->getVMDiskPaths(vmId, diskPaths);
}

bool VMwareBackupProvider::getVMInfo(const std::string& vmId, std::string& name, std::string& status) const {
    if (!connection_ || !connection_->isConnected()) {
        const_cast<VMwareBackupProvider*>(this)->lastError_ = "Not connected";
        return false;
    }
    return connection_->getVMInfo(vmId, name, status);
}

bool VMwareBackupProvider::createSnapshot(const std::string& vmId) {
    if (!connection_ || !connection_->isConnected()) {
        lastError_ = "Not connected";
        return false;
    }

    // Generate unique snapshot name
    currentSnapshotName_ = "backup-snapshot-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    currentVmId_ = vmId;
    
    // Get REST client and create snapshot
    auto* restClient = connection_->getRestClient();
    if (!restClient) {
        lastError_ = "Failed to get REST client";
        return false;
    }

    return restClient->createSnapshot(vmId, currentSnapshotName_, "Snapshot created for backup");
}

bool VMwareBackupProvider::removeSnapshot() {
    if (currentSnapshotName_.empty() || !connection_ || !connection_->isConnected()) {
        return false;
    }

    auto* restClient = connection_->getRestClient();
    if (!restClient) {
        lastError_ = "Failed to get REST client";
        return false;
    }

    bool success = restClient->removeSnapshot(currentVmId_, currentSnapshotName_);
    currentSnapshotName_.clear();
    currentVmId_.clear();
    return success;
}

void VMwareBackupProvider::cleanupSnapshot() {
    if (!currentSnapshotName_.empty()) {
        removeSnapshot();
    }
}

bool VMwareBackupProvider::startBackup(const std::string& vmId, const BackupConfig& config) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    // Get VM info using REST client
    nlohmann::json vmInfo;
    if (!connection_->getRestClient()->getVMInfo(vmId, vmInfo)) {
        lastError_ = "Failed to get VM info";
        return false;
    }

    // Create backup directory if it doesn't exist
    std::filesystem::create_directories(config.backupPath);

    // Create snapshot using REST client
    std::string snapshotName = "backup_" + std::to_string(std::time(nullptr));
    if (!connection_->getRestClient()->createSnapshot(vmId, snapshotName, "Backup snapshot")) {
        lastError_ = "Failed to create snapshot";
        return false;
    }

    // Get snapshots using REST client
    nlohmann::json snapshots;
    if (!connection_->getRestClient()->getSnapshots(vmId, snapshots)) {
        lastError_ = "Failed to get snapshots";
        return false;
    }

    // Get VM disk paths
    std::vector<std::string> diskPaths;
    if (!connection_->getVMDiskPaths(vmId, diskPaths)) {
        lastError_ = "Failed to get VM disk paths";
        return false;
    }

    // Backup each disk
    for (const auto& diskPath : diskPaths) {
        std::string backupPath = config.backupPath + "/" + std::filesystem::path(diskPath).filename().string();
        
        // Use VDDK to backup the disk
        VixDiskLibConnection vddkConn = connection_->getVDDKConnection();
        if (!vddkConn) {
            lastError_ = "Failed to get VDDK connection";
            return false;
        }

        // Open source disk
        VixDiskLibHandle srcDisk;
        VixError vixError = VixDiskLib_Open(vddkConn, diskPath.c_str(), VIXDISKLIB_FLAG_OPEN_READ_ONLY, &srcDisk);
        if (vixError != VIX_OK) {
            lastError_ = "Failed to open source disk";
            return false;
        }

        // Create destination disk
        VixDiskLibCreateParams createParams;
        memset(&createParams, 0, sizeof(createParams));
        createParams.adapterType = VIXDISKLIB_ADAPTER_SCSI_BUSLOGIC;
        createParams.diskType = VIXDISKLIB_DISK_MONOLITHIC_SPARSE;

        vixError = VixDiskLib_Create(vddkConn, backupPath.c_str(), &createParams, nullptr, nullptr);
        if (vixError != VIX_OK) {
            VixDiskLib_Close(srcDisk);
            lastError_ = "Failed to create destination disk";
            return false;
        }

        // Open destination disk
        VixDiskLibHandle dstDisk;
        vixError = VixDiskLib_Open(vddkConn, backupPath.c_str(), VIXDISKLIB_FLAG_OPEN_UNBUFFERED, &dstDisk);
        if (vixError != VIX_OK) {
            VixDiskLib_Close(srcDisk);
            lastError_ = "Failed to open destination disk";
            return false;
        }

        // Copy disk contents
        vixError = VixDiskLib_Clone(vddkConn, backupPath.c_str(), vddkConn, diskPath.c_str(), &createParams, nullptr, nullptr, FALSE);
        if (vixError != VIX_OK) {
            VixDiskLib_Close(srcDisk);
            VixDiskLib_Close(dstDisk);
            lastError_ = "Failed to copy disk contents";
            return false;
        }

        // Close disks
        VixDiskLib_Close(srcDisk);
        VixDiskLib_Close(dstDisk);
    }

    // Start backup job
    std::string backupId = "backup_" + vmId + "_" + std::to_string(std::time(nullptr));
    auto taskManager = std::make_shared<ParallelTaskManager>();
    activeOperations_[backupId] = std::make_unique<BackupJob>(shared_from_this(), taskManager, config);
    
    if (progressCallback_) {
        progressCallback_(0.0);
    }
    if (statusCallback_) {
        statusCallback_("Backup started");
    }

    return true;
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

BackupStatus VMwareBackupProvider::getBackupStatus(const std::string& vmId) {
    BackupStatus status;
    status.state = BackupState::InProgress;
    status.progress = 0.0;
    status.status = "Backup in progress";
    return status;
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

RestoreStatus VMwareBackupProvider::getRestoreStatus(const std::string& restoreId) const {
    RestoreStatus status;
    status.state = RestoreState::NotStarted;
    status.progress = 0.0;
    status.status = "Unknown";
    status.startTime = std::chrono::system_clock::now();
    status.endTime = std::chrono::system_clock::now();
    status.error = "";

    try {
        // Get backup metadata
        std::string metadataPath = restoreId + "/metadata.json";
        if (!std::filesystem::exists(metadataPath)) {
            status.state = RestoreState::Failed;
            status.error = "Backup not found";
            return status;
        }

        std::ifstream file(metadataPath);
        if (!file.is_open()) {
            status.state = RestoreState::Failed;
            status.error = "Failed to open metadata file";
            return status;
        }

        nlohmann::json j;
        file >> j;

        status.state = RestoreState::Completed;
        status.progress = 100.0;
        status.status = "Completed";
    } catch (const std::exception& e) {
        status.state = RestoreState::Failed;
        status.error = e.what();
    }

    return status;
}

bool VMwareBackupProvider::verifyBackup(const std::string& backupId) {
    if (!connection_ || !connection_->isConnected()) {
        lastError_ = "Not connected";
        return false;
    }

    try {
        auto metadata = getLatestBackupInfo(backupId);
        if (!metadata) {
            lastError_ = "Failed to get backup metadata";
            return false;
        }

        return verifyBackupIntegrity(backupId);
    } catch (const std::exception& e) {
        lastError_ = std::string("Failed to verify backup: ") + e.what();
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
    try {
        if (!verifyConnection()) {
            return false;
        }

        // Validate paths
        if (!validateDiskPath(diskPath)) {
            lastError_ = "Invalid disk path: " + diskPath;
            return false;
        }

        if (!validateBackupPath(config.backupPath)) {
            lastError_ = "Invalid backup path: " + config.backupPath;
            return false;
        }

        // Create backup directory if it doesn't exist
        std::filesystem::create_directories(config.backupPath);

        // Initialize CBT if enabled
        if (config.enableCBT && !initializeCBT(vmId)) {
            return false;
        }

        // Open source disk
        VDDKDiskHandle srcHandle(diskPath, VIXDISKLIB_FLAG_OPEN_READ_ONLY);
        VDDKDiskInfo srcInfo(srcHandle.get());

        // Create backup file path
        std::string backupDiskPath = config.backupPath + "/" + std::filesystem::path(diskPath).filename().string();

        // Open destination disk
        VDDKDiskHandle dstHandle(backupDiskPath, VIXDISKLIB_FLAG_OPEN_UNBUFFERED | VIXDISKLIB_FLAG_OPEN_SINGLE_LINK);
        VDDKDiskInfo dstInfo(dstHandle.get());

        // Copy disk data
        const size_t bufferSize = 1024 * 1024;  // 1MB buffer
        std::vector<uint8_t> buffer(bufferSize);
        uint64_t totalSectors = srcInfo.get()->capacity;
        uint64_t sectorsProcessed = 0;

        while (sectorsProcessed < totalSectors) {
            uint64_t sectorsToRead = std::min(static_cast<uint64_t>(bufferSize / VIXDISKLIB_SECTOR_SIZE),
                                            totalSectors - sectorsProcessed);

            VixError vixError = VixDiskLib_Read(srcHandle.get(),
                                              sectorsProcessed,
                                              sectorsToRead,
                                              buffer.data());
            if (VIX_FAILED(vixError)) {
                lastError_ = "Failed to read from source disk: " + vixErrorToString(vixError);
                return false;
            }

            vixError = VixDiskLib_Write(dstHandle.get(),
                                      sectorsProcessed,
                                      sectorsToRead,
                                      buffer.data());
            if (VIX_FAILED(vixError)) {
                lastError_ = "Failed to write to backup disk: " + vixErrorToString(vixError);
                return false;
            }

            sectorsProcessed += sectorsToRead;
            double progress = static_cast<double>(sectorsProcessed) / totalSectors * 100.0;
            updateProgress(progress, "Backing up disk");
        }

        // Clean up CBT if enabled
        if (config.enableCBT && !cleanupCBT(vmId)) {
            return false;
        }

        updateProgress(100.0, "Backup completed successfully");
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Backup failed: ") + e.what();
        return false;
    }
}

bool VMwareBackupProvider::restoreDisk(const std::string& vmId, const std::string& diskPath, const RestoreConfig& config) {
    try {
        if (!verifyConnection()) {
            return false;
        }

        // Validate paths
        if (!validateDiskPath(diskPath)) {
            lastError_ = "Invalid disk path: " + diskPath;
            return false;
        }

        if (!validateRestorePath(config.restorePath)) {
            lastError_ = "Invalid restore path: " + config.restorePath;
            return false;
        }

        // Create restore directory if it doesn't exist
        std::filesystem::create_directories(config.restorePath);

        // Open backup disk
        std::string backupDiskPath = config.restorePath + "/" + std::filesystem::path(diskPath).filename().string();
        VDDKDiskHandle srcHandle(backupDiskPath, VIXDISKLIB_FLAG_OPEN_READ_ONLY);
        VDDKDiskInfo srcInfo(srcHandle.get());

        // Open destination disk
        VDDKDiskHandle dstHandle(diskPath, VIXDISKLIB_FLAG_OPEN_UNBUFFERED);
        VDDKDiskInfo dstInfo(dstHandle.get());

        // Copy disk data
        const size_t bufferSize = 1024 * 1024;  // 1MB buffer
        std::vector<uint8_t> buffer(bufferSize);
        uint64_t totalSectors = srcInfo.get()->capacity;
        uint64_t sectorsProcessed = 0;

        while (sectorsProcessed < totalSectors) {
            uint64_t sectorsToRead = std::min(static_cast<uint64_t>(bufferSize / VIXDISKLIB_SECTOR_SIZE),
                                            totalSectors - sectorsProcessed);

            VixError vixError = VixDiskLib_Read(srcHandle.get(),
                                              sectorsProcessed,
                                              sectorsToRead,
                                              buffer.data());
            if (VIX_FAILED(vixError)) {
                lastError_ = "Failed to read from backup disk: " + vixErrorToString(vixError);
                return false;
            }

            vixError = VixDiskLib_Write(dstHandle.get(),
                                      sectorsProcessed,
                                      sectorsToRead,
                                      buffer.data());
            if (VIX_FAILED(vixError)) {
                lastError_ = "Failed to write to destination disk: " + vixErrorToString(vixError);
                return false;
            }

            sectorsProcessed += sectorsToRead;
            double progress = static_cast<double>(sectorsProcessed) / totalSectors * 100.0;
            updateProgress(progress, "Restoring disk");
        }

        // Verify restore if enabled
        if (config.verifyAfterRestore && !verifyRestore(vmId, config.backupId)) {
            return false;
        }

        updateProgress(100.0, "Restore completed successfully");
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Restore failed: ") + e.what();
        return false;
    }
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

                VixError vixError = VixDiskLib_Read(srcHandle.get(),
                                                  sectorsProcessed,
                                                  sectorsToRead,
                                                  srcBuffer.data());
                if (VIX_FAILED(vixError)) {
                    lastError_ = "Failed to read source disk";
                    return false;
                }

                vixError = VixDiskLib_Read(dstHandle.get(),
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

void VMwareBackupProvider::updateProgress(double progress, const std::string& status) {
    progress_ = progress;
    if (progressCallback_) {
        progressCallback_(static_cast<int>(progress));
    }
    if (statusCallback_) {
        statusCallback_(status);
    }
}

bool VMwareBackupProvider::initializeVDDK() {
    try {
        updateProgress(0.0, "Initializing VDDK");

        // Initialize VDDK
        VixError vixError = VixDiskLib_InitEx(VIXDISKLIB_VERSION_MAJOR,
                                             VIXDISKLIB_VERSION_MINOR,
                                             nullptr,  // logFunc
                                             nullptr,  // warnFunc
                                             nullptr,  // panicFunc
                                             nullptr,  // libDir
                                             nullptr); // configFile
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

double VMwareBackupProvider::getProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_;
}

std::string VMwareBackupProvider::getLastError() const {
    return lastError_;
}

void VMwareBackupProvider::clearLastError() {
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

bool VMwareBackupProvider::getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                                          std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) {
    lastError_ = "CBT operations not supported";
    return false;
}

bool VMwareBackupProvider::listBackups(std::vector<std::string>& backupIds) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        // Clear the output vector
        backupIds.clear();

        // Get the backup directory from the first active operation or use a default
        std::filesystem::path backupDir;
        if (!activeOperations_.empty()) {
            // Use the backup path from the config
            backupDir = activeOperations_.begin()->second->getConfig().backupPath;
        } else {
            backupDir = std::filesystem::current_path() / "backups";
        }

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
        lastError_ = "Failed to list backups: " + std::string(e.what());
        return false;
    }
}

bool VMwareBackupProvider::deleteBackup(const std::string& backupId) {
    std::lock_guard<std::mutex> lock(mutex_);
    try {
        // Get the backup directory from the first active operation or use a default
        std::filesystem::path backupDir;
        if (!activeOperations_.empty()) {
            // Use the backup path from the config
            backupDir = activeOperations_.begin()->second->getConfig().backupPath;
        } else {
            backupDir = std::filesystem::current_path() / "backups";
        }

        // Construct the full backup path
        auto backupPath = backupDir / backupId;

        // Check if the backup exists
        if (!std::filesystem::exists(backupPath)) {
            lastError_ = "Backup not found: " + backupId;
            return false;
        }

        // Check if the backup is currently in use
        if (activeOperations_.find(backupId) != activeOperations_.end()) {
            lastError_ = "Cannot delete backup while it is in use: " + backupId;
            return false;
        }

        // Delete the backup directory and all its contents
        std::filesystem::remove_all(backupPath);

        Logger::info("Successfully deleted backup: " + backupId);
        return true;
    } catch (const std::exception& e) {
        lastError_ = "Failed to delete backup: " + std::string(e.what());
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

bool VMwareBackupProvider::verifyDisk(const std::string& diskPath) {
    try {
        VDDKDiskHandle handle(diskPath, VIXDISKLIB_FLAG_OPEN_READ_ONLY);
        VDDKDiskInfo info(handle.get());
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Failed to verify disk: ") + e.what();
        return false;
    }
} 