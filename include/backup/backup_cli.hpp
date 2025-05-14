#pragma once

#include <string>
#include <memory>
#include <vector>
#include <map>
#include "backup/backup_config.hpp"
#include "backup/backup_job.hpp"
#include "backup/backup_verifier.hpp"
#include "common/vmware_connection.hpp"
#include "common/logger.hpp"

class BackupCLI {
public:
    BackupCLI(std::shared_ptr<VMwareConnection> connection);
    ~BackupCLI();

    void run(int argc, char* argv[]);

private:
    void printUsage();
    void handleBackup(int argc, char* argv[]);
    void handleRestore(int argc, char* argv[]);
    void handleSchedule(int argc, char* argv[]);
    void handleList(int argc, char* argv[]);
    void handleVerify(int argc, char* argv[]);
    void parseBackupOptions(int argc, char* argv[], BackupConfig& config);
    std::string formatTime(time_t time);
    time_t parseTime(const std::string& timeStr);

    std::shared_ptr<VMwareConnection> connection_;
    std::map<std::string, BackupConfig> scheduledBackups_;
}; 