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

bool KVMBackupProvider::getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths) const {
    if (!connection_) {
        lastError_ = "Not connected to KVM host";
        return false;
    }

    virDomainPtr domain = virDomainLookupByUUIDString(connection_, vmId.c_str());
    if (!domain) {
        lastError_ = "Failed to find VM with ID: " + vmId;
        return false;
    }

    char* xmlDesc = virDomainGetXMLDesc(domain, 0);
    if (!xmlDesc) {
        virDomainFree(domain);
        lastError_ = "Failed to get VM XML description";
        return false;
    }

    // Parse XML to get disk paths
    // This is a placeholder implementation
    // TODO: Implement proper XML parsing
    diskPaths.push_back("/path/to/disk1.qcow2");
    diskPaths.push_back("/path/to/disk2.qcow2");

    free(xmlDesc);
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

bool KVMBackupProvider::backupDisk(const std::string& vmId, const std::string& diskPath, const BackupConfig& config) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    // Create backup directory if it doesn't exist
    std::filesystem::create_directories(config.backupPath);

    // Create snapshot for the disk
    std::string snapshotId = "backup_" + std::to_string(std::time(nullptr));
    if (!createSnapshot(vmId, snapshotId)) {
        return false;
    }

    // Initialize CBT for the disk
    if (!initializeCBT(vmId)) {
        return false;
    }

    // Start backup job
    std::string backupId = "backup_" + vmId + "_" + std::to_string(std::time(nullptr));
    auto taskManager = std::make_shared<ParallelTaskManager>();
    backupJobs_[backupId] = std::make_shared<BackupJob>(shared_from_this(), taskManager, config);
    
    if (progressCallback_) {
        progressCallback_(0);
    }
    if (statusCallback_) {
        statusCallback_("Backup started");
    }

    return true;
}

bool KVMBackupProvider::restoreDisk(const std::string& vmId, const std::string& diskPath, const RestoreConfig& config) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    // Create restore job
    auto taskManager = std::make_shared<ParallelTaskManager>();
    auto job = std::make_shared<RestoreJob>(shared_from_this(), taskManager, config);
    
    if (progressCallback_) {
        progressCallback_(0);
    }
    if (statusCallback_) {
        statusCallback_("Restore started");
    }

    return true;
}

bool KVMBackupProvider::verifyDisk(const std::string& diskPath) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    // Check if file exists and is readable
    std::ifstream file(diskPath, std::ios::binary);
    if (!file) {
        lastError_ = "Failed to open disk file: " + diskPath;
        return false;
    }

    // Verify disk format
    std::string format = getDiskFormat(diskPath);
    if (format.empty()) {
        lastError_ = "Failed to determine disk format";
        return false;
    }

    // Verify disk integrity
    return verifyDiskIntegrity(diskPath);
}

bool KVMBackupProvider::getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                                       std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }

    if (!cbtFactory_) {
        lastError_ = "CBT factory not initialized";
        return false;
    }

    auto cbt = cbtFactory_->createCBT(diskPath);
    if (!cbt) {
        lastError_ = "Failed to create CBT for disk: " + diskPath;
        return false;
    }

    return cbt->getChangedBlocks(changedBlocks);
}

bool KVMBackupProvider::listBackups(std::vector<std::string>& backupIds) {
    std::lock_guard<std::mutex> lock(mutex_);
    backupIds.clear();
    for (const auto& job : backupJobs_) {
        backupIds.push_back(job.first);
    }
    return true;
}

bool KVMBackupProvider::deleteBackup(const std::string& backupId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = backupJobs_.find(backupId);
    if (it == backupJobs_.end()) {
        lastError_ = "Backup not found: " + backupId;
        return false;
    }

    backupJobs_.erase(it);
    return true;
}

bool KVMBackupProvider::verifyBackup(const std::string& backupId) {
    std::lock_guard<std::mutex> lock(mutex_);
    auto it = backupJobs_.find(backupId);
    if (it == backupJobs_.end()) {
        lastError_ = "Backup not found: " + backupId;
        return false;
    }

    return it->second->verifyBackup();
}

void KVMBackupProvider::clearLastError() {
    std::lock_guard<std::mutex> lock(mutex_);
    lastError_.clear();
}

double KVMBackupProvider::getProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_;
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