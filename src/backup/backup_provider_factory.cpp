#include "backup/backup_provider_factory.hpp"
#include "common/logger.hpp"
#include <sstream>

BackupProvider* createBackupProvider(const std::string& type, const std::string& connectionString) {
    Logger::info("Creating backup provider of type: " + type);
    Logger::debug("Connection string: " + connectionString);

    if (type == "vmware") {
        Logger::info("Initializing VMware backup provider");
        // Parse connection string format: "host:port:username:password"
        std::stringstream ss(connectionString);
        std::string host, port, username, password;
        std::getline(ss, host, ':');
        std::getline(ss, port, ':');
        std::getline(ss, username, ':');
        std::getline(ss, password, ':');

        Logger::debug("Parsed connection details - Host: " + host + ", Port: " + port + ", Username: " + username);

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
        // Parse connection string (format: username@host)
        size_t atPos = connectionString.find('@');
        if (atPos != std::string::npos) {
            std::string username = connectionString.substr(0, atPos);
            std::string host = connectionString.substr(atPos + 1);
            Logger::debug("Parsed KVM connection details - Host: " + host + ", Username: " + username);
            
            if (!provider->connect(host, username, "")) {
                Logger::error("Failed to connect to KVM host: " + provider->getLastError());
                delete provider;
                throw std::runtime_error("Failed to connect to KVM host: " + provider->getLastError());
            }
            Logger::info("Successfully connected to KVM host");
        } else {
            Logger::error("Invalid connection string format. Expected: username@host");
            delete provider;
            throw std::runtime_error("Invalid connection string format. Expected: username@host");
        }
        return provider;
    } else {
        Logger::error("Unsupported backup provider type: " + type);
        throw std::runtime_error("Unsupported backup provider type: " + type);
    }
} 