#pragma once

#include <string>
#include <vixDiskLib.h>
#include <vixDiskLibCommon.h>

namespace vmware {

class VMwareConnection {
public:
    VMwareConnection(const std::string& host,
                    const std::string& username,
                    const std::string& password);
    ~VMwareConnection();

    // Connect to vCenter
    bool connect();

    // Disconnect from vCenter
    void disconnect();

    // Get VM handle by name
    bool getVMHandle(const std::string& vmName, VixHandle& vmHandle);

    // Get disk paths for a VM
    bool getVMDiskPaths(const std::string& vmName,
                       std::vector<std::string>& diskPaths);

    // Enable/Disable CBT for a VM
    bool enableCBT(const std::string& vmName);
    bool disableCBT(const std::string& vmName);

    // Get CBT information for a VM
    bool getCBTInfo(const std::string& vmName,
                    VixDiskLibBlockList& blockList);

    // Create a snapshot of a VM
    bool createSnapshot(const std::string& vmName,
                       const std::string& snapshotName,
                       const std::string& description = "");

    // Remove a snapshot of a VM
    bool removeSnapshot(const std::string& vmName,
                       const std::string& snapshotName);

private:
    std::string host_;
    std::string username_;
    std::string password_;
    VixHandle hostHandle_;
    bool connected_;

    // Helper methods
    bool initializeVDDK();
    void cleanupVDDK();
    void logError(const std::string& operation);
}; 