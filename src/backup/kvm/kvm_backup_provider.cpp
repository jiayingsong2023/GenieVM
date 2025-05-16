#include "backup/kvm/kvm_backup_provider.hpp"
#include "common/logger.hpp"
#include <libvirt/libvirt.h>
#include <libvirt/virterror.h>
#include <sstream>
#include <stdexcept>
#include <filesystem>

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
    // TODO: Implement XML parsing to extract disk paths
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

    // Initialize CBT for each disk
    for (const auto& diskPath : diskPaths) {
        if (!initializeCBT(vmId)) {
            return false;
        }
    }

    // TODO: Implement backup job creation and start
    return true;
}

bool KVMBackupProvider::cancelBackup(const std::string& backupId) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }
    // TODO: Implement backup cancellation
    return false;
}

bool KVMBackupProvider::pauseBackup(const std::string& backupId) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }
    // TODO: Implement backup pause
    return false;
}

bool KVMBackupProvider::resumeBackup(const std::string& backupId) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }
    // TODO: Implement backup resume
    return false;
}

bool KVMBackupProvider::getBackupStatus(const std::string& backupId, std::string& status, double& progress) const {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }
    // TODO: Implement backup status check
    return false;
}

bool KVMBackupProvider::startRestore(const std::string& vmId, const std::string& backupId) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }
    // TODO: Implement restore
    return false;
}

bool KVMBackupProvider::cancelRestore(const std::string& restoreId) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }
    // TODO: Implement restore cancellation
    return false;
}

bool KVMBackupProvider::pauseRestore(const std::string& restoreId) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }
    // TODO: Implement restore pause
    return false;
}

bool KVMBackupProvider::resumeRestore(const std::string& restoreId) {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }
    // TODO: Implement restore resume
    return false;
}

bool KVMBackupProvider::getRestoreStatus(const std::string& restoreId, std::string& status, double& progress) const {
    if (!connection_) {
        lastError_ = "Not connected";
        return false;
    }
    // TODO: Implement restore status check
    return false;
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