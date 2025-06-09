#include "common/vsphere_manager.hpp"
#include "common/logger.hpp"
#include <sstream>
#include <ctime>
#include <iostream>
#include <stdexcept>

VSphereManager::VSphereManager(const std::string& host, const std::string& username, const std::string& password)
    : host_(host), username_(username), password_(), connected_(false) {
    // Store password securely
    password_ = password;
    restClient_ = std::make_unique<VSphereRestClient>(host, username, password);
}

VSphereManager::~VSphereManager() {
    disconnect();
}

bool VSphereManager::connect() {
    if (connected_) {
        return true;
    }

    try {
        connected_ = restClient_->login();
        return connected_;
    } catch (const std::exception& e) {
        Logger::error("Failed to connect to vSphere: " + std::string(e.what()));
        return false;
    }
}

void VSphereManager::disconnect() {
    if (connected_) {
        Logger::debug("Logging out from vSphereManager");
        restClient_->logout();
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

    // Get required resource IDs
    std::string datastoreId, poolId;
    if (!getDatastore(datastoreName, datastoreId) || !getResourcePool(resourcePoolName, poolId)) {
        Logger::error("Failed to get required resources for VM creation");
        return false;
    }

    // Prepare VM creation request
    nlohmann::json vmConfig = {
        {"name", vmName},
        {"datastore_id", datastoreId},
        {"resource_pool_id", poolId},
        {"num_cpus", 1},
        {"memory_mb", 1024},
        {"guest_os", "other3xLinux64Guest"}
    };

    nlohmann::json response;
    if (!restClient_->createVM(vmConfig, response)) {
        Logger::error("Failed to create VM: " + vmName);
        return false;
    }

    Logger::info("Successfully created VM: " + vmName);
    return true;
}

bool VSphereManager::attachDisks(const std::string& vmName,
                               const std::vector<std::string>& diskPaths) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to find VM: " + vmName);
        return false;
    }

    // Attach each disk
    for (const auto& diskPath : diskPaths) {
        nlohmann::json diskConfig = {
            {"path", diskPath},
            {"controller_type", "SCSI"},
            {"unit_number", 0},  // Will be auto-assigned if in use
            {"thin_provisioned", true}
        };

        nlohmann::json response;
        if (!restClient_->attachDisk(vmId, diskConfig, response)) {
            Logger::error("Failed to attach disk: " + diskPath + " to VM: " + vmName);
            return false;
        }
    }

    Logger::info("Successfully attached " + std::to_string(diskPaths.size()) + " disks to VM: " + vmName);
    return true;
}

bool VSphereManager::getVM(const std::string& vmName, std::string& vmId) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    std::vector<VirtualMachine> vms = getVirtualMachines();
    for (const auto& vm : vms) {
        if (vm.name == vmName) {
            vmId = vm.id;
            return true;
        }
    }

    Logger::error("VM not found: " + vmName);
    return false;
}

bool VSphereManager::getDatastore(const std::string& datastoreName, std::string& datastoreId) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    std::vector<std::string> datastores;
    if (restClient_->getDatastores(datastores)) {
        for (const auto& ds : datastores) {
            if (ds == datastoreName) {
                datastoreId = ds;
                return true;
            }
        }
    }
    return false;
}

bool VSphereManager::getResourcePool(const std::string& poolName, std::string& poolId) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    std::vector<std::string> pools;
    if (restClient_->getResourcePools(pools)) {
        for (const auto& pool : pools) {
            if (pool == poolName) {
                poolId = pool;
                return true;
            }
        }
    }
    return false;
}

std::vector<VirtualMachine> VSphereManager::getVirtualMachines() {
    if (!connected_) {
        throw std::runtime_error("Not connected to vSphere");
    }

    std::vector<VirtualMachine> vms;
    nlohmann::json response;
    if (restClient_->listVMs(response)) {
        for (const auto& vmInfo : response["value"]) {
            VirtualMachine vm;
            vm.id = vmInfo["vm"];
            vm.name = vmInfo["name"];
            vm.powerState = vmInfo["power_state"];
            vm.numCPUs = vmInfo["cpu"]["count"];
            vm.memoryMB = vmInfo["memory"]["size_MiB"];
            vm.guestOS = vmInfo["guest_OS"];
            vm.version = vmInfo["version"];
            vm.additionalInfo = vmInfo;
            vms.push_back(vm);
        }
    }

    return vms;
}

VirtualMachine VSphereManager::getVirtualMachine(const std::string& vmId) {
    if (!connected_) {
        throw std::runtime_error("Not connected to vSphere");
    }

    VirtualMachine vm;
    nlohmann::json vmInfo;
    if (restClient_->getVMInfo(vmId, vmInfo)) {
        vm.id = vmId;
        vm.name = vmInfo["name"];
        vm.powerState = vmInfo["power_state"];
        vm.numCPUs = vmInfo["num_cpus"];
        vm.memoryMB = vmInfo["memory_mb"];
        vm.guestOS = vmInfo["guest_os"];
        vm.version = vmInfo["version"];
        vm.additionalInfo = vmInfo;
    }
    return vm;
}

bool VSphereManager::powerOnVM(const std::string& vmId) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    return restClient_->powerOnVM(vmId);
}

bool VSphereManager::powerOffVM(const std::string& vmId) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    return restClient_->powerOffVM(vmId);
}

bool VSphereManager::suspendVM(const std::string& vmId) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    return restClient_->suspendVM(vmId);
}

bool VSphereManager::resetVM(const std::string& vmId) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    return restClient_->resetVM(vmId);
}

std::vector<VirtualDisk> VSphereManager::getVirtualDisks(const std::string& vmId) {
    if (!connected_) {
        throw std::runtime_error("Not connected to vSphere");
    }

    std::vector<VirtualDisk> disks;
    std::vector<std::string> diskPaths;
    if (restClient_->getVMDiskPaths(vmId, diskPaths)) {
        for (const auto& path : diskPaths) {
            nlohmann::json diskInfo;
            if (restClient_->getVMDiskInfo(vmId, path, diskInfo)) {
                VirtualDisk disk;
                disk.id = diskInfo["id"];
                disk.name = diskInfo["name"];
                disk.path = path;
                disk.capacityKB = diskInfo["capacity_kb"];
                disk.diskType = diskInfo["disk_type"];
                disk.thinProvisioned = diskInfo["thin_provisioned"];
                disk.controllerType = diskInfo["controller_type"];
                disk.unitNumber = diskInfo["unit_number"];
                disk.additionalInfo = diskInfo;
                disks.push_back(disk);
            }
        }
    }
    return disks;
}

VirtualDisk VSphereManager::getVirtualDisk(const std::string& vmId, const std::string& diskId) {
    if (!connected_) {
        throw std::runtime_error("Not connected to vSphere");
    }

    VirtualDisk disk;
    std::vector<std::string> diskPaths;
    if (restClient_->getVMDiskPaths(vmId, diskPaths)) {
        for (const auto& path : diskPaths) {
            nlohmann::json diskInfo;
            if (restClient_->getVMDiskInfo(vmId, path, diskInfo) && diskInfo["id"] == diskId) {
                disk.id = diskId;
                disk.name = diskInfo["name"];
                disk.path = path;
                disk.capacityKB = diskInfo["capacity_kb"];
                disk.diskType = diskInfo["disk_type"];
                disk.thinProvisioned = diskInfo["thin_provisioned"];
                disk.controllerType = diskInfo["controller_type"];
                disk.unitNumber = diskInfo["unit_number"];
                disk.additionalInfo = diskInfo;
                break;
            }
        }
    }
    return disk;
}

bool VSphereManager::cloneVM(const std::string& sourceVmName,
                           const std::string& cloneName,
                           const std::string& datastoreName,
                           const std::string& resourcePoolName) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    // Get source VM ID
    std::string sourceVmId;
    if (!getVM(sourceVmName, sourceVmId)) {
        Logger::error("Failed to find source VM: " + sourceVmName);
        return false;
    }

    // Get required resource IDs
    std::string datastoreId, poolId;
    if (!getDatastore(datastoreName, datastoreId) || !getResourcePool(resourcePoolName, poolId)) {
        Logger::error("Failed to get required resources for VM cloning");
        return false;
    }

    // Prepare clone configuration
    nlohmann::json cloneConfig = {
        {"name", cloneName},
        {"datastore_id", datastoreId},
        {"resource_pool_id", poolId},
        {"linked_clone", false},  // Create a full clone
        {"power_on", false}  // Don't power on the clone after creation
    };

    nlohmann::json response;
    if (!restClient_->cloneVM(sourceVmId, cloneConfig, response)) {
        Logger::error("Failed to clone VM: " + sourceVmName);
        return false;
    }

    Logger::info("Successfully cloned VM: " + sourceVmName + " to: " + cloneName);
    return true;
}

bool VSphereManager::migrateVM(const std::string& vmName,
                             const std::string& targetHost,
                             const std::string& targetDatastore) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to find VM: " + vmName);
        return false;
    }

    // Get target host ID
    std::vector<std::string> hosts;
    if (!restClient_->getHosts(hosts)) {
        Logger::error("Failed to get host list");
        return false;
    }

    bool hostFound = false;
    for (const auto& host : hosts) {
        if (host == targetHost) {
            hostFound = true;
            break;
        }
    }

    if (!hostFound) {
        Logger::error("Target host not found: " + targetHost);
        return false;
    }

    // Get target datastore ID
    std::string datastoreId;
    if (!getDatastore(targetDatastore, datastoreId)) {
        Logger::error("Failed to find target datastore: " + targetDatastore);
        return false;
    }

    // Prepare migration configuration
    nlohmann::json migrateConfig = {
        {"target_host", targetHost},
        {"target_datastore", datastoreId},
        {"priority", "highPriority"},  // Options: highPriority, lowPriority
        {"state", "poweredOff"}  // Options: poweredOn, poweredOff, suspended
    };

    nlohmann::json response;
    if (!restClient_->migrateVM(vmId, migrateConfig, response)) {
        Logger::error("Failed to migrate VM: " + vmName);
        return false;
    }

    Logger::info("Successfully initiated migration of VM: " + vmName);
    return true;
}

bool VSphereManager::createDisk(const std::string& vmName, int64_t sizeKB, const std::string& diskType) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to find VM: " + vmName);
        return false;
    }

    // Prepare disk configuration
    nlohmann::json diskConfig = {
        {"size_kb", sizeKB},
        {"type", diskType},
        {"thin_provisioned", diskType == "thin"},
        {"controller_type", "SCSI"},
        {"unit_number", 0}  // Will be auto-assigned if in use
    };

    nlohmann::json response;
    if (!restClient_->createDisk(vmId, diskConfig, response)) {
        Logger::error("Failed to create disk for VM: " + vmName);
        return false;
    }

    Logger::info("Successfully created disk for VM: " + vmName);
    return true;
}

bool VSphereManager::resizeDisk(const std::string& vmName, const std::string& diskName, int64_t newSizeKB) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to find VM: " + vmName);
        return false;
    }

    // Get disk ID
    std::vector<VirtualDisk> disks = getVirtualDisks(vmId);
    std::string diskId;
    for (const auto& disk : disks) {
        if (disk.name == diskName) {
            diskId = disk.id;
            break;
        }
    }

    if (diskId.empty()) {
        Logger::error("Failed to find disk: " + diskName);
        return false;
    }

    nlohmann::json response;
    if (!restClient_->resizeDisk(vmId, diskId, newSizeKB, response)) {
        Logger::error("Failed to resize disk: " + diskName);
        return false;
    }

    Logger::info("Successfully resized disk: " + diskName);
    return true;
}

bool VSphereManager::deleteDisk(const std::string& vmName, const std::string& diskName) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to find VM: " + vmName);
        return false;
    }

    // Get disk ID
    std::vector<VirtualDisk> disks = getVirtualDisks(vmId);
    std::string diskId;
    for (const auto& disk : disks) {
        if (disk.name == diskName) {
            diskId = disk.id;
            break;
        }
    }

    if (diskId.empty()) {
        Logger::error("Failed to find disk: " + diskName);
        return false;
    }

    nlohmann::json response;
    if (!restClient_->deleteDisk(vmId, diskId, response)) {
        Logger::error("Failed to delete disk: " + diskName);
        return false;
    }

    Logger::info("Successfully deleted disk: " + diskName);
    return true;
}

bool VSphereManager::detachDisk(const std::string& vmName, const std::string& diskName) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to find VM: " + vmName);
        return false;
    }

    // Get disk ID
    std::vector<VirtualDisk> disks = getVirtualDisks(vmId);
    std::string diskId;
    for (const auto& disk : disks) {
        if (disk.name == diskName) {
            diskId = disk.id;
            break;
        }
    }

    if (diskId.empty()) {
        Logger::error("Failed to find disk: " + diskName);
        return false;
    }

    nlohmann::json response;
    if (!restClient_->detachDisk(vmId, diskId, response)) {
        Logger::error("Failed to detach disk: " + diskName);
        return false;
    }

    Logger::info("Successfully detached disk: " + diskName);
    return true;
}

bool VSphereManager::updateDiskBacking(const std::string& vmName, const std::string& diskName,
                                     const std::string& backingType, const std::string& backingPath) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to find VM: " + vmName);
        return false;
    }

    // Get disk ID
    std::vector<VirtualDisk> disks = getVirtualDisks(vmId);
    std::string diskId;
    for (const auto& disk : disks) {
        if (disk.name == diskName) {
            diskId = disk.id;
            break;
        }
    }

    if (diskId.empty()) {
        Logger::error("Failed to find disk: " + diskName);
        return false;
    }

    // Prepare backing configuration
    nlohmann::json backingConfig = {
        {"type", backingType},
        {"path", backingPath}
    };

    nlohmann::json response;
    if (!restClient_->updateDiskBacking(vmId, diskId, backingConfig, response)) {
        Logger::error("Failed to update disk backing: " + diskName);
        return false;
    }

    Logger::info("Successfully updated disk backing: " + diskName);
    return true;
}

std::vector<DiskController> VSphereManager::getDiskControllers(const std::string& vmName) {
    std::vector<DiskController> controllers;
    std::string vmId;
    nlohmann::json response;

    if (!getVM(vmName, vmId)) {
        return controllers;
    }

    if (restClient_->getDiskControllers(vmId, response)) {
        for (const auto& controller : response["controllers"]) {
            DiskController ctrl;
            ctrl.id = controller["id"];
            ctrl.type = controller["type"];
            ctrl.busNumber = controller["bus"];
            ctrl.deviceKey = controller["deviceKey"];
            ctrl.shared = controller["sharing"];
            ctrl.additionalInfo = controller["additionalInfo"];
            controllers.push_back(ctrl);
        }
    }

    return controllers;
}

bool VSphereManager::createDiskController(const std::string& vmName, const std::string& controllerType,
                                        const std::string& bus, bool sharing) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to find VM: " + vmName);
        return false;
    }

    // Prepare controller configuration
    nlohmann::json controllerConfig = {
        {"type", controllerType},
        {"bus", bus},
        {"sharing", sharing}
    };

    nlohmann::json response;
    if (!restClient_->createDiskController(vmId, controllerConfig, response)) {
        Logger::error("Failed to create disk controller for VM: " + vmName);
        return false;
    }

    Logger::info("Successfully created disk controller for VM: " + vmName);
    return true;
}

bool VSphereManager::deleteDiskController(const std::string& vmName, const std::string& controllerId) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to find VM: " + vmName);
        return false;
    }

    nlohmann::json response;
    if (!restClient_->deleteDiskController(vmId, controllerId, response)) {
        Logger::error("Failed to delete disk controller: " + controllerId);
        return false;
    }

    Logger::info("Successfully deleted disk controller: " + controllerId);
    return true;
}

bool VSphereManager::prepareVMForBackup(const std::string& vmName, bool quiesce) {
    // Validate inputs
    if (vmName.empty()) {
        Logger::error("Invalid VM name for backup preparation");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to get VM ID for " + vmName);
        return false;
    }

    // Prepare VM for backup
    nlohmann::json response;
    bool success = restClient_->prepareVMForBackup(vmId, quiesce);
    if (success) {
        Logger::info("Successfully prepared VM " + vmName + " for backup");
    }
    return success;
}

bool VSphereManager::cleanupVMAfterBackup(const std::string& vmName) {
    // Validate inputs
    if (vmName.empty()) {
        Logger::error("Invalid VM name for backup cleanup");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to get VM ID for " + vmName);
        return false;
    }

    // Cleanup VM after backup
    nlohmann::json response;
    bool success = restClient_->cleanupVMAfterBackup(vmId);
    if (success) {
        Logger::info("Successfully cleaned up VM " + vmName + " after backup");
    }
    return success;
}

bool VSphereManager::getChangedDiskAreas(const std::string& vmName, const std::string& diskName,
                                       int64_t startOffset, int64_t length, nlohmann::json& response) {
    // Validate inputs
    if (vmName.empty() || diskName.empty() || startOffset < 0 || length <= 0) {
        Logger::error("Invalid input parameters for getting changed disk areas");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to get VM ID for " + vmName);
        return false;
    }

    // Get disk ID
    std::string diskId;
    if (!getDiskId(vmName, diskName, diskId)) {
        Logger::error("Failed to get disk ID for " + diskName);
        return false;
    }

    // Get changed disk areas
    bool success = restClient_->getChangedDiskAreas(vmId, diskId, startOffset, length, response);
    if (success) {
        Logger::info("Successfully retrieved changed areas for disk " + diskName);
    }
    return success;
}

bool VSphereManager::getDiskLayout(const std::string& vmName, const std::string& diskName, nlohmann::json& response) {
    // Validate inputs
    if (vmName.empty() || diskName.empty()) {
        Logger::error("Invalid input parameters for getting disk layout");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to get VM ID for " + vmName);
        return false;
    }

    // Get disk ID
    std::string diskId;
    if (!getDiskId(vmName, diskName, diskId)) {
        Logger::error("Failed to get disk ID for " + diskName);
        return false;
    }

    // Get disk layout
    bool success = restClient_->getDiskLayout(vmId, diskId, response);
    if (success) {
        Logger::info("Successfully retrieved layout for disk " + diskName);
    }
    return success;
}

bool VSphereManager::getDiskChainInfo(const std::string& vmName, const std::string& diskName, nlohmann::json& response) {
    // Validate inputs
    if (vmName.empty() || diskName.empty()) {
        Logger::error("Invalid input parameters for getting disk chain info");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to get VM ID for " + vmName);
        return false;
    }

    // Get disk ID
    std::string diskId;
    if (!getDiskId(vmName, diskName, diskId)) {
        Logger::error("Failed to get disk ID for " + diskName);
        return false;
    }

    // Get disk chain info
    bool success = restClient_->getDiskChainInfo(vmId, diskId, response);
    if (success) {
        Logger::info("Successfully retrieved chain info for disk " + diskName);
    }
    return success;
}

bool VSphereManager::consolidateDisks(const std::string& vmName, const std::string& diskName, nlohmann::json& response) {
    // Validate inputs
    if (vmName.empty() || diskName.empty()) {
        Logger::error("Invalid input parameters for disk consolidation");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to get VM ID for " + vmName);
        return false;
    }

    // Get disk ID
    std::string diskId;
    if (!getDiskId(vmName, diskName, diskId)) {
        Logger::error("Failed to get disk ID for " + diskName);
        return false;
    }

    // Consolidate disks
    bool success = restClient_->consolidateDisks(vmId, diskId, response);
    if (success) {
        Logger::info("Successfully initiated consolidation for disk " + diskName);
    }
    return success;
}

bool VSphereManager::defragmentDisk(const std::string& vmName, const std::string& diskName, nlohmann::json& response) {
    // Validate inputs
    if (vmName.empty() || diskName.empty()) {
        Logger::error("Invalid input parameters for disk defragmentation");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to get VM ID for " + vmName);
        return false;
    }

    // Get disk ID
    std::string diskId;
    if (!getDiskId(vmName, diskName, diskId)) {
        Logger::error("Failed to get disk ID for " + diskName);
        return false;
    }

    // Defragment disk
    bool success = restClient_->defragmentDisk(vmId, diskId, response);
    if (success) {
        Logger::info("Successfully initiated defragmentation for disk " + diskName);
    }
    return success;
}

bool VSphereManager::shrinkDisk(const std::string& vmName, const std::string& diskName, nlohmann::json& response) {
    // Validate inputs
    if (vmName.empty() || diskName.empty()) {
        Logger::error("Invalid input parameters for disk shrinking");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to get VM ID for " + vmName);
        return false;
    }

    // Get disk ID
    std::string diskId;
    if (!getDiskId(vmName, diskName, diskId)) {
        Logger::error("Failed to get disk ID for " + diskName);
        return false;
    }

    // Shrink disk
    bool success = restClient_->shrinkDisk(vmId, diskId, response);
    if (success) {
        Logger::info("Successfully initiated shrinking for disk " + diskName);
    }
    return success;
}

bool VSphereManager::getBackupProgress(const std::string& taskId, nlohmann::json& response) {
    // Validate inputs
    if (taskId.empty()) {
        Logger::error("Invalid task ID for getting backup progress");
        return false;
    }

    // Get backup progress
    bool success = restClient_->getBackupProgress(taskId, response);
    if (success) {
        Logger::info("Successfully retrieved progress for backup task " + taskId);
    }
    return success;
}

bool VSphereManager::cancelBackup(const std::string& taskId, nlohmann::json& response) {
    // Validate inputs
    if (taskId.empty()) {
        Logger::error("Invalid task ID for canceling backup");
        return false;
    }

    // Cancel backup
    bool success = restClient_->cancelBackup(taskId, response);
    if (success) {
        Logger::info("Successfully canceled backup task " + taskId);
    }
    return success;
}

bool VSphereManager::verifyBackup(const std::string& backupId, nlohmann::json& response) {
    // Validate inputs
    if (backupId.empty()) {
        Logger::error("Invalid backup ID for verification");
        return false;
    }

    // Verify backup
    bool success = restClient_->verifyBackup(backupId, response);
    if (success) {
        Logger::info("Successfully initiated verification for backup " + backupId);
    }
    return success;
}

bool VSphereManager::getBackupHistory(const std::string& vmName, nlohmann::json& response) {
    // Validate inputs
    if (vmName.empty()) {
        Logger::error("Invalid VM name for getting backup history");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to get VM ID for " + vmName);
        return false;
    }

    // Get backup history
    bool success = restClient_->getBackupHistory(vmId, response);
    if (success) {
        Logger::info("Successfully retrieved backup history for VM " + vmName);
    }
    return success;
}

bool VSphereManager::getBackupSchedule(const std::string& vmName, nlohmann::json& response) {
    // Validate inputs
    if (vmName.empty()) {
        Logger::error("Invalid VM name for getting backup schedule");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to get VM ID for " + vmName);
        return false;
    }

    // Get backup schedule
    bool success = restClient_->getBackupSchedule(vmId, response);
    if (success) {
        Logger::info("Successfully retrieved backup schedule for VM " + vmName);
    }
    return success;
}

bool VSphereManager::setBackupSchedule(const std::string& vmName, const nlohmann::json& schedule, nlohmann::json& response) {
    // Validate inputs
    if (vmName.empty()) {
        Logger::error("Invalid VM name for setting backup schedule");
        return false;
    }

    // Validate schedule configuration
    if (!schedule.contains("frequency") || !schedule.contains("time")) {
        Logger::error("Missing required fields in schedule configuration");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to get VM ID for " + vmName);
        return false;
    }

    // Set backup schedule
    bool success = restClient_->setBackupSchedule(vmId, schedule, response);
    if (success) {
        Logger::info("Successfully set backup schedule for VM " + vmName);
    }
    return success;
}

bool VSphereManager::getBackupRetention(const std::string& vmName, nlohmann::json& response) {
    // Validate inputs
    if (vmName.empty()) {
        Logger::error("Invalid VM name for getting backup retention");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to get VM ID for " + vmName);
        return false;
    }

    // Get backup retention
    bool success = restClient_->getBackupRetention(vmId, response);
    if (success) {
        Logger::info("Successfully retrieved backup retention for VM " + vmName);
    }
    return success;
}

bool VSphereManager::setBackupRetention(const std::string& vmName, const nlohmann::json& retention, nlohmann::json& response) {
    // Validate inputs
    if (vmName.empty()) {
        Logger::error("Invalid VM name for setting backup retention");
        return false;
    }

    // Validate retention configuration
    if (!retention.contains("days") || !retention.contains("copies")) {
        Logger::error("Missing required fields in retention configuration");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to get VM ID for " + vmName);
        return false;
    }

    // Set backup retention
    bool success = restClient_->setBackupRetention(vmId, retention, response);
    if (success) {
        Logger::info("Successfully set backup retention for VM " + vmName);
    }
    return success;
}

bool VSphereManager::getDiskId(const std::string& vmName, const std::string& diskName, std::string& diskId) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    // Get VM ID
    std::string vmId;
    if (!getVM(vmName, vmId)) {
        Logger::error("Failed to find VM: " + vmName);
        return false;
    }

    // Get disk ID
    std::vector<VirtualDisk> disks = getVirtualDisks(vmId);
    for (const auto& disk : disks) {
        if (disk.name == diskName) {
            diskId = disk.id;
            return true;
        }
    }

    Logger::error("Failed to find disk: " + diskName);
    return false;
} 