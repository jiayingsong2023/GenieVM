#include "backup/backup_cli.hpp"
#include "backup/backup_job.hpp"
#include "backup/backup_scheduler.hpp"
#include "backup/backup_verifier.hpp"
#include "common/vsphere_rest_client.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <filesystem>

namespace vmware {

BackupCLI::BackupCLI(const std::string& vsphereUrl, const std::string& username, const std::string& password)
    : vsphereClient_(std::make_unique<VSphereRestClient>(vsphereUrl, username, password))
    , scheduler_(std::make_unique<BackupScheduler>()) {
}

void BackupCLI::run(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return;
    }

    std::string command = argv[1];
    try {
        if (command == "backup") {
            handleBackup(argc, argv);
        } else if (command == "restore") {
            handleRestore(argc, argv);
        } else if (command == "schedule") {
            handleSchedule(argc, argv);
        } else if (command == "list") {
            handleList(argc, argv);
        } else if (command == "verify") {
            handleVerify(argc, argv);
        } else {
            std::cerr << "Unknown command: " << command << std::endl;
            printUsage();
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

void BackupCLI::printUsage() {
    std::cout << "Usage: genievm <command> [options]\n\n"
              << "Commands:\n"
              << "  backup <vm-id> [options]    Perform backup of a VM\n"
              << "  restore <vm-id> <backup-id>  Restore a VM from backup\n"
              << "  schedule <vm-id> [options]   Schedule backup for a VM\n"
              << "  list [vm-id]                List backups or scheduled backups\n"
              << "  verify <backup-id>          Verify backup integrity\n\n"
              << "Options:\n"
              << "  --dir <path>                Backup directory\n"
              << "  --incremental               Perform incremental backup\n"
              << "  --retention <days>          Retention period in days\n"
              << "  --max-backups <count>       Maximum number of backups to keep\n"
              << "  --compression <level>       Compression level (0-9)\n"
              << "  --quiesce                   Quiesce VM during backup\n"
              << "  --memory                    Include memory snapshot\n"
              << "  --time <HH:MM>              Schedule time (24-hour format)\n"
              << "  --interval <seconds>        Schedule interval in seconds\n"
              << "  --description <text>        Backup description\n";
}

void BackupCLI::handleBackup(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Error: VM ID required for backup" << std::endl;
        return;
    }

    std::string vmId = argv[2];
    BackupConfig config;
    parseBackupOptions(argc, argv, config);

    auto job = std::make_unique<BackupJob>(vmId, config);
    if (!job->start()) {
        std::cerr << "Failed to start backup job" << std::endl;
        return;
    }

    // Monitor backup progress
    while (job->getStatus() == BackupStatus::RUNNING) {
        std::cout << "\rProgress: " << std::fixed << std::setprecision(1) 
                  << (job->getProgress() * 100) << "%" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << std::endl;

    if (job->getStatus() == BackupStatus::COMPLETED) {
        std::cout << "Backup completed successfully" << std::endl;
    } else {
        std::cerr << "Backup failed: " << job->getErrorMessage() << std::endl;
    }
}

void BackupCLI::handleRestore(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Error: VM ID and backup ID required for restore" << std::endl;
        return;
    }

    std::string vmId = argv[2];
    std::string backupId = argv[3];

    // Parse restore options
    BackupConfig config;
    parseBackupOptions(argc, argv, config);

    // Create and start restore job
    auto job = std::make_unique<RestoreJob>(vmId, backupId, config);
    if (!job->start()) {
        std::cerr << "Failed to start restore job" << std::endl;
        return;
    }

    // Monitor restore progress
    while (job->getStatus() == RestoreStatus::RUNNING) {
        std::cout << "\rProgress: " << std::fixed << std::setprecision(1) 
                  << (job->getProgress() * 100) << "%" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << std::endl;

    if (job->getStatus() == RestoreStatus::COMPLETED) {
        std::cout << "Restore completed successfully" << std::endl;
    } else {
        std::cerr << "Restore failed: " << job->getErrorMessage() << std::endl;
    }
}

void BackupCLI::handleSchedule(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Error: VM ID required for scheduling" << std::endl;
        return;
    }

    std::string vmId = argv[2];
    BackupConfig config;
    parseBackupOptions(argc, argv, config);

    if (!config.scheduled) {
        std::cerr << "Error: Schedule time and interval required" << std::endl;
        return;
    }

    if (scheduler_->addSchedule(vmId, config)) {
        std::cout << "Backup scheduled successfully" << std::endl;
    } else {
        std::cerr << "Failed to schedule backup" << std::endl;
    }
}

void BackupCLI::handleList(int argc, char* argv[]) {
    if (argc > 2) {
        // List backups for specific VM
        std::string vmId = argv[2];
        listBackups(vmId);
    } else {
        // List all scheduled backups
        listSchedules();
    }
}

void BackupCLI::handleVerify(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Error: Backup ID required for verification" << std::endl;
        return;
    }

    std::string backupId = argv[2];
    BackupConfig config;
    parseBackupOptions(argc, argv, config);

    // Find the backup directory
    std::filesystem::path backupPath = std::filesystem::path(config.backupDir) / backupId;
    if (!std::filesystem::exists(backupPath)) {
        std::cerr << "Error: Backup not found: " << backupPath << std::endl;
        return;
    }

    // Get VM ID from backup directory name
    std::string vmId = backupId.substr(0, backupId.find('-'));
    if (vmId.empty()) {
        std::cerr << "Error: Invalid backup ID format" << std::endl;
        return;
    }

    // Get VM disk paths
    auto diskPaths = vsphereClient_->getVMDiskPaths(vmId);
    if (diskPaths.empty()) {
        std::cerr << "Error: No disks found for VM " << vmId << std::endl;
        return;
    }

    // Verify each disk
    size_t totalDisks = diskPaths.size();
    size_t verifiedDisks = 0;
    bool allVerified = true;

    std::cout << "Starting backup verification..." << std::endl;

    for (const auto& diskPath : diskPaths) {
        std::string backupDiskPath = (backupPath / std::filesystem::path(diskPath).filename()).string();
        
        // Create verifier
        BackupVerifier verifier(diskPath, backupDiskPath);
        if (!verifier.initialize()) {
            std::cerr << "Error: Failed to initialize verifier for disk: " << diskPath << std::endl;
            allVerified = false;
            continue;
        }

        // Set progress callback
        verifier.setProgressCallback([&](double progress) {
            std::cout << "\rDisk " << (verifiedDisks + 1) << "/" << totalDisks 
                      << " - Progress: " << std::fixed << std::setprecision(1) 
                      << (progress * 100) << "%" << std::flush;
        });

        // Perform verification
        bool success = verifier.verifyFull();
        std::cout << std::endl;

        if (success) {
            std::cout << "Disk verified successfully: " << diskPath << std::endl;
            verifiedDisks++;
        } else {
            std::cerr << "Verification failed for disk: " << diskPath << std::endl;
            std::cerr << "Error: " << verifier.getResult().errorMessage << std::endl;
            allVerified = false;
        }
    }

    // Print summary
    std::cout << "\nVerification Summary:" << std::endl;
    std::cout << "Total disks: " << totalDisks << std::endl;
    std::cout << "Verified disks: " << verifiedDisks << std::endl;
    std::cout << "Status: " << (allVerified ? "PASSED" : "FAILED") << std::endl;
}

void BackupCLI::parseBackupOptions(int argc, char* argv, BackupConfig& config) {
    for (int i = 3; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--dir" && i + 1 < argc) {
            config.backupDir = argv[++i];
        } else if (arg == "--incremental") {
            config.incremental = true;
        } else if (arg == "--retention" && i + 1 < argc) {
            config.retentionDays = std::stoi(argv[++i]);
        } else if (arg == "--max-backups" && i + 1 < argc) {
            config.maxBackups = std::stoi(argv[++i]);
        } else if (arg == "--compression" && i + 1 < argc) {
            config.compressionLevel = std::stoi(argv[++i]);
        } else if (arg == "--quiesce") {
            config.quiesceVM = true;
        } else if (arg == "--memory") {
            config.memorySnapshot = true;
        } else if (arg == "--time" && i + 1 < argc) {
            config.scheduled = true;
            config.scheduledTime = parseTime(argv[++i]);
        } else if (arg == "--interval" && i + 1 < argc) {
            config.interval = std::chrono::seconds(std::stoi(argv[++i]));
        } else if (arg == "--description" && i + 1 < argc) {
            config.description = argv[++i];
        }
    }
}

std::chrono::system_clock::time_point BackupCLI::parseTime(const std::string& timeStr) {
    std::tm tm = {};
    std::istringstream ss(timeStr);
    ss >> std::get_time(&tm, "%H:%M");
    
    auto now = std::chrono::system_clock::now();
    auto nowTime = std::chrono::system_clock::to_time_t(now);
    std::tm* nowTm = std::localtime(&nowTime);
    
    tm.tm_year = nowTm->tm_year;
    tm.tm_mon = nowTm->tm_mon;
    tm.tm_mday = nowTm->tm_mday;
    
    return std::chrono::system_clock::from_time_t(std::mktime(&tm));
}

void BackupCLI::listBackups(const std::string& vmId) {
    BackupConfig config;
    if (!scheduler_->getSchedule(vmId, config)) {
        std::cerr << "No schedule found for VM " << vmId << std::endl;
        return;
    }

    auto backupPaths = scheduler_->getBackupPaths(vmId);
    if (backupPaths.empty()) {
        std::cout << "No backups found for VM " << vmId << std::endl;
        return;
    }

    std::cout << "Backups for VM " << vmId << ":\n";
    for (const auto& path : backupPaths) {
        auto time = std::filesystem::last_write_time(path);
        auto timeT = std::chrono::system_clock::to_time_t(
            std::chrono::clock_cast<std::chrono::system_clock>(time));
        std::cout << std::put_time(std::localtime(&timeT), "%Y-%m-%d %H:%M:%S")
                  << " - " << std::filesystem::path(path).filename() << std::endl;
    }
}

void BackupCLI::listSchedules() {
    // Get all schedules from the scheduler
    std::vector<std::pair<std::string, BackupConfig>> schedules;
    scheduler_->getAllSchedules(schedules);

    if (schedules.empty()) {
        std::cout << "No scheduled backups found" << std::endl;
        return;
    }

    std::cout << "Scheduled Backups:\n" << std::endl;
    std::cout << std::left << std::setw(15) << "VM ID"
              << std::setw(20) << "Next Run"
              << std::setw(15) << "Interval"
              << std::setw(15) << "Retention"
              << std::setw(10) << "Type"
              << "Description" << std::endl;
    std::cout << std::string(80, '-') << std::endl;

    for (const auto& [vmId, config] : schedules) {
        // Get next run time
        auto nextRun = scheduler_->getNextRunTime(vmId);
        auto nextRunTime = std::chrono::system_clock::to_time_t(nextRun);
        
        // Format interval
        std::string interval;
        if (config.interval.count() >= 86400) {  // 24 hours
            interval = std::to_string(config.interval.count() / 86400) + " days";
        } else if (config.interval.count() >= 3600) {  // 1 hour
            interval = std::to_string(config.interval.count() / 3600) + " hours";
        } else {
            interval = std::to_string(config.interval.count() / 60) + " minutes";
        }

        // Format retention
        std::string retention = std::to_string(config.retentionDays) + " days";

        // Format backup type
        std::string type = config.incremental ? "Incremental" : "Full";

        // Print schedule information
        std::cout << std::left << std::setw(15) << vmId
                  << std::setw(20) << std::put_time(std::localtime(&nextRunTime), "%Y-%m-%d %H:%M")
                  << std::setw(15) << interval
                  << std::setw(15) << retention
                  << std::setw(10) << type
                  << config.description << std::endl;

        // Print additional configuration
        std::cout << "  Configuration:" << std::endl;
        std::cout << "    Backup Directory: " << config.backupDir << std::endl;
        std::cout << "    Max Backups: " << config.maxBackups << std::endl;
        std::cout << "    Compression Level: " << config.compressionLevel << std::endl;
        if (config.quiesceVM) {
            std::cout << "    Quiesce VM: Yes" << std::endl;
        }
        if (config.memorySnapshot) {
            std::cout << "    Memory Snapshot: Yes" << std::endl;
        }
        std::cout << std::endl;
    }
}

} // namespace vmware 