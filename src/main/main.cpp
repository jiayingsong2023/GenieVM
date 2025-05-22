#include <iostream>
#include <string>
#include "common/logger.hpp"
#include <filesystem>
#include <cstdio>
#include <CLI/CLI.hpp>

// Forward declarations
int backupMain(int argc, char* argv[]);
int restoreMain(int argc, char* argv[]);

void printMainUsage() {
    std::cout << "Usage: genievm <command> [options]\n"
              << "Commands:\n"
              << "  backup                     Backup a VM\n"
              << "  restore                    Restore a VM\n"
              << "\n"
              << "Use 'genievm <command> --help' for more information about a command.\n";
}

int main(int argc, char* argv[]) {
    // Immediate debug output to stderr
    fprintf(stderr, "Starting GenieVM...\n");
    fflush(stderr);

    // Initialize logger
    fprintf(stderr, "Initializing logger...\n");
    fflush(stderr);
    
    if (!Logger::initialize("/tmp/genievm.log", LogLevel::DEBUG)) {
        fprintf(stderr, "Failed to initialize logger\n");
        fflush(stderr);
        return 1;
    }
    
    fprintf(stderr, "Logger initialized successfully\n");
    fflush(stderr);

    // Parse command line arguments
    fprintf(stderr, "Parsing command line arguments...\n");
    fflush(stderr);
    
    CLI::App app{"GenieVM - VMware vSphere Backup and Restore Tool"};

    if (argc < 2) {
        printMainUsage();
        return 1;
    }

    std::string command = argv[1];
    if (command == "backup") {
        // Skip the command name and pass the rest of the arguments
        return backupMain(argc - 1, argv + 1);
    } else if (command == "restore") {
        // Skip the command name and pass the rest of the arguments
        return restoreMain(argc - 1, argv + 1);
    } else {
        std::cerr << "Error: Unknown command '" << command << "'\n";
        printMainUsage();
        return 1;
    }
} 