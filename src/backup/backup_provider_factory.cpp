#include "backup/backup_provider_factory.hpp"
#include "common/logger.hpp"
#include <sstream>

BackupProvider* createBackupProvider(const std::string& type, 
                const std::string& host, 
                const std::string& port,
                const std::string& username, 
                const std::string& password) 
{
    Logger::info("Creating backup provider of type: " + type);
    Logger::debug("Connection string: " + host + ":" + port + ":" + username + ":" + password);

    if (type == "vmware") {
        Logger::info("Initializing VMware backup provider");

        if (host.empty() || username.empty() || password.empty()) {
            Logger::error("Invalid connection string format. Expected: host:port:username:password");
            throw std::runtime_error("Invalid connection string format. Expected: host:port:username:password");
        }

        // Create and initialize connection
        Logger::info("Creating VMware connection");
        auto connection = new VMwareConnection();
        if (!connection->connect(host, username, password)) {
            Logger::error("Failed to connect to vCenter: " + connection->getLastError());
            delete connection;
            throw std::runtime_error("Failed to connect to vCenter: " + connection->getLastError());
        }

        Logger::info("Successfully connected to vCenter");
        // Create provider with the connection
        Logger::info("Creating VMware backup provider instance");
        return new VMwareBackupProvider(connection);
    } else if (type == "kvm") {
        Logger::info("Initializing KVM backup provider");
        auto provider = new KVMBackupProvider();
        if (host.empty() || username.empty()) {
            Logger::error("Invalid connection string format. Expected: host:port:username:password");
            throw std::runtime_error("Invalid connection string format. Expected: host:port:username:password");
        }

        provider->connect(host, username, password);    
        return provider;
    } else {
        Logger::error("Unsupported backup provider type: " + type);
        throw std::runtime_error("Unsupported backup provider type: " + type);
    }
} 