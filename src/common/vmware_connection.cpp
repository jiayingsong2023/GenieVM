#include "common/vmware_connection.hpp"
#include "common/logger.hpp"
#include <vixDiskLib.h>
#include <nlohmann/json.hpp>
#include <curl/curl.h>
#include <sstream>
#include <stdexcept>
#include <cstring>

using json = nlohmann::json;

VMwareConnection::VMwareConnection(const std::string& host, const std::string& username, const std::string& password)
    : host_(host), username_(username), password_(password), vddkConnection_(nullptr) {
}

VMwareConnection::~VMwareConnection() {
    disconnectFromDisk();
}

bool VMwareConnection::connect() {
    // Use REST API to connect to vCenter/ESXi
    // This is a placeholder. Replace with actual REST API call.
    Logger::info("Connecting to vCenter/ESXi using REST API");
    return true;
}

void VMwareConnection::disconnect() {
    // Use REST API to disconnect from vCenter/ESXi
    // This is a placeholder. Replace with actual REST API call.
    Logger::info("Disconnecting from vCenter/ESXi using REST API");
}

bool VMwareConnection::getVMHandle(const std::string& vmId, void*& vmHandle) {
    // Use REST API to get VM handle
    // This is a placeholder. Replace with actual REST API call.
    Logger::info("Getting VM handle using REST API");
    return true;
}

bool VMwareConnection::getVMDiskPaths(const std::string& vmId, std::vector<std::string>& diskPaths) {
    // Use REST API to get VM disk paths
    // This is a placeholder. Replace with actual REST API call.
    Logger::info("Getting VM disk paths using REST API");
    return true;
}

bool VMwareConnection::enableCBT(const std::string& vmId) {
    // Use REST API to enable CBT
    // This is a placeholder. Replace with actual REST API call.
    Logger::info("Enabling CBT using REST API");
    return true;
}

bool VMwareConnection::disableCBT(const std::string& vmId) {
    // Use REST API to disable CBT
    // This is a placeholder. Replace with actual REST API call.
    Logger::info("Disabling CBT using REST API");
    return true;
}

bool VMwareConnection::createSnapshot(const std::string& vmId, const std::string& snapshotName, const std::string& description) {
    // Use REST API to create snapshot
    // This is a placeholder. Replace with actual REST API call.
    Logger::info("Creating snapshot using REST API");
    return true;
}

bool VMwareConnection::removeSnapshot(const std::string& vmId, const std::string& snapshotName) {
    // Use REST API to remove snapshot
    // This is a placeholder. Replace with actual REST API call.
    Logger::info("Removing snapshot using REST API");
    return true;
}

bool VMwareConnection::initializeVDDK() {
    VixError vixError = VixDiskLib_Init(1, 1, nullptr, nullptr, nullptr, nullptr);
    if (VIX_FAILED(vixError)) {
        logError("Failed to initialize VDDK");
        return false;
    }
    return true;
}

void VMwareConnection::logError(const std::string& message) {
    char* errorMsg = VixDiskLib_GetErrorText(vixError_, nullptr);
    if (errorMsg) {
        Logger::error(message + ": " + errorMsg);
        VixDiskLib_FreeErrorText(errorMsg);
    } else {
        Logger::error(message);
    }
}

bool VMwareConnection::connectToDisk(const std::string& diskPath) {
    if (!initializeVDDK()) {
        return false;
    }

    VixDiskLibConnectParams connectParams;
    memset(&connectParams, 0, sizeof(connectParams));

    // Allocate memory for strings to avoid const char* issues
    char* vmxSpec = new char[diskPath.length() + 1];
    char* serverName = new char[host_.length() + 1];
    char* userName = new char[username_.length() + 1];
    char* password = new char[password_.length() + 1];

    strcpy(vmxSpec, diskPath.c_str());
    strcpy(serverName, host_.c_str());
    strcpy(userName, username_.c_str());
    strcpy(password, password_.c_str());

    connectParams.vmxSpec = vmxSpec;
    connectParams.serverName = serverName;
    connectParams.creds.uid.userName = userName;
    connectParams.creds.uid.password = password;

    vixError_ = VixDiskLib_Connect(&connectParams, &vddkConnection_);

    // Clean up allocated memory
    delete[] vmxSpec;
    delete[] serverName;
    delete[] userName;
    delete[] password;

    if (VIX_FAILED(vixError_)) {
        logError("Failed to connect to disk");
        return false;
    }

    return true;
}

void VMwareConnection::disconnectFromDisk() {
    if (vddkConnection_) {
        VixDiskLib_Disconnect(vddkConnection_);
        vddkConnection_ = nullptr;
    }
    VixDiskLib_Exit();
}

VixDiskLibConnection VMwareConnection::getVDDKConnection() const {
    return vddkConnection_;
} 