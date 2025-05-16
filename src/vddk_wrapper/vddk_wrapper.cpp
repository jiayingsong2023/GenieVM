#include "vddk_wrapper/vddk_wrapper.h"
#include <vixDiskLib.h>
#include <string>
#include <memory>
#include <dlfcn.h>
#include <cstdlib>

// Define VDDK types if not already defined
typedef void (*VixDiskLibLogFunc)(const char *fmt, ...);
typedef void (*VixDiskLibWarnFunc)(const char *fmt, ...);
typedef void (*VixDiskLibPanicFunc)(const char *fmt, ...);

// Function pointers for VDDK functions
static void* vddk_handle = nullptr;
static VixError (*VixDiskLib_Init_impl)(uint32_t, uint32_t, VixDiskLibLogFunc*, VixDiskLibWarnFunc*, VixDiskLibPanicFunc*, const char*) = nullptr;
static VixError (*VixDiskLib_Connect_impl)(const VixDiskLibConnectParams*, VixDiskLibConnection*) = nullptr;
static VixError (*VixDiskLib_Open_impl)(VixDiskLibConnection, const char*, uint32, VixDiskLibHandle*) = nullptr;
static VixError (*VixDiskLib_Read_impl)(VixDiskLibHandle, VixDiskLibSectorType, VixDiskLibSectorType, uint8*) = nullptr;
static VixError (*VixDiskLib_Write_impl)(VixDiskLibHandle, VixDiskLibSectorType, VixDiskLibSectorType, const uint8*) = nullptr;
static VixError (*VixDiskLib_Close_impl)(VixDiskLibHandle*) = nullptr;
static VixError (*VixDiskLib_Disconnect_impl)(VixDiskLibConnection*) = nullptr;
static void (*VixDiskLib_Exit_impl)() = nullptr;

// Load VDDK library and its functions
static bool load_vddk_library(const char* lib_dir) {
    if (vddk_handle) {
        return true;  // Already loaded
    }

    // Set LD_LIBRARY_PATH to include system libraries first, then VDDK's lib directory
    std::string ld_library_path = "LD_LIBRARY_PATH=/usr/lib/x86_64-linux-gnu:/usr/lib64:/usr/lib";
    if (const char* existing_path = std::getenv("LD_LIBRARY_PATH")) {
        ld_library_path += ":";
        ld_library_path += existing_path;
    }
    ld_library_path += ":";
    ld_library_path += lib_dir;
    putenv(const_cast<char*>(ld_library_path.c_str()));

    // Load system libcurl first with full path
    void* curl_handle = dlopen("/usr/lib/x86_64-linux-gnu/libcurl.so.4", RTLD_NOW | RTLD_GLOBAL);
    if (!curl_handle) {
        return false;
    }

    // Load VDDK library with full path
    std::string lib_path = std::string(lib_dir) + "/libvixDiskLib.so";
    vddk_handle = dlopen(lib_path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!vddk_handle) {
        dlclose(curl_handle);
        return false;
    }

    // Load function pointers
    VixDiskLib_Init_impl = (VixError (*)(uint32_t, uint32_t, VixDiskLibLogFunc*, VixDiskLibWarnFunc*, VixDiskLibPanicFunc*, const char*))dlsym(vddk_handle, "VixDiskLib_Init");
    VixDiskLib_Connect_impl = (VixError (*)(const VixDiskLibConnectParams*, VixDiskLibConnection*))dlsym(vddk_handle, "VixDiskLib_Connect");
    VixDiskLib_Open_impl = (VixError (*)(VixDiskLibConnection, const char*, uint32, VixDiskLibHandle*))dlsym(vddk_handle, "VixDiskLib_Open");
    VixDiskLib_Read_impl = (VixError (*)(VixDiskLibHandle, VixDiskLibSectorType, VixDiskLibSectorType, uint8*))dlsym(vddk_handle, "VixDiskLib_Read");
    VixDiskLib_Write_impl = (VixError (*)(VixDiskLibHandle, VixDiskLibSectorType, VixDiskLibSectorType, const uint8*))dlsym(vddk_handle, "VixDiskLib_Write");
    VixDiskLib_Close_impl = (VixError (*)(VixDiskLibHandle*))dlsym(vddk_handle, "VixDiskLib_Close");
    VixDiskLib_Disconnect_impl = (VixError (*)(VixDiskLibConnection*))dlsym(vddk_handle, "VixDiskLib_Disconnect");
    VixDiskLib_Exit_impl = (void (*)())dlsym(vddk_handle, "VixDiskLib_Exit");

    return VixDiskLib_Init_impl && VixDiskLib_Connect_impl && VixDiskLib_Open_impl &&
           VixDiskLib_Read_impl && VixDiskLib_Write_impl && VixDiskLib_Close_impl &&
           VixDiskLib_Disconnect_impl && VixDiskLib_Exit_impl;
}

extern "C" {

// Wrapper functions that use C++11 internally but expose C-style interface
VixError VixDiskLib_InitWrapper(uint32_t majorVersion,
                               uint32_t minorVersion,
                               VixDiskLibConnectParams *connectParams,
                               const char *libDir,
                               const char *configFile) {
    if (!load_vddk_library(libDir)) {
        return VIX_E_FAIL;
    }
    return VixDiskLib_Init_impl(majorVersion, minorVersion, 
                               nullptr,  // log function
                               nullptr,  // warn function
                               nullptr,  // panic function
                               libDir);
}

VixError VixDiskLib_ConnectWrapper(const VixDiskLibConnectParams *connectParams,
                                  VixDiskLibConnection *connection) {
    return VixDiskLib_Connect_impl(connectParams, connection);
}

VixError VixDiskLib_OpenWrapper(const VixDiskLibConnection connection,
                               const char *path,
                               uint32 flags,
                               VixDiskLibHandle *diskHandle) {
    return VixDiskLib_Open_impl(connection, path, flags, diskHandle);
}

VixError VixDiskLib_ReadWrapper(VixDiskLibHandle diskHandle,
                               VixDiskLibSectorType startSector,
                               VixDiskLibSectorType numSectors,
                               uint8 *readBuffer) {
    return VixDiskLib_Read_impl(diskHandle, startSector, numSectors, readBuffer);
}

VixError VixDiskLib_WriteWrapper(VixDiskLibHandle diskHandle,
                                VixDiskLibSectorType startSector,
                                VixDiskLibSectorType numSectors,
                                const uint8 *writeBuffer) {
    return VixDiskLib_Write_impl(diskHandle, startSector, numSectors, writeBuffer);
}

VixError VixDiskLib_CloseWrapper(VixDiskLibHandle *diskHandle) {
    VixError err = VixDiskLib_Close_impl(diskHandle);
    if (err == VIX_OK) {
        *diskHandle = nullptr;
    }
    return err;
}

VixError VixDiskLib_DisconnectWrapper(VixDiskLibConnection *connection) {
    VixError err = VixDiskLib_Disconnect_impl(connection);
    if (err == VIX_OK) {
        *connection = nullptr;
    }
    return err;
}

void VixDiskLib_ExitWrapper() {
    if (VixDiskLib_Exit_impl) {
        VixDiskLib_Exit_impl();
    }
    if (vddk_handle) {
        dlclose(vddk_handle);
        vddk_handle = nullptr;
    }
}

} // extern "C" 