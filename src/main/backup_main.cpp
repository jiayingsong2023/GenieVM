#include "main/backup_main.hpp"
#include "backup/backup_cli.hpp"
#include "backup/job_manager.hpp"
#include "backup/vmware/vmware_backup_provider.hpp"
#include "common/vmware_connection.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <memory>

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
              << "  -s, --server         vCenter server address\n"
              << "  -u, --username       vCenter username\n"
              << "  -p, --password       vCenter password\n"
              << "  -i, --incremental    Enable incremental backup\n"
              << "  --schedule           Schedule time (HH:MM)\n"
              << "  --interval           Interval in minutes\n"
              << "  --parallel           Number of parallel disk operations\n"
              << "  --compression        Compression level (0-9)\n"
              << "  --retention          Retention period in days\n"
              << "  --max-backups        Maximum number of backups to keep\n"
              << "  --disable-cbt        Disable Changed Block Tracking\n"
              << "  --exclude-disk       Exclude disk from backup\n";
}

int backupMain(int argc, char** argv) {
    try {
        // Create required dependencies
        auto jobManager = std::make_shared<JobManager>();
        auto connection = std::make_shared<VMwareConnection>();
        auto provider = std::make_shared<VMwareBackupProvider>(connection);

        // Initialize the CLI with dependencies
        BackupCLI cli(jobManager, provider);

        // Run the CLI
        cli.run(argc, argv);
        return 0;
    } catch (const std::exception& e) {
        Logger::error("Error in backup main: " + std::string(e.what()));
        return 1;
    }
} 