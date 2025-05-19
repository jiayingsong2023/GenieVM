#pragma once

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <vixDiskLib.h>
#include <nlohmann/json.hpp>

// Forward declaration
class VSphereRestClient;

class VMwareConnection {
public:
    VMwareConnection();
    VMwareConnection(const std::string& host, const std::string& username, const std::string& password);
    ~VMwareConnection();

    bool connect(const std::string& host, const std::string& username, const std::string& password);
    void disconnect();
    bool isConnected() const;
    std::string getLastError() const;

    // VDDK operations
    VixDiskLibConnection getVDDKConnection() const;
    void disconnectFromDisk();
    bool initializeVDDK();
    void cleanupVDDK();

    // VM operations
    std::vector<std::string> listVMs() const;
    bool getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths) const;
    bool getVMInfo(const std::string& vmId, std::string& name, std::string& status) const;
    bool createVM(const nlohmann::json& vmConfig, nlohmann::json& response);
    bool attachDisk(const std::string& vmId, const nlohmann::json& diskConfig, nlohmann::json& response);
    bool powerOnVM(const std::string& vmId);

    // CBT operations
    bool getCBTInfo(const std::string& vmId, bool& enabled, std::string& changeId) const;
    bool enableCBT(const std::string& vmId);
    bool disableCBT(const std::string& vmId);
    bool isCBTEnabled(const std::string& vmId) const;
    bool getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                         std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) const;

    // Backup operations
    bool getBackup(const std::string& backupId, nlohmann::json& backupInfo);
    bool verifyBackup(const std::string& backupId, nlohmann::json& response);

private:
    std::string host_;
    std::string username_;
    std::string password_;
    bool connected_;
    VixDiskLibConnection vddkConnection_;
    std::string lastError_;
    std::unique_ptr<VSphereRestClient> restClient_;
}; 