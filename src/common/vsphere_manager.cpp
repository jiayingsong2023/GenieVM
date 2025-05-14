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

    // TODO: Implement VM creation using REST client
    return true;
}

bool VSphereManager::attachDisks(const std::string& vmName,
                               const std::vector<std::string>& diskPaths) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    // TODO: Implement disk attachment using REST client
    return true;
}

bool VSphereManager::getVM(const std::string& vmName, std::string& vmId) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    // TODO: Implement VM lookup using REST client
    return true;
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

    // TODO: Implement VM listing using REST client
    return {};
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

    // TODO: Implement power on using REST client
    return true;
}

bool VSphereManager::powerOffVM(const std::string& vmId) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    // TODO: Implement power off using REST client
    return true;
}

bool VSphereManager::suspendVM(const std::string& vmId) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    // TODO: Implement suspend using REST client
    return true;
}

bool VSphereManager::resetVM(const std::string& vmId) {
    if (!connected_) {
        Logger::error("Not connected to vSphere");
        return false;
    }

    // TODO: Implement reset using REST client
    return true;
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