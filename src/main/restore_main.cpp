#include "restore/restore_manager.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <string>
#include <cstdlib>

void printUsage() {
    std::cout << "Usage: vmware-restore [options]\n"
              << "Options:\n"
              << "  --host <vcenter-host>     vCenter host address\n"
              << "  --username <username>     vCenter username\n"
              << "  --password <password>     vCenter password\n"
              << "  --vm-name <vm-name>       Name of the VM to restore\n"
              << "  --backup-dir <directory>  Directory containing backup\n"
              << "  --datastore <datastore>   Target datastore for restore\n"
              << "  --resource-pool <pool>    Target resource pool\n"
              << "  --help                    Show this help message\n";
}

int main(int argc, char* argv[]) {
    std::string host;
    std::string username;
    std::string password;
    std::string vmName;
    std::string backupDir;
    std::string datastore;
    std::string resourcePool;

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
        } else if (arg == "--datastore" && i + 1 < argc) {
            datastore = argv[++i];
        } else if (arg == "--resource-pool" && i + 1 < argc) {
            resourcePool = argv[++i];
        } else if (arg == "--help") {
            printUsage();
            return 0;
        }
    }

    // Validate required parameters
    if (host.empty() || username.empty() || password.empty() ||
        vmName.empty() || backupDir.empty() || datastore.empty() ||
        resourcePool.empty()) {
        std::cerr << "Error: Missing required parameters\n";
        printUsage();
        return 1;
    }

    try {
        // Create and initialize restore manager
        RestoreManager restoreManager(host, username, password);
        if (!restoreManager.initialize()) {
            std::cerr << "Failed to initialize restore manager" << std::endl;
            return 1;
        }

        // Perform restore
        std::cout << "Starting restore of VM: " + vmName << std::endl;
        if (restoreManager.restoreVM(vmName, backupDir, datastore, resourcePool)) {
            std::cout << "Restore completed successfully" << std::endl;
            return 0;
        } else {
            std::cerr << "Restore failed" << std::endl;
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Exception occurred: " << e.what() << std::endl;
        return 1;
    }
} 