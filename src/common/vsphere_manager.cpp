#include "common/vsphere_manager.hpp"
#include <sstream>
#include <soapH.h>
#include <ctime>
#include <iostream>

namespace vmware {

VSphereManager::VSphereManager(const std::string& host,
                             const std::string& username,
                             const std::string& password)
    : host_(host)
    , username_(username)
    , password_(password)
    , connected_(false)
{
    restClient_ = std::make_unique<VSphereRestClient>(host, username, password);
}

VSphereManager::~VSphereManager() {
    disconnect();
}

bool VSphereManager::connect() {
    if (restClient_->connect()) {
        connected_ = true;
        return true;
    }
    return false;
}

void VSphereManager::disconnect() {
    if (connected_) {
        restClient_->disconnect();
        connected_ = false;
    }
}

bool VSphereManager::createVM(const std::string& vmName,
                            const std::string& datastoreName,
                            const std::string& resourcePoolName) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }
    return restClient_->createVM(vmName, datastoreName, resourcePoolName);
}

bool VSphereManager::attachDisks(const std::string& vmName,
                               const std::vector<std::string>& diskPaths) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }
    return restClient_->attachDisks(vmName, diskPaths);
}

bool VSphereManager::getVM(const std::string& vmName, std::string& vmId) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }
    return restClient_->getVM(vmName, vmId);
}

bool VSphereManager::getDatastore(const std::string& datastoreName, std::string& datastoreId) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }
    return restClient_->getDatastore(datastoreName, datastoreId);
}

bool VSphereManager::getResourcePool(const std::string& poolName, std::string& poolId) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }
    return restClient_->getResourcePool(poolName, poolId);
}

bool VSphereManager::initializeVimProxy() {
    try {
        vimProxy_ = std::make_unique<vim25::VimBindingProxy>();
        std::string url = "https://" + host_ + "/sdk";
        vimProxy_->soap_endpoint = url.c_str();
        vimProxy_->soap_ssl_verifypeer = false; // For development only
        vimProxy_->soap_ssl_verifyhost = false; // For development only
        return true;
    } catch (const std::exception& e) {
        Logger::error("Failed to initialize vSphere proxy: " + std::string(e.what()));
        return false;
    }
}

bool VSphereManager::login() {
    try {
        vim25::ServiceContent serviceContent = vimProxy_->RetrieveServiceContent(
            vim25::ManagedObjectReference("ServiceInstance", "ServiceInstance")
        );
        serviceInstance_ = serviceContent.rootFolder;

        vim25::UserSession session = vimProxy_->Login(
            serviceContent.sessionManager,
            username_,
            password_,
            nullptr
        );

        return true;
    } catch (const std::exception& e) {
        Logger::error("Failed to login to vCenter: " + std::string(e.what()));
        return false;
    }
}

void VSphereManager::logout() {
    try {
        if (vimProxy_) {
            vimProxy_->Logout(serviceInstance_);
        }
    } catch (const std::exception& e) {
        Logger::error("Error during logout: " + std::string(e.what()));
    }
}

bool VSphereManager::createVirtualMachineConfigSpec(
    const std::string& vmName,
    const vim25::ManagedObjectReference& datastoreRef,
    const vim25::ManagedObjectReference& resourcePoolRef,
    int numCPUs,
    int memoryMB,
    vim25::VirtualMachineConfigSpec& configSpec) {
    
    try {
        // Set basic VM configuration
        configSpec.name = vmName;
        configSpec.guestId = "otherGuest";
        configSpec.files = vim25::VirtualMachineFileInfo();
        configSpec.files->vmPathName = "[" + datastoreRef.value + "] " + vmName;

        // Set CPU and memory
        configSpec.numCPUs = numCPUs;
        configSpec.memoryMB = memoryMB;

        // Set default device configuration
        vim25::VirtualDeviceConfigSpec controllerSpec;
        controllerSpec.operation = "add";
        controllerSpec.device = vim25::VirtualLsiLogicController();
        controllerSpec.device->key = -1;
        controllerSpec.device->busNumber = 0;
        controllerSpec.device->sharedBus = "noSharing";
        configSpec.deviceChange.push_back(controllerSpec);

        return true;
    } catch (const std::exception& e) {
        Logger::error("Failed to create VM config spec: " + std::string(e.what()));
        return false;
    }
}

bool VSphereManager::createVirtualDiskConfigSpec(
    const std::string& diskPath,
    vim25::VirtualDeviceConfigSpec& diskSpec) {
    
    try {
        // Create virtual disk device using smart pointer
        auto disk = std::make_unique<vim25::VirtualDisk>();
        disk->key = -1;
        disk->unitNumber = 0;
        disk->controllerKey = -1;
        disk->capacityInKB = 0; // Will be set from actual disk
        disk->backing = vim25::VirtualDiskFlatVer2BackingInfo();
        disk->backing->fileName = diskPath;
        disk->backing->diskMode = "persistent";
        disk->backing->thinProvisioned = true;

        // Create device config spec
        diskSpec.operation = "add";
        diskSpec.device = disk.release(); // Transfer ownership to vSphere API
        diskSpec.fileOperation = "create";

        return true;
    } catch (const std::exception& e) {
        Logger::error("Failed to create disk config spec: " + std::string(e.what()));
        return false;
    }
}

void VSphereManager::logError(const std::string& operation) {
    Logger::error(operation);
}

} // namespace vmware 