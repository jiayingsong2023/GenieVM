#include "main/restore_main.hpp"
#include "common/job_manager.hpp"
#include "common/logger.hpp"
#include <iostream>
#include <string>
#include <memory>
#include <thread>
#include <chrono>

int restoreMain(int argc, char* argv[]) {
    if (argc < 6) {
        std::cerr << "Usage: " << argv[0] << " <vm_name> <backup_dir> <datastore> <resource_pool> <host> [username] [password]" << std::endl;
        return 1;
    }

    std::string vmName = argv[1];
    std::string backupDir = argv[2];
    std::string datastore = argv[3];
    std::string resourcePool = argv[4];
    std::string host = argv[5];
    std::string username = argc > 6 ? argv[6] : "";
    std::string password = argc > 7 ? argv[7] : "";

    try {
        // Create JobManager
        auto jobManager = std::make_shared<JobManager>();
        
        // Initialize and connect
        if (!jobManager->initialize()) {
            Logger::error("Failed to initialize job manager");
            return 1;
        }

        if (!jobManager->connect(host, username, password)) {
            Logger::error("Failed to connect to server");
            return 1;
        }

        // Create restore configuration
        RestoreConfig config;
        config.vmId = vmName;
        config.backupId = backupDir;
        config.datastore = datastore;
        config.resourcePool = resourcePool;

        // Create and start restore job
        auto job = jobManager->createRestoreJob(config);
        if (!job) {
            Logger::error("Failed to create restore job");
            return 1;
        }

        // Set up progress callback
        job->setProgressCallback([](int progress) {
            Logger::info("Restore progress: " + std::to_string(progress) + "%");
        });

        // Set up status callback
        job->setStatusCallback([](const std::string& status) {
            Logger::info("Restore status: " + status);
        });

        // Start the restore job
        if (!job->start()) {
            Logger::error("Failed to start restore job");
            return 1;
        }

        // Wait for job completion
        while (job->getState() == Job::State::RUNNING) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        // Check final status
        if (job->getState() == Job::State::COMPLETED) {
            Logger::info("Restore completed successfully");
            return 0;
        } else {
            Logger::error("Restore failed: " + job->getError());
            return 1;
        }

    } catch (const std::exception& e) {
        Logger::error("Error during restore: " + std::string(e.what()));
        return 1;
    }
} 