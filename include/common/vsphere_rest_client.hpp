#pragma once

#include <string>
#include <vector>
#include <memory>
#include <curl/curl.h>
#include <nlohmann/json.hpp>
#include "common/logger.hpp"

namespace vmware {

class VSphereRestClient {
public:
    VSphereRestClient(const std::string& host, const std::string& username, const std::string& password);
    ~VSphereRestClient();

    // Authentication
    bool connect();
    void disconnect();
    bool isConnected() const { return connected_; }

    // VM Operations
    bool powerOnVM(const std::string& vmId);
    bool powerOffVM(const std::string& vmId);
    bool suspendVM(const std::string& vmId);
    bool resetVM(const std::string& vmId);
    bool shutdownVM(const std::string& vmId);
    bool rebootVM(const std::string& vmId);
    
    // VM Information
    bool getVMInfo(const std::string& vmId, nlohmann::json& info);
    bool getVMPowerState(const std::string& vmId, std::string& state);
    bool getVMDisks(const std::string& vmId, std::vector<std::string>& diskPaths);
    bool getVMNetworks(const std::string& vmId, std::vector<std::string>& networks);
    
    // Snapshot Operations
    bool createSnapshot(const std::string& vmId, const std::string& name, const std::string& description);
    bool removeSnapshot(const std::string& vmId, const std::string& snapshotId);
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
    bool getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths);
    bool getVMDiskInfo(const std::string& vmId, const std::string& diskPath, nlohmann::json& info);

private:
    struct CURLDeleter {
        void operator()(CURL* curl) { curl_easy_cleanup(curl); }
    };

    std::string host_;
    std::string username_;
    std::string password_;
    std::string sessionId_;
    bool connected_;
    std::unique_ptr<CURL, CURLDeleter> curl_;
    
    // HTTP request helpers
    bool makeRequest(const std::string& method, const std::string& path, 
                    const nlohmann::json& body, nlohmann::json& response);
    bool makeRequest(const std::string& method, const std::string& path, 
                    nlohmann::json& response);
    
    // Authentication helpers
    bool authenticate();
    void clearSession();
    
    // URL helpers
    std::string getBaseUrl() const;
    std::string getAuthHeader() const;
    
    // Error handling
    void handleError(const std::string& operation, const nlohmann::json& response);
    bool checkResponse(const nlohmann::json& response) const;
};

} // namespace vmware 