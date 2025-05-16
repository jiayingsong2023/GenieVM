#include "restore/restore_manager.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <string>
#include <cstdlib>

void printRestoreUsage() {
    std::cout << "Usage: genievm restore [options]\n"
              << "Options:\n"
              << "  -h, --help                 Show this help message\n"
              << "  -v, --vm-name <name>       Name of the VM to restore\n"
              << "  -b, --backup-dir <dir>     Directory containing the backup\n"
              << "  -d, --datastore <name>     Target datastore for restore\n"
              << "  -r, --resource-pool <name> Target resource pool for restore\n"
              << "  -s, --server <host>        vCenter/ESXi host\n"
              << "  -u, --username <user>      Username for vCenter/ESXi\n"
              << "  -p, --password <pass>      Password for vCenter/ESXi\n";
}

int restoreMain(int argc, char* argv[]) {
    std::string vmName;
    std::string backupDir;
    std::string datastore;
    std::string resourcePool;
    std::string host;
    std::string username;
    std::string password;

    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "-h" || arg == "--help") {
            printRestoreUsage();
            return 0;
        } else if (arg == "-v" || arg == "--vm-name") {
            if (i + 1 < argc) vmName = argv[++i];
        } else if (arg == "-b" || arg == "--backup-dir") {
            if (i + 1 < argc) backupDir = argv[++i];
        } else if (arg == "-d" || arg == "--datastore") {
            if (i + 1 < argc) datastore = argv[++i];
        } else if (arg == "-r" || arg == "--resource-pool") {
            if (i + 1 < argc) resourcePool = argv[++i];
        } else if (arg == "-s" || arg == "--server") {
            if (i + 1 < argc) host = argv[++i];
        } else if (arg == "-u" || arg == "--username") {
            if (i + 1 < argc) username = argv[++i];
        } else if (arg == "-p" || arg == "--password") {
            if (i + 1 < argc) password = argv[++i];
        }
    }

    // Validate required parameters
    if (vmName.empty() || backupDir.empty() || datastore.empty() || 
        resourcePool.empty() || host.empty() || username.empty() || password.empty()) {
        std::cerr << "Error: Missing required parameters\n";
        printRestoreUsage();
        return 1;
    }

    try {
        // Create and initialize restore manager
        RestoreManager restoreManager(host, username, password);
        if (!restoreManager.initialize()) {
            std::cerr << "Failed to initialize restore manager\n";
            return 1;
        }

        // Perform restore
        if (!restoreManager.restoreVM(vmName, backupDir, datastore, resourcePool)) {
            std::cerr << "Failed to restore VM\n";
            return 1;
        }

        std::cout << "VM restored successfully\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
} 