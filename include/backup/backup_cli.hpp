#pragma once

#include "backup/vm_config.hpp"
#include "backup/backup_job.hpp"
#include "backup/backup_manager.hpp"
#include "backup/backup_scheduler.hpp"
#include "backup/backup_verifier.hpp"
#include "common/vmware_connection.hpp"
#include "common/logger.hpp"
#include <memory>
#include <string>
#include <vector>
#include <ctime>

class BackupCLI {
public:
    explicit BackupCLI(std::shared_ptr<VMwareConnection> connection);
    ~BackupCLI();

    void run(int argc, char* argv[]);

private:
    std::shared_ptr<VMwareConnection> connection_;
    std::shared_ptr<BackupManager> manager_;
    std::shared_ptr<BackupScheduler> scheduler_;

    void parseBackupOptions(int argc, char* argv[], BackupConfig& config);
    void handleBackupCommand(int argc, char* argv[]);
    void handleScheduleCommand(int argc, char* argv[]);
    void handleListCommand(int argc, char* argv[]);
    void handleVerifyCommand(int argc, char* argv[]);
    void handleRestoreCommand(int argc, char* argv[]);
    void printUsage() const;
    std::string formatTime(time_t time) const;
    time_t parseTime(const std::string& timeStr) const;
}; 