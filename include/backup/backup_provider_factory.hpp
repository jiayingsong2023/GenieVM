#pragma once

#include "backup/backup_provider.hpp"
#include "backup/vmware/vmware_backup_provider.hpp"
#include "backup/kvm/kvm_backup_provider.hpp"
#include <memory>
#include <string>
#include <stdexcept>

inline std::shared_ptr<BackupProvider> createBackupProvider(const std::string& type, const std::string& connectionString) {
    if (type == "vmware") {
        return std::make_shared<VMwareBackupProvider>(connectionString);
    } else if (type == "kvm") {
        auto provider = std::make_shared<KVMBackupProvider>();
        // Parse connection string (format: username@host)
        size_t atPos = connectionString.find('@');
        if (atPos != std::string::npos) {
            std::string username = connectionString.substr(0, atPos);
            std::string host = connectionString.substr(atPos + 1);
            if (!provider->connect(host, username, "")) {
                throw std::runtime_error("Failed to connect to KVM host: " + provider->getLastError());
            }
        } else {
            throw std::runtime_error("Invalid connection string format. Expected: username@host");
        }
        return provider;
    } else {
        throw std::runtime_error("Unsupported backup provider type: " + type);
    }
} 