#pragma once

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <nlohmann/json.hpp>
#include <mutex>
#include "vddk_wrapper/vddk_wrapper.h"

// Forward declaration
class VSphereRestClient;

// VDDK version constants
#define VIXDISKLIB_VERSION_MAJOR 8
#define VIXDISKLIB_VERSION_MINOR 0

class VMwareConnection {
public:
    VMwareConnection();
    VMwareConnection(const std::string& host, const std::string& username, const std::string& password);
    ~VMwareConnection();

    // Connect to vCenter/ESXi
    bool connect(const std::string& host,
                const std::string& username,
                const std::string& password);

    // Disconnect and cleanup
    void disconnect();

    // Get VDDK connection handle
    VDDKConnection getVDDKConnection() const;

    // Get server info
    std::string getServer() const { return server_; }
    std::string getUsername() const { return username_; }
    std::string getThumbprint() const { return thumbprint_; }

    // Check if connected
    bool isConnected() const { return connected_; }

    // Get last error message
    std::string getLastError() const { return lastError_; }

    // Reference counting for active operations
    void incrementRefCount();
    void decrementRefCount();

    // Add method to get REST client
    VSphereRestClient* getRestClient() const { return restClient_; }

    // VDDK operations
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

    // VDDK specific functions
    bool vddkInitialize(const std::string& vmId);

private:
    std::string server_;
    std::string username_;
    std::string password_;
    std::string thumbprint_;
    std::string lastError_;  // Store last error message
    bool connected_;
    bool initialized_;
    mutable std::mutex mutex_;
    VDDKConnection vddkConnection_;
    VSphereRestClient* restClient_;
    int refCount_;  // Track number of active operations
}; 