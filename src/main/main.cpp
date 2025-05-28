#include "common/backup_cli.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <string>


void printBackupUsage() {
    std::cout << "Usage: genievm backup [command] [options]\n"
              << "Commands:\n"
              << "  backup    - Create a backup of a VM\n"
              << "  schedule  - Schedule a backup\n"
              << "  list      - List scheduled backups\n"
              << "  verify    - Verify a backup\n"
              << "  restore   - Restore from a backup\n"
              << "\n"
              << "Options:\n"
              << "  -h, --help           Show this help message\n"
              << "  -v, --vm-name        VM name or ID\n"
              << "  -b, --backup-dir     Backup directory\n"
              << "  -s, --server         Server address\n"
              << "  -u, --username       Username\n"
              << "  -p, --password       Password\n"
              << "  -i, --incremental    Enable incremental backup\n"
              << "  --schedule           Schedule time (HH:MM)\n"
              << "  --interval           Interval in minutes\n"
              << "  --parallel           Number of parallel disk operations\n"
              << "  --compression        Compression level (0-9)\n"
              << "  --retention          Retention period in days\n"
              << "  --max-backups        Maximum number of backups to keep\n"
              << "  --disable-cbt        Disable Changed Block Tracking\n"
              << "  --exclude-disk       Exclude disk from backup\n"
              << "  --vm-type            Backup provider type (vmware/kvm)\n";
}

int main(int argc, char** argv) {
    std::cout << "Starting GenieVM with " << argc << " arguments:" << std::endl;
    for (int i = 0; i < argc; i++) {
        std::cout << "  argv[" << i << "]: " << argv[i] << std::endl;
    }

    // Check for help flag first, before any initialization
    if (argc > 1 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        std::cout << "Help flag detected, showing usage" << std::endl;
        printBackupUsage();
        return 0;
    }

    // Check for version flag
    if (argc > 1 && (std::string(argv[1]) == "--version" || std::string(argv[1]) == "-v")) {
        std::cout << "Version flag detected, showing version" << std::endl;
        std::cout << "GenieVM version 1.0.0\n";
        return 0;
    }

    // Check for command
    if (argc < 2) {
        std::cerr << "Error: No command specified" << std::endl;
        printBackupUsage();
        return 1;
    }

    std::string command = argv[1];
    std::cout << "Command detected: " << command << std::endl;

    // Initialize logger only if we're not showing help or version
    if (command != "--help" && command != "-h" && command != "--version" && command != "-v") {
        std::cout << "Initializing logger..." << std::endl;
        if (!Logger::initialize("/tmp/genievm.log", LogLevel::DEBUG)) {
            std::cerr << "Failed to initialize logger" << std::endl;
            return 1;
        }
        std::cout << "Logger initialized successfully" << std::endl;
    } else {
        std::cout << "Skipping logger initialization for help/version command" << std::endl;
    }

    try {
        BackupCLI cli;
        cli.run(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Error in main: " << e.what() << std::endl;
        return 1;
    }
} 

