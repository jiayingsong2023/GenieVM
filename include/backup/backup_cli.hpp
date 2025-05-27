#pragma once

#include "common/job_manager.hpp"
#include "backup/backup_scheduler.hpp"
#include "common/logger.hpp"
#include <memory>
#include <string>
#include <vector>
#include <ctime>

class BackupCLI {
public:
    explicit BackupCLI(std::shared_ptr<JobManager> jobManager);
    ~BackupCLI();

    void run(int argc, char* argv[]);
    void printUsage() const;

private:
    void handleBackupCommand(int argc, char* argv[]);
    void handleScheduleCommand(int argc, char** argv);
    void handleListCommand(int argc, char** argv);
    bool handleVerifyCommand(int argc, char* argv[]);
    bool handleRestoreCommand(int argc, char* argv[]);
    void parseBackupOptions(int argc, char* argv[], BackupConfig& config);
    std::string formatTime(time_t time) const;
    time_t parseTime(const std::string& timeStr) const;

    std::shared_ptr<JobManager> jobManager_;
    std::shared_ptr<BackupScheduler> scheduler_;
}; 