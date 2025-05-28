#include "main/backup_main.hpp"
#include "main/restore_main.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <string>

void printUsage() {
    std::cout << "Usage: genievm [command] [options]\n"
              << "Commands:\n"
              << "  backup    - Backup operations\n"
              << "  restore   - Restore operations\n"
              << "\n"
              << "Options:\n"
              << "  -h, --help    Show this help message\n"
              << "  -v, --version Show version information\n";
}

int main(int argc, char** argv) {
    std::cout << "Starting GenieVM with " << argc << " arguments:" << std::endl;
    for (int i = 0; i < argc; i++) {
        std::cout << "  argv[" << i << "]: " << argv[i] << std::endl;
    }

    // Check for help flag first, before any initialization
    if (argc > 1 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        std::cout << "Help flag detected, showing usage" << std::endl;
        printUsage();
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
        printUsage();
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
        if (command == "backup") {
            std::cout << "Executing backup command with " << (argc - 1) << " arguments" << std::endl;
            return backupMain(argc - 1, argv + 1);
        } else if (command == "restore") {
            std::cout << "Executing restore command with " << (argc - 1) << " arguments" << std::endl;
            return restoreMain(argc - 1, argv + 1);
        } else {
            std::cerr << "Error: Unknown command: " << command << std::endl;
            if (Logger::isInitialized()) {
                Logger::error("Unknown command: " + command);
            }
            printUsage();
            return 1;
        }
    } catch (const std::exception& e) {
        std::cerr << "Error in main: " << e.what() << std::endl;
        if (Logger::isInitialized()) {
            Logger::error("Error in main: " + std::string(e.what()));
        }
        return 1;
    }
} 