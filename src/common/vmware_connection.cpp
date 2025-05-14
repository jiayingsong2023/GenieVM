#include "common/vmware_connection.hpp"
#include "common/logger.hpp"
#include <vixDiskLib.h>

VMwareConnection::VMwareConnection(const std::string& host,
                                 const std::string& username,
                                 const std::string& password)
    : host_(host)
    , username_(username)
    , password_(password)
    , hostHandle_(nullptr)
    , connected_(false)
    , vddkConnection_(nullptr) {
}

VMwareConnection::~VMwareConnection() {
    disconnect();
    disconnectFromDisk();
}

bool VMwareConnection::connect() {
    if (connected_) {
        return true;
    }

    if (!initializeVDDK()) {
        return false;
    }

    // Connect to vCenter
    VixError vixError = VixHost_Connect(VIX_API_VERSION,
                                       VIX_SERVICEPROVIDER_VMWARE_SERVER,
                                       host_.c_str(),
                                       0,  // port
                                       username_.c_str(),
                                       password_.c_str(),
                                       0,  // options
                                       VIX_INVALID_HANDLE,  // propertyListHandle
                                       &hostHandle_);

    if (VIX_FAILED(vixError)) {
        logError("Failed to connect to vCenter");
        return false;
    }

    connected_ = true;
    Logger::info("Successfully connected to vCenter: " + host_);
    return true;
}

void VMwareConnection::disconnect() {
    if (connected_) {
        if (hostHandle_ != nullptr) {
            VixHost_Disconnect(hostHandle_);
            hostHandle_ = nullptr;
        }
        cleanupVDDK();
        connected_ = false;
        Logger::info("Disconnected from vCenter");
    }
}

bool VMwareConnection::getVMHandle(const std::string& vmName, VixHandle& vmHandle) {
    if (!connected_) {
        Logger::error("Not connected to vCenter");
        return false;
    }

    VixError vixError = VixHost_FindItem(hostHandle_,
                                        VIX_FIND_RUNNING_VMS,
                                        vmName.c_str(),
                                        -1,  // searchType
                                        &vmHandle);

    if (VIX_FAILED(vixError)) {
        logError("Failed to get VM handle");
        return false;
    }

    return true;
}

bool VMwareConnection::getVMDiskPaths(const std::string& vmName,
                                    std::vector<std::string>& diskPaths) {
    if (!connected_) {
        Logger::error("Not connected to vCenter");
        return false;
    }

    VixHandle vmHandle;
    if (!getVMHandle(vmName, vmHandle)) {
        return false;
    }

    // Get VM configuration
    VixError vixError = VixVM_GetConfigInfo(vmHandle,
                                           VIX_VM_CONFIG_NUM_VIRTUAL_DISKS,
                                           nullptr);

    if (VIX_FAILED(vixError)) {
        logError("Failed to get VM configuration");
        return false;
    }

    // TODO: Implement disk path retrieval
    // This is a placeholder - you'll need to implement the actual disk path retrieval
    // using VixVM_GetConfigInfo with appropriate parameters

    return true;
}

bool VMwareConnection::enableCBT(const std::string& vmName) {
    if (!connected_) {
        Logger::error("Not connected to vCenter");
        return false;
    }

    VixHandle vmHandle;
    if (!getVMHandle(vmName, vmHandle)) {
        return false;
    }

    VixError vixError = VixVM_EnableChangedBlockTracking(vmHandle);
    if (VIX_FAILED(vixError)) {
        logError("Failed to enable CBT");
        return false;
    }

    return true;
}

bool VMwareConnection::disableCBT(const std::string& vmName) {
    if (!connected_) {
        Logger::error("Not connected to vCenter");
        return false;
    }

    VixHandle vmHandle;
    if (!getVMHandle(vmName, vmHandle)) {
        return false;
    }

    VixError vixError = VixVM_DisableChangedBlockTracking(vmHandle);
    if (VIX_FAILED(vixError)) {
        logError("Failed to disable CBT");
        return false;
    }

    return true;
}

bool VMwareConnection::getCBTInfo(const std::string& vmName, void* blockList) {
    if (!connected_) {
        Logger::error("Not connected to vCenter");
        return false;
    }

    VixHandle vmHandle;
    if (!getVMHandle(vmName, vmHandle)) {
        return false;
    }

    // TODO: Implement CBT info retrieval
    // This is a placeholder - you'll need to implement the actual CBT info retrieval
    // using appropriate VixVM functions

    return true;
}

bool VMwareConnection::createSnapshot(const std::string& vmName,
                                    const std::string& snapshotName,
                                    const std::string& description) {
    if (!connected_) {
        Logger::error("Not connected to vCenter");
        return false;
    }

    VixHandle vmHandle;
    if (!getVMHandle(vmName, vmHandle)) {
        return false;
    }

    VixError vixError = VixVM_CreateSnapshot(
        vmHandle,
        snapshotName.c_str(),
        description.c_str(),
        VIX_SNAPSHOT_INCLUDE_MEMORY,
        VIX_INVALID_HANDLE,
        nullptr);

    if (VIX_FAILED(vixError)) {
        logError("Failed to create snapshot");
        return false;
    }

    Logger::info("Successfully created snapshot: " + snapshotName);
    return true;
}

bool VMwareConnection::removeSnapshot(const std::string& vmName,
                                    const std::string& snapshotName) {
    if (!connected_) {
        Logger::error("Not connected to vCenter");
        return false;
    }

    VixHandle vmHandle;
    if (!getVMHandle(vmName, vmHandle)) {
        return false;
    }

    VixHandle snapshotHandle;
    VixError vixError = VixVM_GetNamedSnapshot(
        vmHandle,
        snapshotName.c_str(),
        &snapshotHandle);

    if (VIX_FAILED(vixError)) {
        logError("Failed to get snapshot handle");
        return false;
    }

    vixError = VixVM_RemoveSnapshot(
        vmHandle,
        snapshotHandle,
        VIX_SNAPSHOT_REMOVE_CHILDREN,
        nullptr);

    if (VIX_FAILED(vixError)) {
        logError("Failed to remove snapshot");
        return false;
    }

    Logger::info("Successfully removed snapshot: " + snapshotName);
    return true;
}

bool VMwareConnection::initializeVDDK() {
    VixError vixError = VixDiskLib_InitEx(
        VIXDISKLIB_VERSION_MAJOR,
        VIXDISKLIB_VERSION_MINOR,
        nullptr,  // log callback
        nullptr,  // warning callback
        nullptr,  // panic callback
        nullptr,  // libDir
        nullptr); // configFile

    if (VIX_FAILED(vixError)) {
        logError("Failed to initialize VDDK");
        return false;
    }

    return true;
}

void VMwareConnection::cleanupVDDK() {
    VixDiskLib_Exit();
}

void VMwareConnection::logError(const std::string& operation) {
    char* errorMsg = nullptr;
    Vix_GetErrorText(VIX_ERROR_CODE, &errorMsg);
    if (errorMsg) {
        Logger::error(operation + ": " + errorMsg);
        Vix_FreeErrorText(errorMsg);
    } else {
        Logger::error(operation + ": Unknown error");
    }
}

bool VMwareConnection::connectToDisk(const std::string& vmxPath) {
    if (!connected_) {
        Logger::error("Not connected to vCenter");
        return false;
    }

    if (!initializeVDDK()) {
        return false;
    }

    VixDiskLibConnectParams connectParams = {0};
    connectParams.vmxSpec = const_cast<char*>(vmxPath.c_str());
    connectParams.serverName = const_cast<char*>(host_.c_str());
    connectParams.credType = VIXDISKLIB_CRED_UID;
    connectParams.creds.uid.userName = const_cast<char*>(username_.c_str());
    connectParams.creds.uid.password = const_cast<char*>(password_.c_str());
    connectParams.thumbPrint = nullptr;
    connectParams.port = 0;

    VixError error = VixDiskLib_Connect(&connectParams, &vddkConnection_);
    if (VIX_FAILED(error)) {
        Logger::error("Failed to connect to VM disk: " + std::string(VixDiskLib_GetErrorText(error, nullptr)));
        return false;
    }

    return true;
}

void VMwareConnection::disconnectFromDisk() {
    if (vddkConnection_) {
        VixDiskLib_Disconnect(vddkConnection_);
        vddkConnection_ = nullptr;
    }
} 