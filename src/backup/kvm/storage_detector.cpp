#include "storage_detector.hpp"
#include "common/logger.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include <vector>

std::vector<StorageInfo> StorageDetector::detectStorageDevices() {
    std::vector<StorageInfo> devices;
    
    // Check for LVM devices
    detectLVMDevices(devices);
    
    // Check for QCOW2 devices
    detectQCOW2Devices(devices);
    
    return devices;
}

void StorageDetector::detectLVMDevices(std::vector<StorageInfo>& devices) {
    // Implementation for LVM device detection
    // This is a placeholder - implement actual LVM detection logic
}

void StorageDetector::detectQCOW2Devices(std::vector<StorageInfo>& devices) {
    // Implementation for QCOW2 device detection
    // This is a placeholder - implement actual QCOW2 detection logic
}

bool StorageDetector::isLVMDevice(const std::string& path) {
    // Check if the device is an LVM device
    return false; // Placeholder
}

bool StorageDetector::isQCOW2Device(const std::string& path) {
    // Check if the device is a QCOW2 device
    return false; // Placeholder
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