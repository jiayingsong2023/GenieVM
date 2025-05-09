#pragma once

#include <string>
#include <vector>
#include <memory>
#include <vim25/VimBindingProxy.h>
#include "common/logger.hpp"

namespace vmware {

class VSphereManager {
public:
    VSphereManager(const std::string& host,
                  const std::string& username,
                  const std::string& password);
    ~VSphereManager();

    // Connect to vCenter
    bool connect();

    // Disconnect from vCenter
    void disconnect();

    // Create a new VM
    bool createVM(const std::string& vmName,
                 const std::string& datastore,
                 const std::string& resourcePool,
                 int numCPUs = 1,
                 int memoryMB = 1024);

    // Attach disks to a VM
    bool attachDisks(const std::string& vmName,
                    const std::vector<std::string>& diskPaths);

    // Get VM by name
    bool getVM(const std::string& vmName, vim25::ManagedObjectReference& vmRef);

    // Get datastore by name
    bool getDatastore(const std::string& datastoreName,
                     vim25::ManagedObjectReference& datastoreRef);

    // Get resource pool by name
    bool getResourcePool(const std::string& poolName,
                        vim25::ManagedObjectReference& poolRef);

private:
    std::string host_;
    std::string username_;
    std::string password_;
    std::unique_ptr<vim25::VimBindingProxy> vimProxy_;
    vim25::ManagedObjectReference serviceInstance_;
    bool connected_;

    // Helper methods
    bool initializeVimProxy();
    bool login();
    void logout();
    bool createVirtualMachineConfigSpec(
        const std::string& vmName,
        const vim25::ManagedObjectReference& datastoreRef,
        const vim25::ManagedObjectReference& resourcePoolRef,
        int numCPUs,
        int memoryMB,
        vim25::VirtualMachineConfigSpec& configSpec);
    bool createVirtualDiskConfigSpec(
        const std::string& diskPath,
        vim25::VirtualDeviceConfigSpec& diskSpec);
    void logError(const std::string& operation);
}; 