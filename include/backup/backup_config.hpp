#pragma once

#include <string>
#include <chrono>

namespace vmware {

enum class BackupType {
    FULL,
    INCREMENTAL
};

struct BackupConfig {
    // Backup location
    std::string backupDir;
    
    // Backup type
    bool useCBT = true;  // Changed Block Tracking
    bool incremental = false;
    bool verify = false;
    bool compress = false;
    bool encrypt = false;
    
    // Retention settings
    int retentionDays = 7;
    int maxBackups = 10;
    
    // Performance settings
    int compressionLevel = 6;  // 0-9, higher means better compression but slower
    int maxConcurrentDisks = 4;
    
    // Schedule settings
    bool scheduled = false;
    std::chrono::system_clock::time_point scheduledTime;
    std::chrono::seconds interval;  // For periodic backups
    
    // Advanced settings
    bool quiesceVM = false;  // Use VMware Tools to quiesce the VM
    bool memorySnapshot = false;  // Include memory state in snapshot
    std::string description;
    BackupType type = BackupType::FULL;
};

} // namespace vmware 