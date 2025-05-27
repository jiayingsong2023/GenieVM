#include "backup/backup_cli.hpp"
#include "common/job_manager.hpp"
#include "backup/backup_scheduler.hpp"
#include "backup/backup_verifier.hpp"
#include "common/job.hpp"
#include "common/logger.hpp"
#include "main/backup_main.hpp"
#include "common/backup_status.hpp"
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

BackupCLI::BackupCLI(std::shared_ptr<JobManager> jobManager)
    : jobManager_(jobManager)
    , scheduler_(std::make_shared<BackupScheduler>(jobManager)) {
}

BackupCLI::~BackupCLI() {
}

void BackupCLI::run(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage();
        return;
    }

    // The first argument should be the command
    std::string command = argv[1];

    // Shift arguments to the left by one position
    for (int i = 0; i < argc - 1; i++) {
        argv[i] = argv[i + 1];
    }
    argc--;

    if (command == "backup") {
        handleBackupCommand(argc, argv);
    } else if (command == "schedule") {
        handleScheduleCommand(argc, argv);
    } else if (command == "list") {
        handleListCommand(argc, argv);
    } else if (command == "verify") {
        handleVerifyCommand(argc, argv);
    } else if (command == "restore") {
        handleRestoreCommand(argc, argv);
    } else {
        printUsage();
    }
}

void BackupCLI::printUsage() const {
    printBackupUsage();
}

void BackupCLI::handleBackupCommand(int argc, char* argv[]) {
    Logger::info("Starting backup command handling");
    BackupConfig config;
    std::string host, username, password;

    // Parse command line arguments
    for (int i = 0; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printBackupUsage();
            return;
        } else if (arg == "-v" || arg == "--vm-name") {
            if (i + 1 < argc) {
                config.vmId = argv[++i];
                Logger::debug("Parsed VM ID: " + config.vmId);
            }
        } else if (arg == "-b" || arg == "--backup-dir") {
            if (i + 1 < argc) {
                config.backupDir = argv[++i];
                Logger::debug("Parsed backup directory: " + config.backupDir);
            }
        } else if (arg == "-s" || arg == "--server") {
            if (i + 1 < argc) {
                host = argv[++i];
                Logger::debug("Parsed server host: " + host);
            }
        } else if (arg == "-u" || arg == "--username") {
            if (i + 1 < argc) {
                username = argv[++i];
                Logger::debug("Parsed username: " + username);
            }
        } else if (arg == "-p" || arg == "--password") {
            if (i + 1 < argc) {
                password = argv[++i];
                Logger::debug("Parsed password: [REDACTED]");
            }
        } else if (arg == "-i" || arg == "--incremental") {
            config.incremental = true;
        } else if (arg == "--schedule") {
            if (i + 1 < argc) {
                std::string timeStr = argv[++i];
                config.scheduleType = "once";
                config.schedule.hour = std::stoi(timeStr.substr(0, 2));
                config.schedule.minute = std::stoi(timeStr.substr(3, 2));
            }
        } else if (arg == "--interval") {
            if (i + 1 < argc) {
                config.scheduleType = "interval";
                int minutes = std::stoi(argv[++i]);
                config.schedule.hour = minutes / 60;
                config.schedule.minute = minutes % 60;
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
    if (config.vmId.empty() || config.backupDir.empty() || host.empty() || username.empty() || password.empty()) {
        Logger::error("Missing required parameters");
        Logger::error(std::string("VM name: ") + (config.vmId.empty() ? "missing" : "set"));
        Logger::error(std::string("Backup dir: ") + (config.backupDir.empty() ? "missing" : "set"));
        Logger::error(std::string("Server: ") + (host.empty() ? "missing" : "set"));
        Logger::error(std::string("Username: ") + (username.empty() ? "missing" : "set"));
        Logger::error(std::string("Password: ") + (password.empty() ? "missing" : "set"));
        printBackupUsage();
        return;
    }

    Logger::info("Starting backup process for VM: " + config.vmId);
    
    // Connect to server
    Logger::debug("Attempting to connect to server at: " + host);
    if (!jobManager_->connect(host, username, password)) {
        Logger::error("Failed to connect to server: " + jobManager_->getLastError());
        return;
    }

    Logger::info("Successfully connected to server");
    
    try {
        // Create and start backup job
        auto job = jobManager_->createBackupJob(config);
        if (!job) {
            Logger::error("Failed to create backup job: " + jobManager_->getLastError());
            return;
        }

        // Set up progress callback
        job->setProgressCallback([](int progress) {
            std::cout << "\rProgress: " << progress << "%" << std::flush;
        });

        // Set up status callback
        job->setStatusCallback([](const std::string& status) {
            std::cout << "\nStatus: " << status << std::endl;
        });

        // Start the job
        if (!job->start()) {
            Logger::error("Failed to start backup job: " + job->getError());
            return;
        }

        // Wait for job completion
        while (job->isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Print final status
        std::cout << "\nBackup job " << (job->isCompleted() ? "completed successfully" : "failed") << std::endl;
        if (!job->isCompleted()) {
            Logger::error("Error: " + job->getError());
        }

        Logger::info("Backup completed successfully for VM: " + config.vmId);
    } catch (const std::exception& e) {
        Logger::error("Unexpected error: " + std::string(e.what()));
    }
}

void BackupCLI::handleScheduleCommand(int argc, char** argv) {
    if (argc < 3) {
        printUsage();
        return;
    }

    BackupConfig config;
    config.vmId = argv[2];
    config.scheduleType = "daily";  // Default to daily
    config.schedule.hour = 0;
    config.schedule.minute = 0;

    // Parse schedule options
    for (int i = 3; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--type") {
            if (i + 1 < argc) {
                config.scheduleType = argv[++i];
            }
        } else if (arg == "--time") {
            if (i + 1 < argc) {
                std::string time = argv[++i];
                size_t pos = time.find(':');
                if (pos != std::string::npos) {
                    config.schedule.hour = std::stoi(time.substr(0, pos));
                    config.schedule.minute = std::stoi(time.substr(pos + 1));
                }
            }
        } else if (arg == "--day") {
            if (i + 1 < argc) {
                config.schedule.day = std::stoi(argv[++i]);
            }
        }
    }

    if (scheduler_->scheduleBackup(config.vmId, config)) {
        auto nextRunTime = scheduler_->getNextRunTime(config);
        std::cout << "Backup scheduled successfully\n";
        std::cout << "Next run: " << formatTime(std::chrono::system_clock::to_time_t(nextRunTime)) << "\n";
    } else {
        std::cerr << "Failed to schedule backup\n";
    }
}

void BackupCLI::handleListCommand(int argc, char** argv) {
    auto schedules = scheduler_->getScheduledBackups();
    
    if (schedules.empty()) {
        std::cout << "No scheduled backups\n";
        return;
    }

    for (const auto& config : schedules) {
        std::cout << "VM ID: " << config.vmId << "\n";
        std::cout << "Schedule Type: " << config.scheduleType << "\n";
        std::cout << "Time: " << config.schedule.hour << ":" << config.schedule.minute << "\n";
        if (config.scheduleType == "weekly" || config.scheduleType == "monthly") {
            std::cout << "Day: " << config.schedule.day << "\n";
        }
        std::cout << "Next run: " << formatTime(std::chrono::system_clock::to_time_t(scheduler_->getNextRunTime(config))) << "\n\n";
    }
}

bool BackupCLI::handleVerifyCommand(int argc, char* argv[]) {
    Logger::info("Starting verify command handling");
    VerifyConfig config;
    std::string host, username, password;

    // Parse command line arguments
    for (int i = 0; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage();
            return false;
        } else if (arg == "-b" || arg == "--backup-id") {
            if (i + 1 < argc) {
                config.backupId = argv[++i];
                Logger::debug("Parsed backup ID: " + config.backupId);
            }
        } else if (arg == "-s" || arg == "--server") {
            if (i + 1 < argc) {
                host = argv[++i];
                Logger::debug("Parsed server host: " + host);
            }
        } else if (arg == "-u" || arg == "--username") {
            if (i + 1 < argc) {
                username = argv[++i];
                Logger::debug("Parsed username: " + username);
            }
        } else if (arg == "-p" || arg == "--password") {
            if (i + 1 < argc) {
                password = argv[++i];
                Logger::debug("Parsed password: [REDACTED]");
            }
        } else if (arg == "--parallel") {
            if (i + 1 < argc) config.maxConcurrentDisks = std::stoi(argv[++i]);
        }
    }

    // Validate required parameters
    if (config.backupId.empty() || host.empty() || username.empty() || password.empty()) {
        Logger::error("Missing required parameters");
        printUsage();
        return false;
    }

    Logger::info("Starting verify process for backup: " + config.backupId);
    
    // Connect to server
    Logger::debug("Attempting to connect to server at: " + host);
    if (!jobManager_->connect(host, username, password)) {
        Logger::error("Failed to connect to server: " + jobManager_->getLastError());
        return false;
    }

    Logger::info("Successfully connected to server");
    
    try {
        // Create and start verify job
        auto job = jobManager_->createVerifyJob(config);
        if (!job) {
            Logger::error("Failed to create verify job: " + jobManager_->getLastError());
            return false;
        }

        // Set up progress callback
        job->setProgressCallback([](int progress) {
            std::cout << "\rProgress: " << progress << "%" << std::flush;
        });

        // Set up status callback
        job->setStatusCallback([](const std::string& status) {
            std::cout << "\nStatus: " << status << std::endl;
        });

        // Start the job
        if (!job->start()) {
            Logger::error("Failed to start verify job: " + job->getError());
            return false;
        }

        // Wait for job completion
        while (job->isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Print final status
        std::cout << "\nVerify job " << (job->isCompleted() ? "completed successfully" : "failed") << std::endl;
        if (!job->isCompleted()) {
            Logger::error("Error: " + job->getError());
            return false;
        }

        Logger::info("Verify completed successfully for backup: " + config.backupId);
        return true;
    } catch (const std::exception& e) {
        Logger::error("Error during verify: " + std::string(e.what()));
        return false;
    }
}

bool BackupCLI::handleRestoreCommand(int argc, char* argv[]) {
    Logger::info("Starting restore command handling");
    RestoreConfig config;
    std::string host, username, password;

    // Parse command line arguments
    for (int i = 0; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            printUsage();
            return false;
        } else if (arg == "-v" || arg == "--vm-name") {
            if (i + 1 < argc) {
                config.vmId = argv[++i];
                Logger::debug("Parsed VM ID: " + config.vmId);
            }
        } else if (arg == "-b" || arg == "--backup-id") {
            if (i + 1 < argc) {
                config.backupId = argv[++i];
                Logger::debug("Parsed backup ID: " + config.backupId);
            }
        } else if (arg == "-s" || arg == "--server") {
            if (i + 1 < argc) {
                host = argv[++i];
                Logger::debug("Parsed server host: " + host);
            }
        } else if (arg == "-u" || arg == "--username") {
            if (i + 1 < argc) {
                username = argv[++i];
                Logger::debug("Parsed username: " + username);
            }
        } else if (arg == "-p" || arg == "--password") {
            if (i + 1 < argc) {
                password = argv[++i];
                Logger::debug("Parsed password: [REDACTED]");
            }
        } else if (arg == "--parallel") {
            if (i + 1 < argc) config.maxConcurrentDisks = std::stoi(argv[++i]);
        } else if (arg == "--power-on") {
            config.powerOnAfterRestore = true;
        }
    }

    // Validate required parameters
    if (config.vmId.empty() || config.backupId.empty() || host.empty() || username.empty() || password.empty()) {
        Logger::error("Missing required parameters");
        printUsage();
        return false;
    }

    Logger::info("Starting restore process for VM: " + config.vmId);
    
    // Connect to server
    Logger::debug("Attempting to connect to server at: " + host);
    if (!jobManager_->connect(host, username, password)) {
        Logger::error("Failed to connect to server: " + jobManager_->getLastError());
        return false;
    }

    Logger::info("Successfully connected to server");
    
    try {
        // Create and start restore job
        auto job = jobManager_->createRestoreJob(config);
        if (!job) {
            Logger::error("Failed to create restore job: " + jobManager_->getLastError());
            return false;
        }

        // Set up progress callback
        job->setProgressCallback([](int progress) {
            std::cout << "\rProgress: " << progress << "%" << std::flush;
        });

        // Set up status callback
        job->setStatusCallback([](const std::string& status) {
            std::cout << "\nStatus: " << status << std::endl;
        });

        // Start the job
        if (!job->start()) {
            Logger::error("Failed to start restore job: " + job->getError());
            return false;
        }

        // Wait for job completion
        while (job->isRunning()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Print final status
        std::cout << "\nRestore job " << (job->isCompleted() ? "completed successfully" : "failed") << std::endl;
        if (!job->isCompleted()) {
            Logger::error("Error: " + job->getError());
            return false;
        }

        Logger::info("Restore completed successfully for VM: " + config.vmId);
        return true;
    } catch (const std::exception& e) {
        Logger::error("Error during restore: " + std::string(e.what()));
        return false;
    }
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
