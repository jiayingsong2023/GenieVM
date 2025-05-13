#pragma once

#include <string>
#include <memory>
#include <vector>
#include "backup/backup_job.hpp"
#include "backup/backup_scheduler.hpp"
#include "backup/restore_job.hpp"
#include "backup/backup_verifier.hpp"
#include "common/vsphere_rest_client.hpp"
#include "common/logger.hpp"

namespace vmware {

class BackupCLI {
public:
    BackupCLI(const std::string& vsphereUrl, const std::string& username, const std::string& password);
    void run(int argc, char* argv[]);

private:
    void printUsage();
    void handleBackup(int argc, char* argv[]);
    void handleRestore(int argc, char* argv[]);
    void handleSchedule(int argc, char* argv[]);
    void handleList(int argc, char* argv[]);
    void handleVerify(int argc, char* argv[]);
    
    void parseBackupOptions(int argc, char* argv[], BackupConfig& config);
    std::chrono::system_clock::time_point parseTime(const std::string& timeStr);
    void listBackups(const std::string& vmId);
    void listSchedules();

    std::unique_ptr<VSphereRestClient> vsphereClient_;
    std::unique_ptr<BackupScheduler> scheduler_;
};

} // namespace vmware 