#ifndef DISK_BACKUP_HPP
#define DISK_BACKUP_HPP

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include "vddk_wrapper/vddk_wrapper.h"

class DiskBackup {
public:
    DiskBackup();
    ~DiskBackup();

    // Initialize backup
    bool initialize();

    // Open source and target disks
    bool openDisks(const std::string& sourcePath,
                  const std::string& targetPath,
                  uint64_t diskSize);

    // Close disks
    void closeDisks();

    // Backup disk
    bool backupDisk(std::function<void(int)> progressCallback = nullptr);

    // Verify backup
    bool verifyBackup();

    // Get last error
    std::string getLastError() const;

private:
    VDDKConnection connection_;
    VDDKHandle sourceHandle_;
    VDDKHandle targetHandle_;
    std::string lastError_;
    mutable std::mutex mutex_;
};

#endif // DISK_BACKUP_HPP 