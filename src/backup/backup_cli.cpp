#include "backup/backup_cli.hpp"
#include "backup/backup_manager.hpp"
#include "backup/backup_scheduler.hpp"
#include "backup/backup_verifier.hpp"
#include "backup/backup_job.hpp"
#include "backup/vmware/vmware_backup_provider.hpp"
#include "backup/vmware/vmware_connection.hpp"
#include "common/logger.hpp"
#include "main/backup_main.hpp"
#include <iostream>
#include <iomanip>
#include <ctime>
#include <filesystem>
#include <sstream>
#include <chrono>
#include <thread>
#include <memory>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

BackupCLI::BackupCLI(std::shared_ptr<VMwareConnection> connection)
    : connection_(connection) {
    // Create backup manager
    manager_ = std::make_shared<BackupManager>(connection);
    
    // Create scheduler
    scheduler_ = std::make_shared<BackupScheduler>(manager_);
    
    // Initialize scheduler
    if (!scheduler_->initialize()) {
        throw std::runtime_error("Failed to initialize backup scheduler");
    }
}

BackupCLI::~BackupCLI() {
    if (connection_) {
        connection_->disconnect();
    }
}

void BackupCLI::run(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return;
    }

    std::string command = argv[1];
    if (command == "backup") {
        handleBackupCommand(argc, argv);
    } else if (command == "schedule") {
        handleScheduleCommand(argc, argv);
    } else if (command == "list") {
        handleListCommand(argc, argv);
    } else if (command == "verify") {
        handleVerifyCommand(argc, argv);
    } else {
        printUsage();
    }
}

void BackupCLI::printUsage() const {
    // Delegate to backup_main.cpp's printBackupUsage
    printBackupUsage();
}

void BackupCLI::handleBackupCommand(int argc, char* argv[]) {
    if (argc < 2 || (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help"))) {
        printBackupUsage();
        return;
    }

    BackupConfig config;
    std::string host, username, password;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printBackupUsage();
            return;
        } else if (arg == "-v" || arg == "--vm-name") {
            if (i + 1 < argc) config.vmId = argv[++i];
        } else if (arg == "-b" || arg == "--backup-dir") {
            if (i + 1 < argc) config.backupDir = argv[++i];
        } else if (arg == "-s" || arg == "--server") {
            if (i + 1 < argc) host = argv[++i];
        } else if (arg == "-u" || arg == "--username") {
            if (i + 1 < argc) username = argv[++i];
        } else if (arg == "-p" || arg == "--password") {
            if (i + 1 < argc) password = argv[++i];
        } else if (arg == "-i" || arg == "--incremental") {
            config.incremental = true;
        } else if (arg == "--schedule") {
            if (i + 1 < argc) {
                std::string timeStr = argv[++i];
                config.scheduleType = "once";
                config.schedule.hour = std::stoi(timeStr.substr(11, 2));
                config.schedule.minute = std::stoi(timeStr.substr(14, 2));
            }
        } else if (arg == "--interval") {
            if (i + 1 < argc) {
                config.scheduleType = "interval";
                // Store interval in minutes in the hour field
                config.schedule.hour = std::stoi(argv[++i]) / 60;
                config.schedule.minute = std::stoi(argv[i]) % 60;
            }
        } else if (arg == "--parallel") {
            if (i + 1 < argc) config.maxConcurrentDisks = std::stoi(argv[++i]);
        } else if (arg == "--compression") {
            if (i + 1 < argc) config.compressionLevel = std::stoi(argv[++i]);
        } else if (arg == "--retention") {
            if (i + 1 < argc) config.retentionDays = std::stoi(argv[++i]);
        } else if (arg == "--max-backups") {
            if (i + 1 < argc) config.maxBackups = std::stoi(argv[++i]);
        } else if (arg == "--disable-cbt") {
            config.enableCBT = false;
        } else if (arg == "--exclude-disk") {
            if (i + 1 < argc) config.excludedDisks.push_back(argv[++i]);
        }
    }

    // Validate required parameters
    if (config.vmId.empty() || config.backupDir.empty() || 
        host.empty() || username.empty() || password.empty()) {
        std::cerr << "Error: Missing required parameters\n";
        printBackupUsage();
        return;
    }

    // Connect to vCenter/ESXi
    if (!connection_->connect(host, username, password)) {
        std::cerr << "Failed to connect to vCenter/ESXi\n";
        return;
    }

    // Create and run backup job
    auto provider = std::make_shared<VMwareBackupProvider>(connection_);
    auto job = std::make_shared<BackupJob>(provider, config);
    job->start();
}

void BackupCLI::handleScheduleCommand(int argc, char* argv[]) {
    if (argc < 2 || (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help"))) {
        printBackupUsage();
        return;
    }

    std::string host = argv[2];
    std::string username = argv[3];
    std::string password = argv[4];
    std::string schedule = argv[5];

    connection_ = std::make_shared<VMwareConnection>();
    if (!connection_->connect(host, username, password)) {
        std::cerr << "Failed to connect to vCenter\n";
        return;
    }

    BackupConfig config;
    parseBackupOptions(argc, argv, config);
    
    // Parse schedule string (format: "daily|weekly|monthly HH:MM [day]")
    std::istringstream ss(schedule);
    std::string type, time;
    ss >> type >> time;
    
    config.scheduleType = type;
    
    // Parse time
    std::istringstream time_ss(time);
    std::string hour_str, minute_str;
    std::getline(time_ss, hour_str, ':');
    std::getline(time_ss, minute_str, ':');
    config.schedule.hour = std::stoi(hour_str);
    config.schedule.minute = std::stoi(minute_str);
    
    // Parse day if provided
    if (ss >> config.schedule.day) {
        // Day was provided
    } else {
        // Default day based on schedule type
        if (type == "weekly") {
            config.schedule.day = 1;  // Monday
        } else if (type == "monthly") {
            config.schedule.day = 1;  // First day of month
        }
    }

    scheduler_->addSchedule(config.vmId, config);
    auto nextRunTime = scheduler_->getNextRunTime(config.vmId);
    std::cout << "Next backup scheduled for: " << this->formatTime(std::chrono::system_clock::to_time_t(nextRunTime)) << std::endl;
}

void BackupCLI::handleListCommand(int argc, char* argv[]) {
    if (argc < 2 || (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help"))) {
        printBackupUsage();
        return;
    }

    std::string host = argv[2];
    std::string username = argv[3];
    std::string password = argv[4];

    connection_ = std::make_shared<VMwareConnection>();
    if (!connection_->connect(host, username, password)) {
        std::cerr << "Failed to connect to vCenter\n";
        return;
    }

    std::vector<std::pair<std::string, BackupConfig>> schedules;
    scheduler_->getAllSchedules(schedules);
    for (const auto& [vmId, config] : schedules) {
        std::cout << "VM: " << vmId << "\n";
        std::cout << "Schedule Type: " << config.scheduleType << "\n";
        std::cout << "Schedule Time: " << config.schedule.hour << ":" << config.schedule.minute << "\n";
        if (config.scheduleType == "weekly") {
            std::cout << "Day of Week: " << config.schedule.day << "\n";
        } else if (config.scheduleType == "monthly") {
            std::cout << "Day of Month: " << config.schedule.day << "\n";
        }
        std::cout << "Next run: " << this->formatTime(std::chrono::system_clock::to_time_t(scheduler_->getNextRunTime(vmId))) << "\n\n";
    }
}

void BackupCLI::handleVerifyCommand(int argc, char* argv[]) {
    if (argc < 2 || (argc == 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help"))) {
        printBackupUsage();
        return;
    }

    std::string host = argv[2];
    std::string username = argv[3];
    std::string password = argv[4];

    connection_ = std::make_shared<VMwareConnection>();
    if (!connection_->connect(host, username, password)) {
        std::cerr << "Failed to connect to vCenter\n";
        return;
    }

    BackupConfig config;
    parseBackupOptions(argc, argv, config);
    
    auto provider = std::make_shared<VMwareBackupProvider>(connection_);
    auto job = std::make_shared<BackupJob>(provider, config);
    job->verifyBackup();
}

void BackupCLI::parseBackupOptions(int argc, char* argv[], BackupConfig& config) {
    for (int i = 5; i < argc; ++i) {
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

std::string BackupCLI::formatTime(time_t time) const {
    std::tm* tm = std::localtime(&time);
    std::stringstream ss;
    ss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

time_t BackupCLI::parseTime(const std::string& timeStr) const {
    std::tm tm = {};
    std::stringstream ss(timeStr);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    return std::mktime(&tm);
} 