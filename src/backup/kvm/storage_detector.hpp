#pragma once

#include <string>
#include <vector>
#include <sys/ioctl.h>
#include <linux/fs.h>

class StorageDetector {
public:
    enum class StorageType {
        UNKNOWN,
        QCOW2,
        LVM,
        RAW
    };

    struct StorageInfo {
        std::string path;
        StorageType type;
        uint64_t size;
        bool isReadOnly;
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
    static uint64_t getDeviceSize(const std::string& path);
}; 