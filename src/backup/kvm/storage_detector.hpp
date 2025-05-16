#pragma once

#include <string>
#include <vector>

struct StorageInfo {
    std::string path;
    std::string type;
    uint64_t size;
    bool isReadOnly;
};

class StorageDetector {
public:
    enum class StorageType {
        UNKNOWN,
        QCOW2,
        LVM,
        RAW
    };

    StorageDetector() = default;
    ~StorageDetector() = default;

    std::vector<StorageInfo> detectStorageDevices();
    void detectLVMDevices(std::vector<StorageInfo>& devices);
    void detectQCOW2Devices(std::vector<StorageInfo>& devices);
    bool isLVMDevice(const std::string& path);
    bool isQCOW2Device(const std::string& path);
    static StorageType detectStorageType(const std::string& path);
    static bool isQCOW2(const std::string& path);
    static bool isLVM(const std::string& path);
    static bool isRaw(const std::string& path);
}; 