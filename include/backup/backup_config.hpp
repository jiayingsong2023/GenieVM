#pragma once

#include <string>
#include <vector>

struct BackupConfig {
    std::string vmId;
    std::string backupDir;
    int compressionLevel = 6;  // 0-9, where 9 is maximum compression
    int maxConcurrentDisks = 4;
    int retentionDays = 30;
    int maxBackups = 10;
    bool enableCBT = true;  // Changed Block Tracking
    std::vector<std::string> excludedDisks;  // List of disk IDs to exclude from backup

    // Schedule configuration
    bool scheduled = false;
    int scheduleHour = 0;
    int scheduleMinute = 0;
}; 