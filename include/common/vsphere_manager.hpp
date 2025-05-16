#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <future>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "common/logger.hpp"
#include "common/vsphere_rest_client.hpp"
#include "common/vsphere_types.hpp"

struct DiskController {
    std::string id;
    std::string type;
    std::string busNumber;
    std::string deviceKey;
    bool shared;
    nlohmann::json additionalInfo;
};

class VSphereManager {
public:
    VSphereManager(const std::string& host, const std::string& username, const std::string& password);
    ~VSphereManager();

    bool connect();
    void disconnect();

    // VM Management
    bool createVM(const std::string& vmName, const std::string& datastoreName, const std::string& resourcePoolName);
    bool cloneVM(const std::string& sourceVmName, const std::string& cloneName, 
                const std::string& datastoreName, const std::string& resourcePoolName);
    bool migrateVM(const std::string& vmName, const std::string& targetHost, const std::string& targetDatastore);
    bool attachDisks(const std::string& vmName, const std::vector<std::string>& diskPaths);
    bool getVM(const std::string& vmName, std::string& vmId);
    std::vector<VirtualMachine> getVirtualMachines();
    VirtualMachine getVirtualMachine(const std::string& vmId);
    bool powerOnVM(const std::string& vmId);
    bool powerOffVM(const std::string& vmId);
    bool suspendVM(const std::string& vmId);
    bool resetVM(const std::string& vmId);

    // Disk Management
    std::vector<VirtualDisk> getVirtualDisks(const std::string& vmId);
    VirtualDisk getVirtualDisk(const std::string& vmId, const std::string& diskId);
    bool createDisk(const std::string& vmName, int64_t sizeKB, const std::string& diskType);
    bool resizeDisk(const std::string& vmName, const std::string& diskName, int64_t newSizeKB);
    bool deleteDisk(const std::string& vmName, const std::string& diskName);
    bool detachDisk(const std::string& vmName, const std::string& diskName);
    bool updateDiskBacking(const std::string& vmName, const std::string& diskName, 
                          const std::string& datastoreName, const std::string& diskFormat);
    std::vector<DiskController> getDiskControllers(const std::string& vmName);
    bool createDiskController(const std::string& vmName, const std::string& controllerType,
                            const std::string& busNumber, bool shared);
    bool deleteDiskController(const std::string& vmName, const std::string& controllerId);

    // Resource Management
    bool getDatastore(const std::string& datastoreName, std::string& datastoreId);
    bool getResourcePool(const std::string& poolName, std::string& poolId);

    // Backup Operations
    bool prepareVMForBackup(const std::string& vmName, bool quiesce);
    bool cleanupVMAfterBackup(const std::string& vmName);
    bool getChangedDiskAreas(const std::string& vmName, const std::string& diskName, 
                            int64_t startOffset, int64_t length, nlohmann::json& response);
    bool getDiskLayout(const std::string& vmName, const std::string& diskName, nlohmann::json& response);
    bool getDiskChainInfo(const std::string& vmName, const std::string& diskName, nlohmann::json& response);
    bool consolidateDisks(const std::string& vmName, const std::string& diskName, nlohmann::json& response);
    bool defragmentDisk(const std::string& vmName, const std::string& diskName, nlohmann::json& response);
    bool shrinkDisk(const std::string& vmName, const std::string& diskName, nlohmann::json& response);
    bool getBackupProgress(const std::string& taskId, nlohmann::json& response);
    bool cancelBackup(const std::string& taskId, nlohmann::json& response);
    bool verifyBackup(const std::string& backupId, nlohmann::json& response);
    bool getBackupHistory(const std::string& vmName, nlohmann::json& response);
    bool getBackupSchedule(const std::string& vmName, nlohmann::json& response);
    bool setBackupSchedule(const std::string& vmName, const nlohmann::json& schedule, nlohmann::json& response);
    bool getBackupRetention(const std::string& vmName, nlohmann::json& response);
    bool setBackupRetention(const std::string& vmName, const nlohmann::json& retention, nlohmann::json& response);

private:
    std::string host_;
    std::string username_;
    std::string password_;
    std::unique_ptr<VSphereRestClient> restClient_;
    bool connected_;
    bool getDiskId(const std::string& vmName, const std::string& diskName, std::string& diskId);
}; 