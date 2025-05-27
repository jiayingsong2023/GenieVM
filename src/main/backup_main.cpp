#include "main/backup_main.hpp"
#include "backup/backup_cli.hpp"
#include "common/job_manager.hpp"
#include "backup/backup_provider.hpp"
#include "backup/backup_provider_factory.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <memory>
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
              << "  --provider           Backup provider type (vmware/kvm)\n";
}

int backupMain(int argc, char** argv) {
    try {
        // Create JobManager
        auto jobManager = std::make_shared<JobManager>();

        // Parse provider type from command line
        std::string providerType = "vmware"; // Default to VMware
        std::string connectionString;
        for (int i = 1; i < argc; i++) {
            std::string arg = argv[i];
            if (arg == "--provider" && i + 1 < argc) {
                providerType = argv[++i];
            } else if (arg == "-s" || arg == "--server") {
                if (i + 1 < argc) {
                    connectionString = argv[++i];
                }
            }
        }

        // Create provider using factory
        auto provider = createBackupProvider(providerType, connectionString);
        if (!provider) {
            Logger::error("Failed to create backup provider");
            return 1;
        }

        // Set provider in JobManager
        jobManager->setProvider(provider);

        // Initialize the CLI with JobManager
        BackupCLI cli(jobManager);

        // Run the CLI
        cli.run(argc, argv);
        return 0;
    } catch (const std::exception& e) {
        Logger::error("Error in backup main: " + std::string(e.what()));
        return 1;
    }
} 