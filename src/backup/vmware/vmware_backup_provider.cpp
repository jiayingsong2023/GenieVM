#include "backup/vmware/vmware_backup_provider.hpp"
#include "common/vmware_connection.hpp"
#include "common/backup_status.hpp"
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
}

VMwareBackupProvider::~VMwareBackupProvider() {
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

        // Clean up active operations
        cleanupActiveOperations();

        // Disconnect from vCenter
        if (connection_) {
            connection_->disconnect();
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

bool VMwareBackupProvider::startBackup(const std::string& vmId, const BackupConfig& config) {
    if (!connection_ || !connection_->isConnected()) {
        lastError_ = "Not connected";
        return false;
    }

    try {
        updateProgress(0.0, "Starting backup");

        // Generate unique backup ID if not provided
        std::string backupId = config.backupDir.empty() ? 
            std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) : 
            config.backupDir;

        // Create backup directory
        std::filesystem::create_directories(backupId);

        // Get VM info
        std::vector<std::string> diskPaths;
        if (!connection_->getVMDiskPaths(vmId, diskPaths)) {
            lastError_ = "Failed to get VM disk paths";
            return false;
        }

        // Backup each disk
        BackupMetadata metadata;
        metadata.backupId = backupId;
        metadata.vmId = vmId;
        metadata.timestamp = std::chrono::system_clock::now().time_since_epoch().count();
        metadata.type = BackupType::FULL;
        metadata.size = 0;

        for (const auto& diskPath : diskPaths) {
            updateProgress(0.0, "Backing up disk: " + diskPath);

            if (!backupDisk(vmId, diskPath, backupId)) {
                lastError_ = "Failed to backup disk: " + diskPath;
                return false;
            }

            metadata.disks.push_back(std::filesystem::path(diskPath).filename().string());
            metadata.size += std::filesystem::file_size(backupId + "/" + metadata.disks.back());
        }

        // Calculate checksum
        metadata.checksum = calculateChecksum(backupId);

        // Save metadata
        if (!saveBackupMetadata(backupId, vmId, metadata.disks)) {
            lastError_ = "Failed to save backup metadata";
            return false;
        }

        updateProgress(100.0, "Success");
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Start backup failed: ") + e.what();
        return false;
    }
}

bool VMwareBackupProvider::cancelBackup(const std::string& backupId) {
    if (!connection_ || !connection_->isConnected()) {
        lastError_ = "Not connected";
        return false;
    }

    try {
        updateProgress(0.0, "Cancelling backup");

        // Remove backup directory
        std::filesystem::remove_all(backupId);

        updateProgress(100.0, "Success");
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Cancel backup failed: ") + e.what();
        return false;
    }
}

bool VMwareBackupProvider::pauseBackup(const std::string& backupId) {
    lastError_ = "Pause operation not supported";
    return false;
}

bool VMwareBackupProvider::resumeBackup(const std::string& backupId) {
    lastError_ = "Resume operation not supported";
    return false;
}

BackupStatus VMwareBackupProvider::getBackupStatus(const std::string& backupId) const {
    BackupStatus status;
    status.state = BackupState::NotStarted;
    status.progress = 0.0;
    status.status = "Unknown";
    status.startTime = std::chrono::system_clock::now();
    status.endTime = std::chrono::system_clock::now();
    status.error = "";

    try {
        if (!std::filesystem::exists(backupId)) {
            status.state = BackupState::Failed;
            status.error = "Backup not found";
            return status;
        }

        // Get backup metadata
        std::string metadataPath = backupId + "/metadata.json";
        if (!std::filesystem::exists(metadataPath)) {
            status.state = BackupState::Failed;
            status.error = "Metadata not found";
            return status;
        }

        std::ifstream file(metadataPath);
        if (!file.is_open()) {
            status.state = BackupState::Failed;
            status.error = "Failed to open metadata file";
            return status;
        }

        nlohmann::json j;
        file >> j;

        status.state = BackupState::Completed;
        status.progress = 100.0;
        status.status = "Completed";
    } catch (const std::exception& e) {
        status.state = BackupState::Failed;
        status.error = e.what();
    }

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

        // Restore each disk
        for (const auto& diskPath : diskPaths) {
            updateProgress(0.0, "Restoring disk: " + diskPath);

            if (!restoreDisk(vmId, diskPath, backupId)) {
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
    try {
        std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open()) {
            throw std::runtime_error("Failed to open file: " + filePath);
        }

        SHA256_CTX sha256;
        SHA256_Init(&sha256);

        std::vector<char> buffer(4096);
        while (file) {
            file.read(buffer.data(), buffer.size());
            std::streamsize count = file.gcount();
            if (count > 0) {
                SHA256_Update(&sha256, buffer.data(), count);
            }
        }

        unsigned char hash[SHA256_DIGEST_LENGTH];
        SHA256_Final(hash, &sha256);

        std::stringstream ss;
        for (int i = 0; i < SHA256_DIGEST_LENGTH; i++) {
            ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
        }
        return ss.str();
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string("Failed to calculate checksum: ") + e.what());
    }
}

bool VMwareBackupProvider::backupDisk(const std::string& vmId, const std::string& diskPath, const std::string& backupPath) {
    try {
        updateProgress(0.0, "Backing up disk: " + diskPath);

        // Initialize VDDK if needed
        if (!initializeVDDK()) {
            return false;
        }

        // Open source disk
        VDDKDiskHandle srcHandle(diskPath, VIXDISKLIB_FLAG_OPEN_READ_ONLY);
        VDDKDiskInfo srcInfo(srcHandle.get());

        // Create backup directory
        std::filesystem::create_directories(backupPath);

        // Create target disk
        std::string targetPath = backupPath + "/" + std::filesystem::path(diskPath).filename().string();
        VDDKDiskHandle dstHandle(targetPath, VIXDISKLIB_FLAG_OPEN_UNBUFFERED);

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
                lastError_ = "Failed to read source disk";
                return false;
            }

            vixError = VixDiskLib_Write(dstHandle.get(),
                                      sectorsProcessed,
                                      sectorsToRead,
                                      buffer.data());
            if (VIX_FAILED(vixError)) {
                lastError_ = "Failed to write target disk";
                return false;
            }

            sectorsProcessed += sectorsToRead;
            double progress = static_cast<double>(sectorsProcessed) / totalSectors * 100.0;
            updateProgress(progress, "Backing up disk: " + diskPath);
        }

        updateProgress(100.0, "Success");
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Backup disk failed: ") + e.what();
        return false;
    }
}

bool VMwareBackupProvider::restoreDisk(const std::string& vmId, const std::string& diskPath, const std::string& backupPath) {
    try {
        updateProgress(0.0, "Restoring disk: " + diskPath);

        // Initialize VDDK if needed
        if (!initializeVDDK()) {
            return false;
        }

        // Open backup disk
        std::string backupDiskPath = backupPath + "/" + std::filesystem::path(diskPath).filename().string();
        VDDKDiskHandle srcHandle(backupDiskPath, VIXDISKLIB_FLAG_OPEN_READ_ONLY);
        VDDKDiskInfo srcInfo(srcHandle.get());

        // Open target disk
        VDDKDiskHandle dstHandle(diskPath, VIXDISKLIB_FLAG_OPEN_UNBUFFERED);

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
                lastError_ = "Failed to read backup disk";
                return false;
            }

            vixError = VixDiskLib_Write(dstHandle.get(),
                                      sectorsProcessed,
                                      sectorsToRead,
                                      buffer.data());
            if (VIX_FAILED(vixError)) {
                lastError_ = "Failed to write target disk";
                return false;
            }

            sectorsProcessed += sectorsToRead;
            double progress = static_cast<double>(sectorsProcessed) / totalSectors * 100.0;
            updateProgress(progress, "Restoring disk: " + diskPath);
        }

        updateProgress(100.0, "Success");
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Restore disk failed: ") + e.what();
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
                                          std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) const {
    const_cast<VMwareBackupProvider*>(this)->lastError_ = "CBT operations not supported";
    return false;
} 