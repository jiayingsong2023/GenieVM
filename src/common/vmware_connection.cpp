#include "common/logger.hpp"
#include "common/vmware_connection.hpp"
#include "common/vsphere_rest_client.hpp"
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <sstream>
#include <stdexcept>
#include <cstring>
#include "vddk_wrapper/vddk_wrapper.h"

using json = nlohmann::json;

VMwareConnection::VMwareConnection()
    : connected_(false), initialized_(false), vddkConnection_(nullptr), restClient_(nullptr), refCount_(0), lastError_("") {
    Logger::debug("VMwareConnection default constructor called");
}

VMwareConnection::VMwareConnection(const std::string& host, const std::string& username, const std::string& password)
    : server_(host), username_(username), password_(password), connected_(false), initialized_(false), vddkConnection_(nullptr), restClient_(nullptr), refCount_(0), lastError_("") {
    Logger::debug("VMwareConnection parameterized constructor called for server: " + host);
    restClient_ = new VSphereRestClient(host, username, password);
}

VMwareConnection::~VMwareConnection() {
    Logger::debug("VMwareConnection destructor called for server: " + server_ + " - current ref count: " + std::to_string(refCount_));
    Logger::debug("Connection state at destruction - connected: " + std::string(connected_ ? "true" : "false"));
    
    // Only cleanup if no active operations
    if (refCount_ == 0) {
        // First disconnect if still connected
        if (connected_) {
            Logger::debug("Disconnecting in VMwareConnection destructor for server: " + server_);
            disconnect();
        }
        
        // Then cleanup disk connection
        Logger::debug("Cleaning up disk connection for server: " + server_);
        disconnectFromDisk();
        
        // Finally cleanup REST client
        if (restClient_) {
            Logger::debug("Cleaning up REST client in VMwareConnection destructor for server: " + server_);
            delete restClient_;
            restClient_ = nullptr;
            Logger::debug("REST client cleanup completed for server: " + server_);
        }
    } else {
        Logger::info("Skipping cleanup in destructor as there are " + std::to_string(refCount_) + " active operations for server: " + server_);
    }
    Logger::debug("VMwareConnection destructor completed for server: " + server_);
}

bool VMwareConnection::connect(const std::string& host, const std::string& username, const std::string& password) {
    Logger::info("Initializing VMware connection to: " + host);
    
    server_ = host;
    username_ = username;
    password_ = password;
    
    if (!restClient_) {
        Logger::debug("Creating new REST client instance");
        restClient_ = new VSphereRestClient(host, username, password);
    }
    
    Logger::info("Attempting to establish connection to vCenter/ESXi");
    connected_ = restClient_->login();
    if (!connected_) {
        lastError_ = "Failed to connect to vCenter/ESXi";
        Logger::error(lastError_ + ". Please check the following:");
        Logger::error("1. vCenter/ESXi host is reachable");
        Logger::error("2. Credentials are correct");
        Logger::error("3. Network connectivity and firewall settings");
        Logger::error("4. SSL/TLS configuration");
    } else {
        Logger::info("Successfully connected to vCenter/ESXi");
        Logger::debug("Connection established with server: " + host + ", username: " + username);
        Logger::debug("Current ref count after connection: " + std::to_string(refCount_));
    }
    return connected_;
}

void VMwareConnection::disconnect() {
    Logger::debug("Disconnect called for server: " + server_ + ", current ref count: " + std::to_string(refCount_));
    // Only disconnect if no active operations
    if (refCount_ == 0) {
        if (connected_ && restClient_) {
            Logger::debug("Logging out from VMwareConnection for server: " + server_);
            restClient_->logout();
            connected_ = false;
            Logger::debug("Successfully logged out from server: " + server_);
        }
    } else {
        Logger::info("Skipping disconnect as there are " + std::to_string(refCount_) + " active operations for server: " + server_);
    }
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

bool VMwareConnection::initialize() {
    // Initialize VDDK
    VixError vixError = VixDiskLib_InitWrapper(VIXDISKLIB_VERSION_MAJOR,
                                             VIXDISKLIB_VERSION_MINOR,
                                             nullptr);
    if (vixError != VIX_OK) {
        char* errorMsg = VixDiskLib_GetErrorTextWrapper(vixError, nullptr, 0);
        if (errorMsg) {
            lastError_ = errorMsg;
            VixDiskLib_FreeErrorTextWrapper(errorMsg);
        }
        return false;
    }
    initialized_ = true;
    return true;
}

void VMwareConnection::cleanupVDDK() {
    if (vddkConnection_) {
        VixDiskLib_DisconnectWrapper(&vddkConnection_);
        vddkConnection_ = nullptr;
    }
    VixDiskLib_ExitWrapper();
    initialized_ = false;
}

VDDKConnection VMwareConnection::getVDDKConnection() const {
    return vddkConnection_;
}

void VMwareConnection::disconnectFromDisk() {
    if (vddkConnection_) {
        VixDiskLib_DisconnectWrapper(&vddkConnection_);
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

void VMwareConnection::incrementRefCount() {
    Logger::debug("Incrementing ref count from " + std::to_string(refCount_) + " for server: " + server_);
    refCount_++;
    Logger::debug("Incremented ref count to " + std::to_string(refCount_));
}

void VMwareConnection::decrementRefCount() {
    Logger::debug("Decrementing ref count from " + std::to_string(refCount_) + " for server: " + server_);
    if (refCount_ > 0) {
        refCount_--;
        Logger::debug("Decremented ref count to " + std::to_string(refCount_));
        
        // If this was the last operation, cleanup
        if (refCount_ == 0) {
            Logger::debug("No more active operations, cleaning up connection for server: " + server_);
            if (connected_) {
                disconnect();
            }
        }
    } else {
        Logger::info("Attempted to decrement ref count below 0 for server: " + server_);
    }
} 
