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
    void logout();

    // VM Operations
    bool getVMInfo(const std::string& vmId, nlohmann::json& vmInfo);
    bool getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths);
    bool enableCBT(const std::string& vmId);
    bool disableCBT(const std::string& vmId);
    bool createSnapshot(const std::string& vmId, const std::string& snapshotName, const std::string& description);
    bool removeSnapshot(const std::string& vmId, const std::string& snapshotName);

    // VM Information
    bool getVMPowerState(const std::string& vmId, std::string& state);
    bool getVMDisks(const std::string& vmId, std::vector<std::string>& diskPaths);
    bool getVMNetworks(const std::string& vmId, std::vector<std::string>& networks);
    
    // Snapshot Operations
    bool revertToSnapshot(const std::string& vmId, const std::string& snapshotId);
    bool getSnapshots(const std::string& vmId, nlohmann::json& snapshots);
    
    // Resource Operations
    bool getDatastores(std::vector<std::string>& datastores);
    bool getNetworks(std::vector<std::string>& networks);
    bool getResourcePools(std::vector<std::string>& resourcePools);
    bool getHosts(std::vector<std::string>& hosts);
    
    // Backup Operations
    bool prepareVMForBackup(const std::string& vmId, bool quiesce = true);
    bool cleanupVMAfterBackup(const std::string& vmId);
    bool getVMDiskInfo(const std::string& vmId, const std::string& diskPath, nlohmann::json& info);

private:
    // Helper methods
    bool makeRequest(const std::string& method, const std::string& endpoint, 
                    const nlohmann::json& body, nlohmann::json& response);
    bool makeRequest(const std::string& method, const std::string& endpoint, 
                    nlohmann::json& response);
    std::string buildUrl(const std::string& endpoint);
    void setCommonHeaders(CURL* curl);
    static size_t writeCallback(void* contents, size_t size, size_t nmemb, std::string* userp);

    std::string host_;
    std::string username_;
    std::string password_;
    std::string sessionId_;
    CURL* curl_;
    bool isLoggedIn_;
    
    // Error handling
    void handleError(const std::string& operation, const nlohmann::json& response);
    bool checkResponse(const nlohmann::json& response) const;
}; 