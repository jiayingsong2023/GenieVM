#include "common/vsphere_manager.hpp"
#include "common/logger.hpp"
#include <sstream>
#include <soapH.h>
#include <ctime>
#include <iostream>
#include <stdexcept>

VSphereManager::VSphereManager(std::shared_ptr<VMwareConnection> connection)
    : connection_(connection) {
}

VSphereManager::~VSphereManager() {
}

bool VSphereManager::initialize() {
    if (!connection_) {
        Logger::error("No connection provided");
        return false;
    }
    return true;
}

std::vector<VirtualMachine> VSphereManager::getVirtualMachines() {
    if (!connection_) {
        throw std::runtime_error("No connection provided");
    }

    // TODO: Implement VM listing using VMwareConnection
    return {};
}

VirtualMachine VSphereManager::getVirtualMachine(const std::string& vmId) {
    if (!connection_) {
        throw std::runtime_error("No connection provided");
    }

    // TODO: Implement VM retrieval using VMwareConnection
    return VirtualMachine{};
}

bool VSphereManager::powerOnVM(const std::string& vmId) {
    if (!connection_) {
        Logger::error("No connection provided");
        return false;
    }

    // TODO: Implement power on using VMwareConnection
    return true;
}

bool VSphereManager::powerOffVM(const std::string& vmId) {
    if (!connection_) {
        Logger::error("No connection provided");
        return false;
    }

    // TODO: Implement power off using VMwareConnection
    return true;
}

bool VSphereManager::suspendVM(const std::string& vmId) {
    if (!connection_) {
        Logger::error("No connection provided");
        return false;
    }

    // TODO: Implement suspend using VMwareConnection
    return true;
}

bool VSphereManager::resetVM(const std::string& vmId) {
    if (!connection_) {
        Logger::error("No connection provided");
        return false;
    }

    // TODO: Implement reset using VMwareConnection
    return true;
}

std::vector<VirtualDisk> VSphereManager::getVirtualDisks(const std::string& vmId) {
    if (!connection_) {
        throw std::runtime_error("No connection provided");
    }

    // TODO: Implement disk listing using VMwareConnection
    return {};
}

VirtualDisk VSphereManager::getVirtualDisk(const std::string& vmId, const std::string& diskId) {
    if (!connection_) {
        throw std::runtime_error("No connection provided");
    }

    // TODO: Implement disk retrieval using VMwareConnection
    return VirtualDisk{};
}

bool VSphereManager::createVM(const std::string& vmName,
                            const std::string& datastoreName,
                            const std::string& resourcePoolName) {
    if (!connection_) {
        Logger::error("No connection provided");
        return false;
    }
    return connection_->createVM(vmName, datastoreName, resourcePoolName);
}

bool VSphereManager::attachDisks(const std::string& vmName,
                               const std::vector<std::string>& diskPaths) {
    if (!connection_) {
        Logger::error("No connection provided");
        return false;
    }
    return connection_->attachDisks(vmName, diskPaths);
}

bool VSphereManager::getVM(const std::string& vmName, std::string& vmId) {
    if (!connection_) {
        Logger::error("No connection provided");
        return false;
    }
    return connection_->getVM(vmName, vmId);
}

bool VSphereManager::getDatastore(const std::string& datastoreName, std::string& datastoreId) {
    if (!connection_) {
        Logger::error("No connection provided");
        return false;
    }
    return connection_->getDatastore(datastoreName, datastoreId);
}

bool VSphereManager::getResourcePool(const std::string& poolName, std::string& poolId) {
    if (!connection_) {
        Logger::error("No connection provided");
        return false;
    }
    return connection_->getResourcePool(poolName, poolId);
}

bool VSphereManager::initializeVimProxy() {
    try {
        vimProxy_ = std::make_unique<vim25::VimBindingProxy>();
        std::string url = "https://" + connection_->getHost() + "/sdk";
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
            connection_->getUsername(),
            connection_->getPassword(),
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