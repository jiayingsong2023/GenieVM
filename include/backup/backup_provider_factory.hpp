#pragma once

#include "backup/backup_provider.hpp"
#include "backup/vmware/vmware_backup_provider.hpp"
#include "backup/kvm/kvm_backup_provider.hpp"
#include <memory>
#include <string>
#include <stdexcept>

// Factory function to create appropriate backup provider
BackupProvider* createBackupProvider(const std::string& type, 
        const std::string& host, 
        const std::string& port,
        const std::string& username, 
        const std::string& password); 