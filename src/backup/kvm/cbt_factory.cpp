#include "backup/kvm/cbt_factory.hpp"
#include "backup/kvm/qcow2_cbt.hpp"
#include "backup/kvm/lvm_cbt.hpp"
#include "backup/vmware/vmware_backup_provider.hpp"
#include "common/logger.hpp"
#include "common/vmware_connection.hpp"
#include <stdexcept>
#include <fstream>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/fs.h>

std::unique_ptr<CBT> CBTFactory::createCBT(const std::string& diskPath) {
    // Determine the type of disk and create appropriate CBT implementation
    if (isQCOW2Disk(diskPath)) {
        return std::make_unique<QCOW2CBT>(diskPath);
    } else if (isLVMDisk(diskPath)) {
        return std::make_unique<LVMCBT>(diskPath);
    }
    
    throw std::runtime_error("Unsupported disk type for CBT: " + diskPath);
}

bool CBTFactory::isQCOW2Disk(const std::string& diskPath) {
    // Check if file exists and is readable
    std::ifstream file(diskPath, std::ios::binary);
    if (!file) {
        return false;
    }

    // Read QCOW2 header
    struct {
        uint32_t magic;
        uint32_t version;
        uint64_t backing_file_offset;
        uint32_t backing_file_size;
        uint32_t cluster_bits;
        uint64_t size;
        uint32_t crypt_method;
        uint32_t l1_size;
        uint64_t l1_table_offset;
        uint64_t refcount_table_offset;
        uint32_t refcount_table_clusters;
        uint32_t nb_snapshots;
        uint64_t snapshots_offset;
    } header;

    file.read(reinterpret_cast<char*>(&header), sizeof(header));
    
    // Check QCOW2 magic number (QFI\xfb)
    return header.magic == 0x514649fb;
}

bool CBTFactory::isLVMDisk(const std::string& diskPath) {
    // Check if it's a block device
    struct stat st;
    if (stat(diskPath.c_str(), &st) != 0) {
        return false;
    }

    // Check if it's a block device
    if (!S_ISBLK(st.st_mode) && !S_ISCHR(st.st_mode)) {
        return false;
    }

    // Check if it's in /dev/mapper/ or a volume group
    if (diskPath.find("/dev/mapper/") == 0 || 
        (diskPath.find("/dev/") == 0 && diskPath.find("/") != std::string::npos)) {
        
        // Try to get LVM information
        std::string cmd = "lvs --noheadings --nosuffix --units b " + diskPath + " 2>/dev/null";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            return false;
        }

        char buffer[256];
        bool hasLVMInfo = fgets(buffer, sizeof(buffer), pipe) != nullptr;
        pclose(pipe);
        return hasLVMInfo;
    }

    return false;
}

std::shared_ptr<BackupProvider> CBTFactory::createProvider(const std::string& type) {
    if (type == "vmware") {
        auto connection = std::make_shared<VMwareConnection>("", "", "");
        return std::make_shared<VMwareBackupProvider>(connection);
    }
    throw std::runtime_error("Unsupported provider type: " + type);
}

bool CBTFactory::isCBTEnabled(const std::string& vmId) {
    auto connection = createConnection();
    if (!connection) {
        return false;
    }

    bool enabled;
    std::string changeId;
    return connection->getCBTInfo(vmId, enabled, changeId) && enabled;
}

bool CBTFactory::enableCBT(const std::string& vmId) {
    auto connection = createConnection();
    if (!connection) {
        return false;
    }

    return connection->enableCBT(vmId);
}

bool CBTFactory::disableCBT(const std::string& vmId) {
    auto connection = createConnection();
    if (!connection) {
        return false;
    }

    return connection->disableCBT(vmId);
}

bool CBTFactory::getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                                std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) {
    auto connection = createConnection();
    if (!connection) {
        return false;
    }

    return connection->getChangedBlocks(vmId, diskPath, changedBlocks);
}

std::shared_ptr<VMwareConnection> CBTFactory::createConnection() {
    return std::make_shared<VMwareConnection>();
} 
