#pragma once

#include <string>
#include <memory>
#include <vector>
#include "common/vmware_connection.hpp"
#include "restore/disk_restore.hpp"

namespace vmware {

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