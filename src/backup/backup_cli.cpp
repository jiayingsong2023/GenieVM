#include "backup/backup_cli.hpp"
#include "backup/backup_job.hpp"
#include "backup/backup_scheduler.hpp"
#include "backup/backup_verifier.hpp"
#include "common/vmware_connection.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <filesystem>
#include <sstream>
#include <chrono>
#include <thread>

BackupCLI::BackupCLI(std::shared_ptr<VMwareConnection> connection)
    : connection_(connection) {
}

BackupCLI::~BackupCLI() {
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
              << "  --compression <level>       Compression level (0-9)\n"
              << "  --concurrent-disks <num>    Maximum number of concurrent disk operations\n"
              << "  --retention <days>          Retention period in days\n"
              << "  --max-backups <count>       Maximum number of backups to keep\n"
              << "  --disable-cbt              Disable Changed Block Tracking\n"
              << "  --exclude-disk <id>        Exclude disk from backup\n";
}

void BackupCLI::handleBackup(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: backup <vm-id> [options]" << std::endl;
        return;
    }

    BackupConfig config;
    config.vmId = argv[2];  // Changed from argv[1] to argv[2] since command is at argv[1]
    parseBackupOptions(argc, argv, config);

    auto job = std::make_shared<BackupJob>(connection_, config);
    if (!job->start()) {
        std::cerr << "Failed to start backup job" << std::endl;
        return;
    }

    std::cout << "Backup job started. Progress: " << std::flush;
    while (job->getStatus() == BackupJob::Status::RUNNING) {
        std::cout << "\rProgress: " << (job->getProgress() * 100) << "%" << std::flush;
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    std::cout << std::endl;

    if (job->getStatus() == BackupJob::Status::COMPLETED) {
        std::cout << "Backup completed successfully" << std::endl;
    } else {
        std::cerr << "Backup failed: " << job->getErrorMessage() << std::endl;
    }
}

void BackupCLI::handleRestore(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: restore <vm-id> <backup-id>" << std::endl;
        return;
    }

    // TODO: Implement restore functionality
    std::cout << "Restore functionality not implemented yet" << std::endl;
}

void BackupCLI::handleSchedule(int argc, char* argv[]) {
    if (argc < 3) {
        std::cerr << "Usage: schedule <vm-id> [options]" << std::endl;
        return;
    }

    BackupConfig config;
    config.vmId = argv[2];  // Changed from argv[1] to argv[2] since command is at argv[1]
    parseBackupOptions(argc, argv, config);

    scheduledBackups_[config.vmId] = config;
    std::cout << "Backup scheduled for VM " << config.vmId << std::endl;
}

void BackupCLI::handleList(int argc, char* argv[]) {
    if (scheduledBackups_.empty()) {
        std::cout << "No scheduled backups" << std::endl;
        return;
    }

    std::cout << "Scheduled backups:" << std::endl;
    for (const auto& [vmId, config] : scheduledBackups_) {
        std::cout << "VM: " << vmId << std::endl;
        std::cout << "  Backup directory: " << config.backupDir << std::endl;
        std::cout << "  Compression level: " << config.compressionLevel << std::endl;
        std::cout << "  Max concurrent disks: " << config.maxConcurrentDisks << std::endl;
        std::cout << "  Retention days: " << config.retentionDays << std::endl;
        std::cout << "  Max backups: " << config.maxBackups << std::endl;
        std::cout << "  CBT enabled: " << (config.enableCBT ? "yes" : "no") << std::endl;
        if (!config.excludedDisks.empty()) {
            std::cout << "  Excluded disks: ";
            for (const auto& disk : config.excludedDisks) {
                std::cout << disk << " ";
            }
            std::cout << std::endl;
        }
        std::cout << std::endl;
    }
}

void BackupCLI::handleVerify(int argc, char* argv[]) {
    if (argc < 4) {
        std::cerr << "Usage: verify <vm-id> <backup-id>" << std::endl;
        return;
    }

    std::string vmId = argv[2];  // Changed from argv[1] to argv[2] since command is at argv[1]
    std::string backupId = argv[3];

    // Get VM disk paths
    std::vector<std::string> diskPaths;
    if (!connection_->getVMDiskPaths(vmId, diskPaths)) {
        std::cerr << "Failed to get VM disk paths" << std::endl;
        return;
    }

    // Find the backup directory
    std::filesystem::path backupPath = std::filesystem::path("backups") / backupId;
    if (!std::filesystem::exists(backupPath)) {
        std::cerr << "Error: Backup not found: " << backupPath << std::endl;
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
        auto verifier = std::make_unique<BackupVerifier>(diskPath, backupDiskPath);
        if (!verifier->initialize()) {
            std::cerr << "Error: Failed to initialize verifier for disk: " << diskPath << std::endl;
            allVerified = false;
            continue;
        }

        // Set progress callback
        verifier->setProgressCallback([&](double progress) {
            std::cout << "\rDisk " << (verifiedDisks + 1) << "/" << totalDisks 
                      << " - Progress: " << std::fixed << std::setprecision(1) 
                      << (progress * 100) << "%" << std::flush;
        });

        // Perform verification
        bool success = verifier->verifyFull();
        std::cout << std::endl;

        if (success) {
            std::cout << "Disk verified successfully: " << diskPath << std::endl;
            verifiedDisks++;
        } else {
            std::cerr << "Verification failed for disk: " << diskPath << std::endl;
            std::cerr << "Error: " << verifier->getResult().errorMessage << std::endl;
            allVerified = false;
        }
    }

    // Print summary
    std::cout << "\nVerification Summary:" << std::endl;
    std::cout << "Total disks: " << totalDisks << std::endl;
    std::cout << "Verified disks: " << verifiedDisks << std::endl;
    std::cout << "Status: " << (allVerified ? "PASSED" : "FAILED") << std::endl;
}

void BackupCLI::parseBackupOptions(int argc, char* argv[], BackupConfig& config) {
    for (int i = 3; i < argc; ++i) {  // Start from argv[3] since command and vm-id are at argv[1] and argv[2]
        std::string arg = argv[i];
        if (arg == "--dir" && i + 1 < argc) {
            config.backupDir = argv[++i];
        } else if (arg == "--compression" && i + 1 < argc) {
            config.compressionLevel = std::stoi(argv[++i]);
        } else if (arg == "--concurrent-disks" && i + 1 < argc) {
            config.maxConcurrentDisks = std::stoi(argv[++i]);
        } else if (arg == "--retention" && i + 1 < argc) {
            config.retentionDays = std::stoi(argv[++i]);
        } else if (arg == "--max-backups" && i + 1 < argc) {
            config.maxBackups = std::stoi(argv[++i]);
        } else if (arg == "--disable-cbt") {
            config.enableCBT = false;
        } else if (arg == "--exclude-disk" && i + 1 < argc) {
            config.excludedDisks.push_back(argv[++i]);
        }
    }
}

std::string BackupCLI::formatTime(time_t time) {
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

time_t BackupCLI::parseTime(const std::string& timeStr) {
    std::tm tm = {};
    std::stringstream ss(timeStr);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return std::mktime(&tm);
} 