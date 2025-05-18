#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <future>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "common/logger.hpp"
#include "common/vsphere_manager.hpp"
#include "backup/restore_job.hpp"
#include "restore/disk_restore.hpp"

class RestoreManager {
public:
    RestoreManager(const std::string& host,
                  const std::string& username,
                  const std::string& password);
    ~RestoreManager();

    // Initialize connection to vCenter
    bool initialize();

    // Restore a VM from backup
    bool restoreVM(const std::string& vmName,
                  const std::string& backupDir,
                  const std::string& datastore,
                  const std::string& resourcePool);

    // Get list of available backups
    std::vector<std::string> getAvailableBackups(const std::string& backupDir);

private:
    std::unique_ptr<VMwareConnection> connection_;
    std::string host_;
    std::string username_;
    std::string password_;
    std::string vmId_;
    bool initialized_;

    // Helper methods
    bool createVM(const std::string& vmName,
                 const std::string& datastore,
                 const std::string& resourcePool);
    bool attachDisks(const std::string& vmName,
                    const std::vector<std::string>& diskPaths);
    bool validateBackup(const std::string& backupDir);
    void logRestoreProgress(const std::string& message);
}; 