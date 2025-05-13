#include "common/vsphere_manager.hpp"
#include <sstream>
#include <soapH.h>
#include <ctime>

namespace vmware {

VSphereManager::VSphereManager(const std::string& host,
                             const std::string& username,
                             const std::string& password)
    : host_(host)
    , username_(username)
    , password_(password)
    , connected_(false)
{
}

VSphereManager::~VSphereManager() {
    if (connected_) {
        disconnect();
    }
}

bool VSphereManager::connect() {
    if (!initializeVimProxy()) {
        return false;
    }

    if (!login()) {
        return false;
    }

    connected_ = true;
    return true;
}

void VSphereManager::disconnect() {
    if (connected_) {
        logout();
        connected_ = false;
    }
}

bool VSphereManager::createVM(const std::string& vmName,
                            const std::string& datastore,
                            const std::string& resourcePool,
                            int numCPUs,
                            int memoryMB) {
    if (!connected_) {
        Logger::error("Not connected to vCenter");
        return false;
    }

    // Get datastore reference
    vim25::ManagedObjectReference datastoreRef;
    if (!getDatastore(datastore, datastoreRef)) {
        return false;
    }

    // Get resource pool reference
    vim25::ManagedObjectReference poolRef;
    if (!getResourcePool(resourcePool, poolRef)) {
        return false;
    }

    // Create VM configuration
    vim25::VirtualMachineConfigSpec configSpec;
    if (!createVirtualMachineConfigSpec(vmName, datastoreRef, poolRef,
                                      numCPUs, memoryMB, configSpec)) {
        return false;
    }

    try {
        // Create the VM
        vim25::ManagedObjectReference vmRef = vimProxy_->CreateVM_Task(
            serviceInstance_,
            configSpec,
            poolRef,
            nullptr
        );

        // Wait for the task to complete
        vim25::TaskInfo taskInfo;
        time_t startTime = std::time(nullptr);
        while (true) {
            taskInfo = vimProxy_->ReadTask(vmRef);
            if (taskInfo.state == "success") {
                break;
            } else if (taskInfo.state == "error") {
                Logger::error("Failed to create VM: " + taskInfo.error->fault->faultString);
                return false;
            }
            // Check if we've been waiting too long (e.g., 5 minutes)
            if (std::time(nullptr) - startTime > 300) {
                Logger::error("Timeout waiting for VM creation");
                return false;
            }
            // Small delay to prevent CPU spinning
            for (volatile int i = 0; i < 1000000; ++i) {}
        }

        Logger::info("Successfully created VM: " + vmName);
        return true;
    } catch (const std::exception& e) {
        Logger::error("Exception while creating VM: " + std::string(e.what()));
        return false;
    }
}

bool VSphereManager::attachDisks(const std::string& vmName,
                               const std::vector<std::string>& diskPaths) {
    if (!connected_) {
        Logger::error("Not connected to vCenter");
        return false;
    }

    // Get VM reference
    vim25::ManagedObjectReference vmRef;
    if (!getVM(vmName, vmRef)) {
        return false;
    }

    try {
        // Create disk device specs
        std::vector<vim25::VirtualDeviceConfigSpec> deviceSpecs;
        for (const auto& diskPath : diskPaths) {
            vim25::VirtualDeviceConfigSpec diskSpec;
            if (!createVirtualDiskConfigSpec(diskPath, diskSpec)) {
                return false;
            }
            deviceSpecs.push_back(diskSpec);
        }

        // Create VM config spec for disk attachment
        vim25::VirtualMachineConfigSpec configSpec;
        configSpec.deviceChange = deviceSpecs;

        // Reconfigure VM to attach disks
        vim25::ManagedObjectReference taskRef = vimProxy_->ReconfigVM_Task(
            vmRef,
            configSpec
        );

        // Wait for the task to complete
        vim25::TaskInfo taskInfo;
        time_t startTime = std::time(nullptr);
        while (true) {
            taskInfo = vimProxy_->ReadTask(taskRef);
            if (taskInfo.state == "success") {
                break;
            } else if (taskInfo.state == "error") {
                Logger::error("Failed to attach disks: " + taskInfo.error->fault->faultString);
                return false;
            }
            // Check if we've been waiting too long (e.g., 5 minutes)
            if (std::time(nullptr) - startTime > 300) {
                Logger::error("Timeout waiting for disk attachment");
                return false;
            }
            // Small delay to prevent CPU spinning
            for (volatile int i = 0; i < 1000000; ++i) {}
        }

        Logger::info("Successfully attached " + std::to_string(diskPaths.size()) + 
                    " disks to VM: " + vmName);
        return true;
    } catch (const std::exception& e) {
        Logger::error("Exception while attaching disks: " + std::string(e.what()));
        return false;
    }
}

bool VSphereManager::getVM(const std::string& vmName,
                          vim25::ManagedObjectReference& vmRef) {
    try {
        // Create property filter spec
        vim25::PropertyFilterSpec filterSpec;
        vim25::ObjectSpec objectSpec;
        objectSpec.obj = serviceInstance_;
        objectSpec.skip = false;
        filterSpec.objectSet.push_back(objectSpec);

        vim25::PropertySpec propertySpec;
        propertySpec.type = "VirtualMachine";
        propertySpec.all = false;
        propertySpec.pathSet.push_back("name");
        filterSpec.propSet.push_back(propertySpec);

        // Create property collector
        vim25::ManagedObjectReference propCollector = vimProxy_->CreateFilter(
            serviceInstance_,
            filterSpec,
            false
        );

        // Get property collector results
        std::vector<vim25::ObjectContent> results = vimProxy_->WaitForUpdates(
            propCollector,
            ""
        );

        // Clean up property collector
        vimProxy_->DestroyPropertyFilter(propCollector);

        // Find VM by name
        for (const auto& objContent : results) {
            if (objContent.propSet[0].val == vmName) {
                vmRef = objContent.obj;
                return true;
            }
        }

        Logger::error("VM not found: " + vmName);
        return false;
    } catch (const std::exception& e) {
        Logger::error("Exception while getting VM: " + std::string(e.what()));
        return false;
    }
}

bool VSphereManager::getDatastore(const std::string& datastoreName,
                                vim25::ManagedObjectReference& datastoreRef) {
    try {
        // Create property filter spec
        vim25::PropertyFilterSpec filterSpec;
        vim25::ObjectSpec objectSpec;
        objectSpec.obj = serviceInstance_;
        objectSpec.skip = false;
        filterSpec.objectSet.push_back(objectSpec);

        vim25::PropertySpec propertySpec;
        propertySpec.type = "Datastore";
        propertySpec.all = false;
        propertySpec.pathSet.push_back("name");
        filterSpec.propSet.push_back(propertySpec);

        // Create property collector
        vim25::ManagedObjectReference propCollector = vimProxy_->CreateFilter(
            serviceInstance_,
            filterSpec,
            false
        );

        // Get property collector results
        std::vector<vim25::ObjectContent> results = vimProxy_->WaitForUpdates(
            propCollector,
            ""
        );

        // Clean up property collector
        vimProxy_->DestroyPropertyFilter(propCollector);

        // Find datastore by name
        for (const auto& objContent : results) {
            if (objContent.propSet[0].val == datastoreName) {
                datastoreRef = objContent.obj;
                return true;
            }
        }

        Logger::error("Datastore not found: " + datastoreName);
        return false;
    } catch (const std::exception& e) {
        Logger::error("Exception while getting datastore: " + std::string(e.what()));
        return false;
    }
}

bool VSphereManager::getResourcePool(const std::string& poolName,
                                   vim25::ManagedObjectReference& poolRef) {
    try {
        // Create property filter spec
        vim25::PropertyFilterSpec filterSpec;
        vim25::ObjectSpec objectSpec;
        objectSpec.obj = serviceInstance_;
        objectSpec.skip = false;
        filterSpec.objectSet.push_back(objectSpec);

        vim25::PropertySpec propertySpec;
        propertySpec.type = "ResourcePool";
        propertySpec.all = false;
        propertySpec.pathSet.push_back("name");
        filterSpec.propSet.push_back(propertySpec);

        // Create property collector
        vim25::ManagedObjectReference propCollector = vimProxy_->CreateFilter(
            serviceInstance_,
            filterSpec,
            false
        );

        // Get property collector results
        std::vector<vim25::ObjectContent> results = vimProxy_->WaitForUpdates(
            propCollector,
            ""
        );

        // Clean up property collector
        vimProxy_->DestroyPropertyFilter(propCollector);

        // Find resource pool by name
        for (const auto& objContent : results) {
            if (objContent.propSet[0].val == poolName) {
                poolRef = objContent.obj;
                return true;
            }
        }

        Logger::error("Resource pool not found: " + poolName);
        return false;
    } catch (const std::exception& e) {
        Logger::error("Exception while getting resource pool: " + std::string(e.what()));
        return false;
    }
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