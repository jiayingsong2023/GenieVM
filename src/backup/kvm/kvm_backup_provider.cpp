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

KVMBackupProvider::KVMBackupProvider()
    : connection_(nullptr)
    , cbtFactory_(std::make_unique<CBTFactory>()) {
}

KVMBackupProvider::~KVMBackupProvider() {
    disconnect();
}

bool KVMBackupProvider::initialize() {
    // Initialize libvirt
    if (virInitialize() < 0) {
        lastError_ = "Failed to initialize libvirt";
        return false;
    }
    return true;
}

bool KVMBackupProvider::connect(const std::string& host, const std::string& username, const std::string& password) {
    if (connection_) {
        disconnect();
    }

    std::string uri = "qemu+ssh://" + username + "@" + host + "/system";
    connection_ = virConnectOpenAuth(uri.c_str(), nullptr, 0);
    if (!connection_) {
        lastError_ = "Failed to connect to KVM host";
        return false;
    }

    return true;
}

void KVMBackupProvider::disconnect() {
    if (connection_) {
        virConnectClose(connection_);
        connection_ = nullptr;
    }
}

bool KVMBackupProvider::isConnected() const {
    return connection_ != nullptr;
}

std::vector<std::string> KVMBackupProvider::listVMs() const {
    std::vector<std::string> vms;
    
    if (!connection_) {
        lastError_ = "Not connected";
        return vms;
    }

    int numDomains = 0;
    virDomainPtr* domains = nullptr;
    
    numDomains = virConnectListAllDomains(connection_, &domains, VIR_CONNECT_LIST_DOMAINS_ACTIVE);
    if (numDomains < 0) {
        lastError_ = "Failed to list domains";
        return vms;
    }

    for (int i = 0; i < numDomains; i++) {
        const char* name = virDomainGetName(domains[i]);
        if (name) {
            vms.push_back(name);
            free(const_cast<char*>(name));
        }
        virDomainFree(domains[i]);
    }
    free(domains);

    return vms;
}

bool KVMBackupProvider::getVMDiskPaths(const std::string& vmId, std::vector<std::string>& paths) const {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    virDomainPtr domain = virDomainLookupByName(connection_, vmId.c_str());
    if (!domain) {
        lastError_ = "Failed to find VM: " + vmId;
        return false;
    }

    // Get domain XML
    char* xml = virDomainGetXMLDesc(domain, VIR_DOMAIN_XML_SECURE);
    if (!xml) {
        lastError_ = "Failed to get domain XML";
        virDomainFree(domain);
        return false;
    }

    // Parse XML to find disk paths
    std::string xmlStr(xml);
    size_t pos = 0;
    while ((pos = xmlStr.find("<disk", pos)) != std::string::npos) {
        // Find source element
        size_t sourcePos = xmlStr.find("<source", pos);
        if (sourcePos != std::string::npos) {
            // Find file attribute
            size_t filePos = xmlStr.find("file='", sourcePos);
            if (filePos != std::string::npos) {
                filePos += 6; // Skip "file='"
                size_t endPos = xmlStr.find("'", filePos);
                if (endPos != std::string::npos) {
                    std::string path = xmlStr.substr(filePos, endPos - filePos);
                    paths.push_back(path);
                }
            }
        }
        pos += 5; // Skip "<disk"
    }

    free(xml);
    virDomainFree(domain);

    return true;
}

bool KVMBackupProvider::getVMInfo(const std::string& vmId, std::string& name, std::string& status) const {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    virDomainPtr domain = virDomainLookupByName(connection_, vmId.c_str());
    if (!domain) {
        lastError_ = "Failed to find VM: " + vmId;
        return false;
    }

    name = vmId;
    int state;
    if (virDomainGetState(domain, &state, nullptr, 0) < 0) {
        virDomainFree(domain);
        lastError_ = "Failed to get VM state";
        return false;
    }

    switch (state) {
        case VIR_DOMAIN_RUNNING:
            status = "running";
            break;
        case VIR_DOMAIN_PAUSED:
            status = "paused";
            break;
        case VIR_DOMAIN_SHUTDOWN:
            status = "shutdown";
            break;
        case VIR_DOMAIN_SHUTOFF:
            status = "shutoff";
            break;
        default:
            status = "unknown";
    }

    virDomainFree(domain);
    return true;
}

bool KVMBackupProvider::startBackup(const std::string& vmId, const BackupConfig& config) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    // Get VM disk paths
    std::vector<std::string> diskPaths;
    if (!getVMDiskPaths(vmId, diskPaths)) {
        return false;
    }

    // Create backup directory if it doesn't exist
    std::filesystem::create_directories(config.backupPath);

    // Create snapshot for each disk
    for (const auto& diskPath : diskPaths) {
        std::string snapshotId = "backup_" + std::to_string(std::time(nullptr));
        if (!createSnapshot(vmId, snapshotId)) {
            return false;
        }
    }

    // Initialize CBT for each disk
    for (const auto& diskPath : diskPaths) {
        if (!initializeCBT(vmId)) {
            return false;
        }
    }

    // Start backup job
    std::string backupId = "backup_" + vmId + "_" + std::to_string(std::time(nullptr));
    backupJobs_[backupId] = std::make_unique<BackupJob>(shared_from_this(), config);
    
    if (progressCallback_) {
        progressCallback_(0.0);
    }
    if (statusCallback_) {
        statusCallback_("Backup started");
    }

    return true;
}

bool KVMBackupProvider::cancelBackup(const std::string& backupId) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    auto it = backupJobs_.find(backupId);
    if (it == backupJobs_.end()) {
        lastError_ = "Backup job not found";
        return false;
    }

    // Stop the backup job
    it->second->cancel();
    backupJobs_.erase(it);

    if (statusCallback_) {
        statusCallback_("Backup cancelled");
    }

    return true;
}

bool KVMBackupProvider::pauseBackup(const std::string& backupId) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    auto it = backupJobs_.find(backupId);
    if (it == backupJobs_.end()) {
        lastError_ = "Backup job not found";
        return false;
    }

    // TODO: Implement pause functionality in BackupJob
    // For now, just return false as pause is not implemented
    lastError_ = "Pause functionality not implemented";
    return false;
}

bool KVMBackupProvider::resumeBackup(const std::string& backupId) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    auto it = backupJobs_.find(backupId);
    if (it == backupJobs_.end()) {
        lastError_ = "Backup job not found";
        return false;
    }

    // TODO: Implement resume functionality in BackupJob
    // For now, just return false as resume is not implemented
    lastError_ = "Resume functionality not implemented";
    return false;
}

BackupStatus KVMBackupProvider::getBackupStatus(const std::string& backupId) const {
    BackupStatus status;
    auto it = backupJobs_.find(backupId);
    if (it == backupJobs_.end()) {
        status.state = BackupState::NotStarted;
        status.status = "not_found";
        status.progress = 0.0;
        return status;
    }

    std::string jobStatus = it->second->getStatus();
    if (jobStatus == "pending") {
        status.state = BackupState::NotStarted;
    } else if (jobStatus == "running") {
        status.state = BackupState::InProgress;
    } else if (jobStatus == "completed") {
        status.state = BackupState::Completed;
    } else if (jobStatus == "failed") {
        status.state = BackupState::Failed;
    } else if (jobStatus == "cancelled") {
        status.state = BackupState::Cancelled;
    } else if (jobStatus == "paused") {
        status.state = BackupState::Paused;
    }

    status.status = jobStatus;
    status.progress = it->second->getProgress();
    return status;
}

bool KVMBackupProvider::startRestore(const std::string& vmId, const std::string& backupId) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    // Get VM disk paths
    std::vector<std::string> diskPaths;
    if (!getVMDiskPaths(vmId, diskPaths)) {
        return false;
    }

    // Start restore job
    std::string restoreId = "restore_" + vmId + "_" + std::to_string(std::time(nullptr));
    RestoreConfig restoreConfig;
    restoreConfig.vmId = vmId;
    restoreConfig.backupId = backupId;
    // You may want to fill in more fields of restoreConfig as needed
    restoreJobs_[restoreId] = std::make_unique<RestoreJob>(vmId, backupId, restoreConfig);
    
    if (progressCallback_) {
        progressCallback_(0.0);
    }
    if (statusCallback_) {
        statusCallback_("Restore started");
    }

    return true;
}

bool KVMBackupProvider::cancelRestore(const std::string& restoreId) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    auto it = restoreJobs_.find(restoreId);
    if (it == restoreJobs_.end()) {
        lastError_ = "Restore job not found";
        return false;
    }

    // Stop the restore job
    it->second->stop();
    restoreJobs_.erase(it);

    if (statusCallback_) {
        statusCallback_("Restore cancelled");
    }

    return true;
}

bool KVMBackupProvider::pauseRestore(const std::string& restoreId) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    auto it = restoreJobs_.find(restoreId);
    if (it == restoreJobs_.end()) {
        lastError_ = "Restore job not found";
        return false;
    }

    it->second->pause();
    if (statusCallback_) {
        statusCallback_("Restore paused");
    }

    return true;
}

bool KVMBackupProvider::resumeRestore(const std::string& restoreId) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    auto it = restoreJobs_.find(restoreId);
    if (it == restoreJobs_.end()) {
        lastError_ = "Restore job not found";
        return false;
    }

    it->second->resume();
    if (statusCallback_) {
        statusCallback_("Restore resumed");
    }

    return true;
}

RestoreStatus KVMBackupProvider::getRestoreStatus(const std::string& restoreId) const {
    RestoreStatus status;
    auto it = restoreJobs_.find(restoreId);
    if (it == restoreJobs_.end()) {
        status.state = RestoreState::NotStarted;
        status.status = "not_found";
        status.progress = 0.0;
        return status;
    }

    std::string jobStatus = it->second->getStatus();
    if (jobStatus == "pending") {
        status.state = RestoreState::NotStarted;
    } else if (jobStatus == "running") {
        status.state = RestoreState::InProgress;
    } else if (jobStatus == "completed") {
        status.state = RestoreState::Completed;
    } else if (jobStatus == "failed") {
        status.state = RestoreState::Failed;
    } else if (jobStatus == "cancelled") {
        status.state = RestoreState::Cancelled;
    } else if (jobStatus == "paused") {
        status.state = RestoreState::Paused;
    }

    status.status = jobStatus;
    status.progress = it->second->getProgress();
    return status;
}

bool KVMBackupProvider::enableCBT(const std::string& vmId) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    std::vector<std::string> diskPaths;
    if (!getVMDiskPaths(vmId, diskPaths)) {
        return false;
    }

    for (const auto& diskPath : diskPaths) {
        auto cbt = cbtFactory_->createCBT(diskPath);
        if (!cbt || !cbt->enable()) {
            lastError_ = "Failed to enable CBT for disk: " + diskPath;
            return false;
        }
    }

    return true;
}

bool KVMBackupProvider::disableCBT(const std::string& vmId) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    std::vector<std::string> diskPaths;
    if (!getVMDiskPaths(vmId, diskPaths)) {
        return false;
    }

    for (const auto& diskPath : diskPaths) {
        auto cbt = cbtFactory_->createCBT(diskPath);
        if (!cbt || !cbt->disable()) {
            lastError_ = "Failed to disable CBT for disk: " + diskPath;
            return false;
        }
    }

    return true;
}

bool KVMBackupProvider::isCBTEnabled(const std::string& vmId) const {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    std::vector<std::string> diskPaths;
    if (!getVMDiskPaths(vmId, diskPaths)) {
        return false;
    }

    for (const auto& diskPath : diskPaths) {
        auto cbt = cbtFactory_->createCBT(diskPath);
        if (!cbt || !cbt->isEnabled()) {
            return false;
        }
    }

    return true;
}

bool KVMBackupProvider::getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                                       std::vector<std::pair<uint64_t, uint64_t>>& blocks) const {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    auto cbt = cbtFactory_->createCBT(diskPath);
    if (!cbt) {
        lastError_ = "Failed to create CBT for disk: " + diskPath;
        return false;
    }

    return cbt->getChangedBlocks(blocks);
}

void KVMBackupProvider::setProgressCallback(ProgressCallback callback) {
    progressCallback_ = std::move(callback);
}

void KVMBackupProvider::setStatusCallback(StatusCallback callback) {
    statusCallback_ = std::move(callback);
}

void KVMBackupProvider::clearLastError() {
    lastError_.clear();
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

bool KVMBackupProvider::createSnapshot(const std::string& vmId, const std::string& snapshotId) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    virDomainPtr domain = virDomainLookupByName(connection_, vmId.c_str());
    if (!domain) {
        lastError_ = "Failed to find VM: " + vmId;
        return false;
    }

    // Create snapshot
    virDomainSnapshotPtr snapshot = virDomainSnapshotCreateXML(domain,
        ("<domainsnapshot><name>" + snapshotId + "</name></domainsnapshot>").c_str(),
        VIR_DOMAIN_SNAPSHOT_CREATE_DISK_ONLY);
    
    if (!snapshot) {
        lastError_ = "Failed to create snapshot";
        virDomainFree(domain);
        return false;
    }

    virDomainSnapshotFree(snapshot);
    virDomainFree(domain);

    return true;
}

bool KVMBackupProvider::removeSnapshot(const std::string& vmId, const std::string& snapshotId) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    virDomainPtr domain = virDomainLookupByName(connection_, vmId.c_str());
    if (!domain) {
        lastError_ = "Failed to find VM: " + vmId;
        return false;
    }

    virDomainSnapshotPtr snapshot = virDomainSnapshotLookupByName(domain, snapshotId.c_str(), 0);
    if (!snapshot) {
        lastError_ = "Failed to find snapshot";
        virDomainFree(domain);
        return false;
    }

    if (virDomainSnapshotDelete(snapshot, VIR_DOMAIN_SNAPSHOT_DELETE_METADATA_ONLY) < 0) {
        lastError_ = "Failed to delete snapshot";
        virDomainSnapshotFree(snapshot);
        virDomainFree(domain);
        return false;
    }

    virDomainSnapshotFree(snapshot);
    virDomainFree(domain);

    return true;
}

bool KVMBackupProvider::verifyBackup(const std::string& backupId) {
    try {
        if (!std::filesystem::exists(backupId)) {
            lastError_ = "Backup not found: " + backupId;
            return false;
        }

        // Get backup metadata
        auto metadata = getLatestBackupInfo(backupId);
        if (!metadata) {
            lastError_ = "Failed to get backup metadata";
            return false;
        }

        // Verify each disk file
        for (const auto& disk : metadata->disks) {
            std::string diskPath = backupId + "/" + disk;
            if (!std::filesystem::exists(diskPath)) {
                lastError_ = "Disk file not found: " + diskPath;
                return false;
            }

            // Verify disk integrity
            if (!verifyDiskIntegrity(diskPath)) {
                lastError_ = "Disk integrity check failed: " + diskPath;
                return false;
            }
        }

        // Verify checksum
        std::string currentChecksum = calculateChecksum(backupId);
        if (currentChecksum != metadata->checksum) {
            lastError_ = "Checksum mismatch";
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Failed to verify backup: ") + e.what();
        return false;
    }
}

double KVMBackupProvider::getProgress() const {
    return progress_;
}

std::optional<BackupMetadata> KVMBackupProvider::getLatestBackupInfo(const std::string& backupId) {
    try {
        std::string metadataPath = backupId + "/metadata.json";
        if (!std::filesystem::exists(metadataPath)) {
            return std::nullopt;
        }

        std::ifstream file(metadataPath);
        if (!file.is_open()) {
            return std::nullopt;
        }

        nlohmann::json metadata;
        file >> metadata;
        
        BackupMetadata result;
        result.backupId = metadata["backupId"].get<std::string>();
        result.vmId = metadata["vmId"].get<std::string>();
        result.timestamp = metadata["timestamp"].get<int64_t>();
        result.type = static_cast<BackupType>(metadata["type"].get<int>());
        result.size = metadata["size"].get<int64_t>();
        result.disks = metadata["disks"].get<std::vector<std::string>>();
        result.checksum = metadata["checksum"].get<std::string>();
        
        return result;
    } catch (const std::exception& e) {
        lastError_ = std::string("Failed to read backup metadata: ") + e.what();
        return std::nullopt;
    }
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