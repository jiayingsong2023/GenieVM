#include "backup/backup_manager.hpp"
#include "common/logger.hpp"
#include "common/thread_utils.hpp"
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

    // Initialize logger
    vmware::Logger::init();

    try {
        // Create and initialize backup manager
        vmware::BackupManager backupManager(host, username, password, maxParallel);
        if (!backupManager.initialize()) {
            vmware::Logger::error("Failed to initialize backup manager");
            return 1;
        }

        // Start the scheduler
        backupManager.startScheduler();

        if (!scheduleTime.empty()) {
            // Schedule a one-time backup
            time_t scheduledTime = parseDateTime(scheduleTime);
            vmware::Logger::info("Scheduling backup of VM: " + vmName + " at " + scheduleTime);
            if (!backupManager.scheduleBackup(vmName, backupDir, scheduledTime, useCBT)) {
                vmware::Logger::error("Failed to schedule backup");
                return 1;
            }
        } else if (!intervalStr.empty()) {
            // Schedule a periodic backup
            int interval = std::stoi(intervalStr);
            vmware::Logger::info("Scheduling periodic backup of VM: " + vmName + 
                               " every " + intervalStr + " seconds");
            if (!backupManager.schedulePeriodicBackup(vmName, backupDir, interval, useCBT)) {
                vmware::Logger::error("Failed to schedule periodic backup");
                return 1;
            }
        } else {
            // Perform immediate backup
            vmware::Logger::info("Starting backup of VM: " + vmName);
            if (!backupManager.backupVM(vmName, backupDir, useCBT)) {
                vmware::Logger::error("Backup failed");
                return 1;
            }
            vmware::Logger::info("Backup completed successfully");
            return 0;
        }

        // Keep the program running for scheduled backups
        while (true) {
            vmware::thread_utils::sleep_for_seconds(1);
        }
    } catch (const std::exception& e) {
        vmware::Logger::error("Exception occurred: " + std::string(e.what()));
        return 1;
    }
} 