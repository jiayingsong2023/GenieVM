#include "backup/backup_cli.hpp"
#include "common/vmware_connection.hpp"
#include "common/logger.hpp"
#include "common/thread_utils.hpp"
#include <iostream>
#include <string>
#include <ctime>
#include <sstream>
#include <iomanip>
#include <cstdlib>

void printBackupUsage() {
    std::cout << "Usage: genievm <command> [options]\n"
              << "Commands:\n"
              << "  backup                     Backup a VM\n"
              << "  schedule                   Schedule a backup\n"
              << "  list                       List backups\n"
              << "  verify                     Verify a backup\n"
              << "\n"
              << "Common Options:\n"
              << "  -h, --help                 Show this help message\n"
              << "  -v, --vm-name <name>       Name of the VM\n"
              << "  -b, --backup-dir <dir>     Directory for backup\n"
              << "  -s, --server <host>        vCenter/ESXi host\n"
              << "  -u, --username <user>      Username for vCenter/ESXi\n"
              << "  -p, --password <pass>      Password for vCenter/ESXi\n"
              << "\n"
              << "Backup Options:\n"
              << "  -i, --incremental          Use incremental backup (CBT)\n"
              << "  --schedule <time>          Schedule backup at specific time (YYYY-MM-DD HH:MM:SS)\n"
              << "  --interval <seconds>       Schedule periodic backup every N seconds\n"
              << "  --parallel <num>           Number of parallel backup tasks (default: 4)\n"
              << "  --compression <level>      Compression level (0-9, default: 0)\n"
              << "  --retention <days>         Number of days to keep backups (default: 7)\n"
              << "  --max-backups <num>        Maximum number of backups to keep (default: 10)\n"
              << "  --disable-cbt              Disable Changed Block Tracking\n"
              << "  --exclude-disk <path>      Exclude disk from backup (can be used multiple times)\n"
              << "\n"
              << "Use 'genievm <command> --help' for more information about a specific command.\n";
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

int backupMain(int argc, char* argv[]) {
    try {
        auto connection = std::make_shared<VMwareConnection>();
        BackupCLI cli(connection);
        cli.run(argc, argv);
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 