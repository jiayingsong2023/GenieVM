#include "storage_detector.hpp"
#include "common/logger.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <vector>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <linux/fs.h>

std::vector<StorageDetector::StorageInfo> StorageDetector::detectStorageDevices() {
    std::vector<StorageInfo> devices;
    
    // Check for LVM devices
    detectLVMDevices(devices);
    
    // Check for QCOW2 devices
    detectQCOW2Devices(devices);
    
    return devices;
}

void StorageDetector::detectLVMDevices(std::vector<StorageInfo>& devices) {
    // Scan /dev/mapper directory for LVM devices
    for (const auto& entry : std::filesystem::directory_iterator("/dev/mapper")) {
        if (entry.is_block_file() || entry.is_character_file()) {
            std::string path = entry.path().string();
            if (isLVMDevice(path)) {
                StorageInfo info;
                info.path = path;
                info.type = StorageType::LVM;
                info.size = getDeviceSize(path);
                info.isReadOnly = false;
                devices.push_back(info);
            }
        }
    }

    // Scan /dev directory for volume groups
    for (const auto& entry : std::filesystem::directory_iterator("/dev")) {
        if (entry.is_directory()) {
            std::string dirName = entry.path().filename().string();
            if (dirName.find("vg_") == 0) {
                for (const auto& subEntry : std::filesystem::directory_iterator(entry.path())) {
                    if (subEntry.is_block_file() || subEntry.is_character_file()) {
                        std::string path = subEntry.path().string();
                        if (isLVMDevice(path)) {
                            StorageInfo info;
                            info.path = path;
                            info.type = StorageType::LVM;
                            info.size = getDeviceSize(path);
                            info.isReadOnly = false;
                            devices.push_back(info);
                        }
                    }
                }
            }
        }
    }
}

void StorageDetector::detectQCOW2Devices(std::vector<StorageInfo>& devices) {
    // Scan common QCOW2 locations
    std::vector<std::string> searchPaths = {
        "/var/lib/libvirt/images",
        "/var/lib/libvirt/qemu",
        "/var/lib/libvirt/images/snapshots"
    };

    for (const auto& basePath : searchPaths) {
        if (std::filesystem::exists(basePath)) {
            for (const auto& entry : std::filesystem::recursive_directory_iterator(basePath)) {
                if (entry.is_regular_file()) {
                    std::string path = entry.path().string();
                    if (isQCOW2Device(path)) {
                        StorageInfo info;
                        info.path = path;
                        info.type = StorageType::QCOW2;
                        info.size = std::filesystem::file_size(path);
                        info.isReadOnly = false;
                        devices.push_back(info);
                    }
                }
            }
        }
    }
}

bool StorageDetector::isLVMDevice(const std::string& path) {
    // Check if it's a block device
    struct stat st;
    if (stat(path.c_str(), &st) != 0) {
        return false;
    }

    // Check if it's a block device
    if (!S_ISBLK(st.st_mode) && !S_ISCHR(st.st_mode)) {
        return false;
    }

    // Check if it's in /dev/mapper/ or a volume group
    if (path.find("/dev/mapper/") == 0 || 
        (path.find("/dev/") == 0 && path.find("/") != std::string::npos)) {
        
        // Try to get LVM information
        std::string cmd = "lvs --noheadings --nosuffix --units b " + path + " 2>/dev/null";
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

bool StorageDetector::isQCOW2Device(const std::string& path) {
    // Check if file exists and is readable
    std::ifstream file(path, std::ios::binary);
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

uint64_t StorageDetector::getDeviceSize(const std::string& path) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) {
        return 0;
    }

    uint64_t size = 0;
    if (ioctl(fd, BLKGETSIZE64, &size) == -1) {
        close(fd);
        return 0;
    }

    close(fd);
    return size;
}

StorageDetector::StorageType StorageDetector::detectStorageType(const std::string& path) {
    if (isQCOW2(path)) {
        return StorageType::QCOW2;
    }
    
    if (isLVM(path)) {
        return StorageType::LVM;
    }
    
    if (isRaw(path)) {
        return StorageType::RAW;
    }
    
    return StorageType::UNKNOWN;
}

bool StorageDetector::isQCOW2(const std::string& path) {
    // QCOW2 magic number: QFI\xfb
    const char* qcow2_magic = "QFI\xfb";
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    char magic[4];
    file.read(magic, 4);
    return memcmp(magic, qcow2_magic, 4) == 0;
}

bool StorageDetector::isLVM(const std::string& path) {
    // Check if path starts with /dev/mapper/ or /dev/vg_name/
    return path.find("/dev/mapper/") == 0 || 
           path.find("/dev/") == 0 && path.find("/") != std::string::npos;
}

bool StorageDetector::isRaw(const std::string& path) {
    // Check if it's a regular file or block device
    std::ifstream file(path, std::ios::binary);
    return file.good();
} 