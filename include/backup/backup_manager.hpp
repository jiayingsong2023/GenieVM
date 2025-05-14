#pragma once

#include <string>
#include <memory>
#include <vector>
#include "common/vmware_connection.hpp"
#include "backup/backup_config.hpp"
#include "backup/backup_job.hpp"

class BackupManager {
public:
    BackupManager(std::shared_ptr<VMwareConnection> connection);
    ~BackupManager();

    // Create a new backup job
    std::shared_ptr<BackupJob> createBackupJob(const BackupConfig& config);

    // Get a list of all backup jobs
    std::vector<std::shared_ptr<BackupJob>> getBackupJobs() const;

    // Get a specific backup job by ID
    std::shared_ptr<BackupJob> getBackupJob(const std::string& jobId) const;

    // Remove a backup job
    bool removeBackupJob(const std::string& jobId);

private:
    std::shared_ptr<VMwareConnection> connection_;
    std::vector<std::shared_ptr<BackupJob>> jobs_;
}; 