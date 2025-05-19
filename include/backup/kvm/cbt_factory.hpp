#pragma once

#include <memory>
#include <string>
#include <vector>
#include "backup/backup_provider.hpp"
#include "backup/kvm/cbt.hpp"
#include "common/vmware_connection.hpp"

class CBTFactory {
public:
    static std::unique_ptr<CBT> createCBT(const std::string& diskPath);
    static bool isQCOW2Disk(const std::string& diskPath);
    static bool isLVMDisk(const std::string& diskPath);
    static std::shared_ptr<BackupProvider> createProvider(const std::string& type);
    static bool isCBTEnabled(const std::string& vmId);
    static bool enableCBT(const std::string& vmId);
    static bool disableCBT(const std::string& vmId);
    static bool getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                               std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks);
    static std::shared_ptr<VMwareConnection> createConnection();
}; 