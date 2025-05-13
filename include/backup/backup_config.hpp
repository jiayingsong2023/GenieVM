#pragma once

#include <string>
#include <chrono>

namespace vmware {

struct BackupConfig {
    // Backup location
    std::string backupDir;
    
    // Backup type
    bool useCBT = true;  // Changed Block Tracking
    bool incremental = true;
    
    // Retention settings
    int retentionDays = 30;
    int maxBackups = 10;
    
    // Performance settings
    int compressionLevel = 6;  // 0-9, higher means better compression but slower
    int maxConcurrentDisks = 4;
    
    // Schedule settings
    bool scheduled = false;
    std::chrono::system_clock::time_point scheduledTime;
    std::chrono::seconds interval;  // For periodic backups
    
    // Advanced settings
    bool quiesceVM = true;  // Use VMware Tools to quiesce the VM
    bool memorySnapshot = false;  // Include memory state in snapshot
    std::string description;
};

} // namespace vmware 