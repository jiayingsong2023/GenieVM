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
};

// Configuration for backup operations
struct BackupConfig {
    std::string vmId;
    std::string sourcePath;  // Path to source disk
    std::string backupPath;  // Path to backup disk
    std::string backupDir;  // Directory to store backups
    std::string scheduleType;  // "daily", "weekly", "monthly"
    struct {
        int hour = 0;
        int minute = 0;
        int day = 1;  // Day of week (0-6) for weekly, day of month (1-31) for monthly
    } schedule;
    int compressionLevel = 0;
    int maxConcurrentDisks = 1;
    int retentionDays = 7;
    int maxBackups = 10;
    bool enableCBT = true;
    bool incremental = false;  // Whether to perform incremental backup
    std::vector<std::string> excludedDisks;
};

// Configuration for restore operations
struct RestoreConfig {
    std::string vmId;
    std::string backupId;
    std::string targetDatastore;
    std::string targetResourcePool;
    std::vector<DiskConfig> diskConfigs;
    // vSphere connection parameters
    std::string vsphereHost;
    std::string vsphereUsername;
    std::string vspherePassword;
}; 