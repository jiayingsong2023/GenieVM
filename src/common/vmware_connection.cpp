#include "common/logger.hpp"
#include "common/vmware_connection.hpp"
#include "common/vsphere_rest_client.hpp"
#include <vixDiskLib.h>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <sstream>
#include <stdexcept>
#include <cstring>

using json = nlohmann::json;

VMwareConnection::VMwareConnection()
    : connected_(false), vddkConnection_(nullptr) {
}

VMwareConnection::VMwareConnection(const std::string& host, const std::string& username, const std::string& password)
    : host_(host), username_(username), password_(password), connected_(false), vddkConnection_(nullptr) {
    restClient_ = std::make_unique<VSphereRestClient>(host, username, password);
}

VMwareConnection::~VMwareConnection() {
    disconnect();
    disconnectFromDisk();
}

bool VMwareConnection::connect(const std::string& host, const std::string& username, const std::string& password) {
    host_ = host;
    username_ = username;
    password_ = password;
    
    if (!restClient_) {
        restClient_ = std::make_unique<VSphereRestClient>(host, username, password);
    }
    
    connected_ = restClient_->login();
    if (!connected_) {
        lastError_ = "Failed to connect to vCenter/ESXi";
        Logger::error(lastError_);
    }
    return connected_;
}

void VMwareConnection::disconnect() {
    if (connected_) {
        restClient_->logout();
        connected_ = false;
    }
}

bool VMwareConnection::isConnected() const {
    return connected_;
}

std::string VMwareConnection::getLastError() const {
    return lastError_;
}

std::vector<std::string> VMwareConnection::listVMs() const {
    if (!connected_) {
        return std::vector<std::string>();
    }

    std::vector<std::string> vmIds;
    json vmInfo;
    if (restClient_->getVMInfo("", vmInfo)) {
        for (const auto& vm : vmInfo["value"]) {
            vmIds.push_back(vm["vm"].get<std::string>());
        }
    }
    return vmIds;
}

bool VMwareConnection::getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths) const {
    if (!connected_) {
        return false;
    }
    return restClient_->getVMDiskPaths(vmId, diskPaths);
}

bool VMwareConnection::getVMInfo(const std::string& vmId, std::string& name, std::string& status) const {
    if (!connected_) {
        return false;
    }

    json vmInfo;
    if (restClient_->getVMInfo(vmId, vmInfo)) {
        name = vmInfo["name"].get<std::string>();
        status = vmInfo["power_state"].get<std::string>();
        return true;
    }
    return false;
}

bool VMwareConnection::getCBTInfo(const std::string& vmId, bool& enabled, std::string& changeId) const {
    if (!connected_) {
        return false;
    }

    json vmInfo;
    if (restClient_->getVMInfo(vmId, vmInfo)) {
        enabled = vmInfo["change_tracking_enabled"].get<bool>();
        changeId = vmInfo["change_tracking_id"].get<std::string>();
        return true;
    }
    return false;
}

bool VMwareConnection::enableCBT(const std::string& vmId) {
    if (!connected_) {
        return false;
    }
    return restClient_->enableCBT(vmId);
}

bool VMwareConnection::disableCBT(const std::string& vmId) {
    if (!connected_) {
        return false;
    }
    return restClient_->disableCBT(vmId);
}

bool VMwareConnection::isCBTEnabled(const std::string& vmId) const {
    if (!connected_) {
        return false;
    }

    bool enabled;
    std::string changeId;
    return getCBTInfo(vmId, enabled, changeId) && enabled;
}

bool VMwareConnection::getChangedBlocks(const std::string& vmId, const std::string& diskPath,
                                      std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) const {
    if (!connected_) {
        return false;
    }

    json diskInfo;
    if (restClient_->getVMDiskInfo(vmId, diskPath, diskInfo)) {
        if (diskInfo.contains("changed_blocks")) {
            for (const auto& block : diskInfo["changed_blocks"]) {
                changedBlocks.emplace_back(
                    block["start"].get<uint64_t>(),
                    block["length"].get<uint64_t>()
                );
            }
            return true;
        }
    }
    return false;
}

bool VMwareConnection::initializeVDDK() {
    VixError vixError = VixDiskLib_Init(1, 1, nullptr, nullptr, nullptr, nullptr);
    if (VIX_FAILED(vixError)) {
        lastError_ = "Failed to initialize VDDK";
        Logger::error(lastError_);
        return false;
    }
    return true;
}

void VMwareConnection::cleanupVDDK() {
    VixDiskLib_Exit();
}

VixDiskLibConnection VMwareConnection::getVDDKConnection() const {
    return vddkConnection_;
}

void VMwareConnection::disconnectFromDisk() {
    if (vddkConnection_) {
        VixDiskLib_Disconnect(vddkConnection_);
        vddkConnection_ = nullptr;
    }
}

bool VMwareConnection::getBackup(const std::string& backupId, nlohmann::json& backupInfo)  {
    if (!connected_) {
        lastError_ = "Not connected to vSphere";
        return false;
    }

    try {
        // Use the REST client to get backup information
        if (!restClient_) {
            lastError_ = "REST client not initialized";
            return false;
        }

        // Get backup details from the REST API
        std::string response;
        if (!restClient_->getBackup(backupId, response)) {
            lastError_ = "Failed to get backup information from REST API";
            return false;
        }

        // Parse the response into JSON
        backupInfo = nlohmann::json::parse(response);
        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Error getting backup information: ") + e.what();
        return false;
    }
}

bool VMwareConnection::createVM(const nlohmann::json& vmConfig, nlohmann::json& response) {
    if (!isConnected()) {
        lastError_ = "Not connected to vSphere";
        return false;
    }

    try {
        std::string responseStr;
        nlohmann::json responseJson;
        if (!restClient_->createVM(vmConfig, responseJson)) {
            lastError_ = "Failed to create VM: " + restClient_->getLastError();
            return false;
        }
        response = responseJson;
        return true;
    } catch (const std::exception& e) {
        lastError_ = "Exception in createVM: " + std::string(e.what());
        return false;
    }
}

bool VMwareConnection::attachDisk(const std::string& vmId, const nlohmann::json& diskConfig, nlohmann::json& response) {
    if (!isConnected()) {
        lastError_ = "Not connected to vSphere";
        return false;
    }

    try {
        std::string responseStr;
        nlohmann::json responseJson;
        if (!restClient_->attachDisk(vmId, diskConfig, responseJson)) {
            lastError_ = "Failed to attach disk: " + restClient_->getLastError();
            return false;
        }
        response = responseJson;
        return true;
    } catch (const std::exception& e) {
        lastError_ = "Exception in attachDisk: " + std::string(e.what());
        return false;
    }
}

bool VMwareConnection::powerOnVM(const std::string& vmId) {
    if (!connected_) {
        lastError_ = "Not connected to vSphere";
        return false;
    }

    try {
        // Use the REST client to power on VM
        if (!restClient_) {
            lastError_ = "REST client not initialized";
            return false;
        }

        // Power on VM using REST API
        if (!restClient_->powerOnVM(vmId)) {
            lastError_ = "Failed to power on VM via REST API";
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Error powering on VM: ") + e.what();
        return false;
    }
}

bool VMwareConnection::verifyBackup(const std::string& backupId, nlohmann::json& response) {
    if (!isConnected()) {
        lastError_ = "Not connected to vSphere";
        return false;
    }

    try {
        std::string responseStr;
        nlohmann::json responseJson;
        if (!restClient_->verifyBackup(backupId, responseJson)) {
            lastError_ = "Failed to verify backup: " + restClient_->getLastError();
            return false;
        }
        response = responseJson;
        return true;
    } catch (const std::exception& e) {
        lastError_ = "Exception in verifyBackup: " + std::string(e.what());
        return false;
    }
} 
