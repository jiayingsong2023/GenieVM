#pragma once

#include <string>
#include <vector>
#include <memory>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "common/logger.hpp"

class VSphereRestClient {
public:
    VSphereRestClient(const std::string& host, const std::string& username, const std::string& password);
    ~VSphereRestClient();

    // Authentication
    bool login();
    bool logout();
    bool isLoggedIn() const;
    std::string getLastError() const;

    // VM Operations
    bool getVMInfo(const std::string& vmId, nlohmann::json& vmInfo);
    bool getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths);
    bool getVMDiskInfo(const std::string& vmId, const std::string& diskPath, nlohmann::json& diskInfo);
    bool enableCBT(const std::string& vmId);
    bool disableCBT(const std::string& vmId);
    bool getVMPowerState(const std::string& vmId, std::string& powerState);
    bool powerOnVM(const std::string& vmId);
    bool powerOffVM(const std::string& vmId);
    bool suspendVM(const std::string& vmId);
    bool resetVM(const std::string& vmId);
    bool shutdownVM(const std::string& vmId);
    bool rebootVM(const std::string& vmId);
    bool createVM(const nlohmann::json& vmConfig, nlohmann::json& response);
    bool attachDisk(const std::string& vmId, const nlohmann::json& diskConfig, nlohmann::json& response);
    bool listVMs(nlohmann::json& response);
    bool cloneVM(const std::string& sourceVmId, const nlohmann::json& cloneConfig, nlohmann::json& response);
    bool migrateVM(const std::string& vmId, const nlohmann::json& migrateConfig, nlohmann::json& response);

    // Disk Operations
    bool createDisk(const std::string& vmId, const nlohmann::json& diskConfig, nlohmann::json& response);
    bool resizeDisk(const std::string& vmId, const std::string& diskId, int64_t newSizeKB, nlohmann::json& response);
    bool deleteDisk(const std::string& vmId, const std::string& diskId, nlohmann::json& response);
    bool detachDisk(const std::string& vmId, const std::string& diskId, nlohmann::json& response);
    bool updateDiskBacking(const std::string& vmId, const std::string& diskId, const nlohmann::json& backingConfig, nlohmann::json& response);
    bool getDiskControllers(const std::string& vmId, nlohmann::json& response);
    bool createDiskController(const std::string& vmId, const nlohmann::json& controllerConfig, nlohmann::json& response);
    bool deleteDiskController(const std::string& vmId, const std::string& controllerId, nlohmann::json& response);

    // Snapshot Operations
    bool createSnapshot(const std::string& vmId, const std::string& name, const std::string& description);
    bool removeSnapshot(const std::string& vmId, const std::string& snapshotId);
    bool revertToSnapshot(const std::string& vmId, const std::string& snapshotId);
    bool getSnapshots(const std::string& vmId, nlohmann::json& snapshots);

    // Resource Operations
    bool getVMNetworks(const std::string& vmId, std::vector<std::string>& networks);
    bool getDatastores(std::vector<std::string>& datastores);
    bool getNetworks(std::vector<std::string>& networks);
    bool getResourcePools(std::vector<std::string>& resourcePools);
    bool getHosts(std::vector<std::string>& hosts);

    // Backup Operations
    bool prepareVMForBackup(const std::string& vmId, bool quiesce);
    bool cleanupVMAfterBackup(const std::string& vmId);
    bool getChangedDiskAreas(const std::string& vmId, const std::string& diskId, 
                            int64_t startOffset, int64_t length, nlohmann::json& response);
    bool getDiskLayout(const std::string& vmId, const std::string& diskId, nlohmann::json& response);
    bool getDiskChainInfo(const std::string& vmId, const std::string& diskId, nlohmann::json& response);
    bool consolidateDisks(const std::string& vmId, const std::string& diskId, nlohmann::json& response);
    bool defragmentDisk(const std::string& vmId, const std::string& diskId, nlohmann::json& response);
    bool shrinkDisk(const std::string& vmId, const std::string& diskId, nlohmann::json& response);
    bool getBackupProgress(const std::string& taskId, nlohmann::json& response);
    bool cancelBackup(const std::string& taskId, nlohmann::json& response);
    bool verifyBackup(const std::string& backupId, nlohmann::json& response);
    bool getBackupHistory(const std::string& vmId, nlohmann::json& response);
    bool getBackupSchedule(const std::string& vmId, nlohmann::json& response);
    bool setBackupSchedule(const std::string& vmId, const nlohmann::json& schedule, nlohmann::json& response);
    bool getBackupRetention(const std::string& vmId, nlohmann::json& response);
    bool setBackupRetention(const std::string& vmId, const nlohmann::json& retention, nlohmann::json& response);
    bool getBackup(const std::string& backupId, std::string& response);

private:
    // Helper methods
    bool makeRequest(const std::string& method, const std::string& endpoint, 
                    const nlohmann::json& data, nlohmann::json& response);
    std::string buildUrl(const std::string& endpoint) const;
    void setCommonHeaders(struct curl_slist*& headers);
    bool checkResponse(const nlohmann::json& response) const;
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* userp);
    void handleError(const std::string& operation, const nlohmann::json& response);

    std::string host_;
    std::string username_;
    std::string password_;
    std::string sessionId_;
    CURL* curl_;
    bool isLoggedIn_;
    std::string lastError_;
}; 