#pragma once

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include <vixDiskLib.h>

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

    // CBT operations
    bool getCBTInfo(const std::string& vmId, bool& enabled, std::string& changeId) const;
    bool enableCBT(const std::string& vmId);
    bool disableCBT(const std::string& vmId);
    bool isCBTEnabled(const std::string& vmId) const;
    bool getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                         std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) const;

private:
    std::string host_;
    std::string username_;
    std::string password_;
    bool connected_;
    VixDiskLibConnection vddkConnection_;
    std::string lastError_;
    std::unique_ptr<VSphereRestClient> restClient_;
}; 