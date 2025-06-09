#ifndef DISK_RESTORE_HPP
#define DISK_RESTORE_HPP

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include "vddk_wrapper/vddk_wrapper.h"

class DiskRestore {
public:
    DiskRestore();
    ~DiskRestore();

    // Initialize restore
    bool initialize();

    // Open source and target disks
    bool openDisks(const std::string& sourcePath,
                  const std::string& targetPath);

    // Close disks
    void closeDisks();

    // Restore disk
    bool restoreDisk(std::function<void(int)> progressCallback = nullptr);

    // Verify restore
    bool verifyRestore();

    // Get last error
    std::string getLastError() const;

private:
    VDDKConnection connection_;
    VDDKHandle sourceHandle_;
    VDDKHandle targetHandle_;
    std::string lastError_;
    mutable std::mutex mutex_;
};

#endif // DISK_RESTORE_HPP 
