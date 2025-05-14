#include "backup/backup_manager.hpp"
#include "common/logger.hpp"
#include "common/thread_utils.hpp"
#include "common/vmware_connection.hpp"
#include <iostream>
#include <string>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <cstdlib>

void printUsage() {
    std::cout << "Usage: vmware-backup [options]\n"
              << "Options:\n"
              << "  --host <vcenter-host>     vCenter host address\n"
              << "  --username <username>     vCenter username\n"
              << "  --password <password>     vCenter password\n"
              << "  --vm-name <vm-name>       Name of the VM to backup\n"
              << "  --backup-dir <directory>  Directory for backup\n"
              << "  --incremental             Use incremental backup (CBT)\n"
              << "  --schedule <time>         Schedule backup at specific time (YYYY-MM-DD HH:MM:SS)\n"
              << "  --interval <seconds>      Schedule periodic backup every N seconds\n"
              << "  --parallel <num>          Number of parallel backup tasks (default: 4)\n"
              << "  --help                    Show this help message\n";
}

time_t parseDateTime(const std::string& dateTimeStr) {
    std::tm tm = {};
    std::stringstream ss(dateTimeStr);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        throw std::runtime_error("Invalid date/time format");
    }
    return std::mktime(&tm);
}

int main(int argc, char* argv[]) {
    std::string host;
    std::string username;
    std::string password;
    std::string vmName;
    std::string backupDir;
    bool useCBT = false;
    std::string scheduleTime;
    std::string intervalStr;
    size_t maxParallel = 4;

    // Parse command line arguments
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--host" && i + 1 < argc) {
            host = argv[++i];
        } else if (arg == "--username" && i + 1 < argc) {
            username = argv[++i];
        } else if (arg == "--password" && i + 1 < argc) {
            password = argv[++i];
        } else if (arg == "--vm-name" && i + 1 < argc) {
            vmName = argv[++i];
        } else if (arg == "--backup-dir" && i + 1 < argc) {
            backupDir = argv[++i];
        } else if (arg == "--incremental") {
            useCBT = true;
        } else if (arg == "--schedule" && i + 1 < argc) {
            scheduleTime = argv[++i];
        } else if (arg == "--interval" && i + 1 < argc) {
            intervalStr = argv[++i];
        } else if (arg == "--parallel" && i + 1 < argc) {
            maxParallel = std::stoul(argv[++i]);
        } else if (arg == "--help") {
            printUsage();
            return 0;
        }
    }

    // Validate required parameters
    if (host.empty() || username.empty() || password.empty() ||
        vmName.empty() || backupDir.empty()) {
        std::cerr << "Error: Missing required parameters\n";
        printUsage();
        return 1;
    }

    try {
        // Create VMware connection
        auto connection = std::make_shared<VMwareConnection>(host, username, password);
        if (!connection->connect()) {
            std::cerr << "Failed to connect to vCenter" << std::endl;
            return 1;
        }

        // Create backup manager
        BackupManager backupManager(connection);

        // Create backup configuration
        BackupConfig config;
        config.vmId = vmName;  // Using vmName as vmId for now
        config.backupDir = backupDir;
        config.enableCBT = useCBT;
        config.maxConcurrentDisks = maxParallel;

        if (!scheduleTime.empty()) {
            // Schedule a one-time backup
            time_t scheduledTime = parseDateTime(scheduleTime);
            std::cout << "Scheduling backup of VM: " + vmName + " at " + scheduleTime << std::endl;
            config.scheduled = true;
            std::tm* tm = std::localtime(&scheduledTime);
            config.scheduleHour = tm->tm_hour;
            config.scheduleMinute = tm->tm_min;
            auto job = backupManager.createBackupJob(config);
            if (!job) {
                std::cerr << "Failed to schedule backup" << std::endl;
                return 1;
            }
        } else if (!intervalStr.empty()) {
            // Schedule a periodic backup
            int interval = std::stoi(intervalStr);
            std::cout << "Scheduling periodic backup of VM: " + vmName + 
                     " every " + intervalStr + " seconds" << std::endl;
            config.scheduled = true;
            // Convert interval to hours and minutes
            config.scheduleHour = interval / 3600;
            config.scheduleMinute = (interval % 3600) / 60;
            auto job = backupManager.createBackupJob(config);
            if (!job) {
                std::cerr << "Failed to schedule periodic backup" << std::endl;
                return 1;
            }
        } else {
            // Perform immediate backup
            std::cout << "Starting backup of VM: " + vmName << std::endl;
            config.scheduled = false;
            auto job = backupManager.createBackupJob(config);
            if (!job) {
                std::cerr << "Backup failed" << std::endl;
                return 1;
            }
            std::cout << "Backup completed successfully" << std::endl;
            return 0;
        }

        // Keep the program running for scheduled backups
        while (true) {
            thread_utils::sleep_for_seconds(1);
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception occurred: " << e.what() << std::endl;
        return 1;
    }
} 