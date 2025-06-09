#pragma once

#include <string>
#include <vector>
#include <chrono>

// Disk configuration for both backup and restore operations
struct DiskConfig {
    std::string path;      // Path to the disk
    int64_t sizeKB;       // Size in KB
    std::string format;   // Disk format (e.g., "thin", "thick", "eagerZeroedThick")
    std::string type;     // Disk type (e.g., "scsi", "ide", "sata")
    bool thinProvisioned; // Whether the disk is thin provisioned
};

// Configuration for backup operations
struct BackupConfig {
    std::string vmId;
    std::string sourcePath;  // Path to source disk
    std::string backupPath;  // Path to backup disk
    std::string backupDir;  // Directory to store backups
    std::string scheduleType;  // "daily", "weekly", "monthly", "once", "interval"
    struct {
        int hour = 0;
        int minute = 0;
        int day = 1;  // Day of week (0-6) for weekly, day of month (1-31) for monthly
    } schedule;
    int maxBackups{0};
    bool incremental{false};
    int compressionLevel{0};
    int maxConcurrentDisks{1};
    bool enableCBT{true};
    int retentionDays{7};
    std::vector<std::string> excludedDisks;
};

// Configuration for verify operations
struct VerifyConfig {
    std::string backupId;
    bool verifyChecksums = true;
    bool verifyMetadata = true;
    bool verifyData = true;
    int maxConcurrentDisks = 1;
};

// Configuration for restore operations
struct RestoreConfig {
    std::string vmId;
    std::string backupId;
    std::string vmName;           // Name of the VM to create
    std::string datastore;        // Target datastore
    std::string resourcePool;     // Target resource pool
    std::string guestOS;          // Guest OS type
    std::string restorePath;      // Path to restore the VM
    int numCPUs = 2;             // Number of CPUs
    int memoryMB = 4096;         // Memory size in MB
    bool verifyAfterRestore{true};
    bool powerOnAfterRestore{false};
    std::vector<DiskConfig> diskConfigs;
    int maxConcurrentDisks{1};
    std::vector<std::string> excludedDisks;
    // vSphere connection parameters
    std::string vsphereHost;
    std::string vsphereUsername;
    std::string vspherePassword;
}; 