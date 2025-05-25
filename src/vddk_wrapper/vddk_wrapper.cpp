#include <dlfcn.h>
#include <string>
#include <memory>
#include <vector>
#include <stdexcept>
#include <vixDiskLib.h>
#include "vddk_wrapper/vddk_wrapper.h"

// Define progress function type since it's not in vixDiskLib.h
typedef void (*VixDiskLibGenericProgressFunc)(void* data, int percent);

// Function pointers for VDDK functions
static VixError (*pfn_VixDiskLib_Init)(uint32, uint32, VixDiskLibGenericLogFunc*, VixDiskLibGenericLogFunc*, VixDiskLibGenericLogFunc*, const char*) = nullptr;
static void (*pfn_VixDiskLib_Exit)(void) = nullptr;
static VixError (*pfn_VixDiskLib_Connect)(const VixDiskLibConnectParams*, VixDiskLibConnection*) = nullptr;
static VixError (*pfn_VixDiskLib_Disconnect)(VixDiskLibConnection) = nullptr;
static VixError (*pfn_VixDiskLib_Open)(const VixDiskLibConnection, const char*, uint32, VixDiskLibHandle*) = nullptr;
static VixError (*pfn_VixDiskLib_Close)(VixDiskLibHandle) = nullptr;
static VixError (*pfn_VixDiskLib_GetInfo)(VixDiskLibHandle, VixDiskLibInfo**) = nullptr;
static VixError (*pfn_VixDiskLib_FreeInfo)(VixDiskLibInfo*) = nullptr;
static VixError (*pfn_VixDiskLib_Create)(const VixDiskLibConnection, const char*, const VixDiskLibCreateParams*, VixDiskLibGenericProgressFunc, void*) = nullptr;
static VixError (*pfn_VixDiskLib_Clone)(const VixDiskLibConnection, const char*, const VixDiskLibConnection, const char*, const VixDiskLibCreateParams*, VixDiskLibGenericProgressFunc, void*, bool) = nullptr;
static VixError (*pfn_VixDiskLib_Read)(VixDiskLibHandle, VixDiskLibSectorType, VixDiskLibSectorType, uint8*) = nullptr;
static VixError (*pfn_VixDiskLib_Write)(VixDiskLibHandle, VixDiskLibSectorType, VixDiskLibSectorType, const uint8*) = nullptr;
static VixError (*pfn_VixDiskLib_QueryAllocatedBlocks)(VixDiskLibHandle, VixDiskLibSectorType, VixDiskLibSectorType, VixDiskLibBlockList**) = nullptr;
static void (*pfn_VixDiskLib_FreeBlockList)(VixDiskLibBlockList*) = nullptr;
static char* (*pfn_VixDiskLib_GetErrorText)(VixError, char*, size_t) = nullptr;
static void (*pfn_VixDiskLib_FreeErrorText)(char*) = nullptr;

// Helper function to load a library
static void* load_library(const char* path) {
    void* handle = dlopen(path, RTLD_NOW | RTLD_DEEPBIND);
    if (!handle) {
        throw std::runtime_error(std::string("Failed to load library: ") + path + " - " + dlerror());
    }
    return handle;
}

// Load VDDK library and its dependencies
static void load_vddk_library() {
    static bool initialized = false;
    if (initialized) return;

    // Set environment variables
    setenv("LD_PRELOAD", "/usr/local/vddk/lib64/libstdc++.so.6", 1);
    setenv("LD_LIBRARY_PATH", "/usr/local/vddk/lib64", 1);

    // Load dependencies in order
    std::vector<std::string> deps = {
        "/usr/local/vddk/lib64/libgcc_s.so.1",
        "/usr/local/vddk/lib64/libstdc++.so.6",
        "/usr/local/vddk/lib64/libcrypto.so.1.0.2",
        "/usr/local/vddk/lib64/libssl.so.1.0.2",
        "/usr/local/vddk/lib64/libcurl.so.4",
        "/usr/local/vddk/lib64/libxml2.so.2",
        "/usr/local/vddk/lib64/libz.so.1",
        "/usr/local/vddk/lib64/libcares.so.2",
        "/usr/local/vddk/lib64/libvixDiskLib.so"
    };

    for (const auto& dep : deps) {
        try {
            load_library(dep.c_str());
        } catch (const std::runtime_error& e) {
            // Log warning but continue
            fprintf(stderr, "Warning: %s\n", e.what());
        }
    }

    // Load VDDK library
    void* vddk_handle = load_library("/usr/local/vddk/lib64/libvixDiskLib.so");

    // Get function pointers
    pfn_VixDiskLib_Init = (VixError (*)(uint32, uint32, VixDiskLibGenericLogFunc*, VixDiskLibGenericLogFunc*, VixDiskLibGenericLogFunc*, const char*))dlsym(vddk_handle, "VixDiskLib_Init");
    pfn_VixDiskLib_Exit = (void (*)(void))dlsym(vddk_handle, "VixDiskLib_Exit");
    pfn_VixDiskLib_Connect = (VixError (*)(const VixDiskLibConnectParams*, VixDiskLibConnection*))dlsym(vddk_handle, "VixDiskLib_Connect");
    pfn_VixDiskLib_Disconnect = (VixError (*)(VixDiskLibConnection))dlsym(vddk_handle, "VixDiskLib_Disconnect");
    pfn_VixDiskLib_Open = (VixError (*)(const VixDiskLibConnection, const char*, uint32, VixDiskLibHandle*))dlsym(vddk_handle, "VixDiskLib_Open");
    pfn_VixDiskLib_Close = (VixError (*)(VixDiskLibHandle))dlsym(vddk_handle, "VixDiskLib_Close");
    pfn_VixDiskLib_GetInfo = (VixError (*)(VixDiskLibHandle, VixDiskLibInfo**))dlsym(vddk_handle, "VixDiskLib_GetInfo");
    pfn_VixDiskLib_FreeInfo = (VixError (*)(VixDiskLibInfo*))dlsym(vddk_handle, "VixDiskLib_FreeInfo");
    pfn_VixDiskLib_Create = (VixError (*)(const VixDiskLibConnection, const char*, const VixDiskLibCreateParams*, VixDiskLibGenericProgressFunc, void*))dlsym(vddk_handle, "VixDiskLib_Create");
    pfn_VixDiskLib_Clone = (VixError (*)(const VixDiskLibConnection, const char*, const VixDiskLibConnection, const char*, const VixDiskLibCreateParams*, VixDiskLibGenericProgressFunc, void*, bool))dlsym(vddk_handle, "VixDiskLib_Clone");
    pfn_VixDiskLib_Read = (VixError (*)(VixDiskLibHandle, VixDiskLibSectorType, VixDiskLibSectorType, uint8*))dlsym(vddk_handle, "VixDiskLib_Read");
    pfn_VixDiskLib_Write = (VixError (*)(VixDiskLibHandle, VixDiskLibSectorType, VixDiskLibSectorType, const uint8*))dlsym(vddk_handle, "VixDiskLib_Write");
    pfn_VixDiskLib_QueryAllocatedBlocks = (VixError (*)(VixDiskLibHandle, VixDiskLibSectorType, VixDiskLibSectorType, VixDiskLibBlockList**))dlsym(vddk_handle, "VixDiskLib_QueryAllocatedBlocks");
    pfn_VixDiskLib_FreeBlockList = (void (*)(VixDiskLibBlockList*))dlsym(vddk_handle, "VixDiskLib_FreeBlockList");
    pfn_VixDiskLib_GetErrorText = (char* (*)(VixError, char*, size_t))dlsym(vddk_handle, "VixDiskLib_GetErrorText");
    pfn_VixDiskLib_FreeErrorText = (void (*)(char*))dlsym(vddk_handle, "VixDiskLib_FreeErrorText");

    initialized = true;
}

// Wrapper functions
extern "C" {

VixError VixDiskLib_InitWrapper(uint32_t majorVersion,
                               uint32_t minorVersion,
                               const char* configFile) {
    load_vddk_library();
    if (!pfn_VixDiskLib_Init) return VIX_E_FAIL;
    return pfn_VixDiskLib_Init(majorVersion, minorVersion, nullptr, nullptr, nullptr, configFile);
}

void VixDiskLib_ExitWrapper() {
    if (pfn_VixDiskLib_Exit) pfn_VixDiskLib_Exit();
}

VixError VixDiskLib_ConnectWrapper(const VDDKConnectParams* connectParams,
                                  VDDKConnection* connection) {
    if (!pfn_VixDiskLib_Connect) return VIX_E_FAIL;
    return pfn_VixDiskLib_Connect(reinterpret_cast<const VixDiskLibConnectParams*>(connectParams),
                                reinterpret_cast<VixDiskLibConnection*>(connection));
}

VixError VixDiskLib_DisconnectWrapper(VDDKConnection* connection) {
    if (!pfn_VixDiskLib_Disconnect) return VIX_E_FAIL;
    return pfn_VixDiskLib_Disconnect(*reinterpret_cast<VixDiskLibConnection*>(connection));
}

VixError VixDiskLib_OpenWrapper(const VDDKConnection connection,
                               const char* path,
                               uint32_t flags,
                               VDDKHandle* handle) {
    if (!pfn_VixDiskLib_Open) return VIX_E_FAIL;
    return pfn_VixDiskLib_Open(reinterpret_cast<const VixDiskLibConnection>(connection),
                             path,
                             flags,
                             reinterpret_cast<VixDiskLibHandle*>(handle));
}

VixError VixDiskLib_CloseWrapper(VDDKHandle* handle) {
    if (!pfn_VixDiskLib_Close) return VIX_E_FAIL;
    return pfn_VixDiskLib_Close(*reinterpret_cast<VixDiskLibHandle*>(handle));
}

VixError VixDiskLib_GetInfoWrapper(VDDKHandle handle,
                                  VDDKInfo** info) {
    if (!pfn_VixDiskLib_GetInfo) return VIX_E_FAIL;
    return pfn_VixDiskLib_GetInfo(reinterpret_cast<VixDiskLibHandle>(handle),
                                reinterpret_cast<VixDiskLibInfo**>(info));
}

VixError VixDiskLib_FreeInfoWrapper(VDDKInfo* info) {
    if (!pfn_VixDiskLib_FreeInfo) return VIX_E_FAIL;
    return pfn_VixDiskLib_FreeInfo(reinterpret_cast<VixDiskLibInfo*>(info));
}

VixError VixDiskLib_CreateWrapper(const VDDKConnection connection,
                                 const char* path,
                                 const VDDKCreateParams* createParams,
                                 void (*progressFunc)(void* data, int percent),
                                 void* progressCallbackData) {
    if (!pfn_VixDiskLib_Create) return VIX_E_FAIL;
    return pfn_VixDiskLib_Create(reinterpret_cast<const VixDiskLibConnection>(connection),
                               path,
                               reinterpret_cast<const VixDiskLibCreateParams*>(createParams),
                               static_cast<VixDiskLibGenericProgressFunc>(progressFunc),
                               progressCallbackData);
}

VixError VixDiskLib_CloneWrapper(const VDDKConnection connection,
                                const char* path,
                                const VDDKConnection srcConnection,
                                const char* srcPath,
                                const VDDKCreateParams* createParams,
                                void (*progressFunc)(void* data, int percent),
                                void* progressCallbackData,
                                bool doInflate) {
    if (!pfn_VixDiskLib_Clone) return VIX_E_FAIL;
    return pfn_VixDiskLib_Clone(reinterpret_cast<const VixDiskLibConnection>(connection),
                              path,
                              reinterpret_cast<const VixDiskLibConnection>(srcConnection),
                              srcPath,
                              reinterpret_cast<const VixDiskLibCreateParams*>(createParams),
                              static_cast<VixDiskLibGenericProgressFunc>(progressFunc),
                              progressCallbackData,
                              doInflate);
}

VixError VixDiskLib_QueryAllocatedBlocksWrapper(VDDKHandle handle,
                                               VixDiskLibSectorType startSector,
                                               VixDiskLibSectorType numSectors,
                                               VDDKBlockList** blockList) {
    if (!pfn_VixDiskLib_QueryAllocatedBlocks) return VIX_E_FAIL;
    return pfn_VixDiskLib_QueryAllocatedBlocks(reinterpret_cast<VixDiskLibHandle>(handle),
                                            startSector,
                                            numSectors,
                                            reinterpret_cast<VixDiskLibBlockList**>(blockList));
}

void VixDiskLib_FreeBlockListWrapper(VDDKBlockList* blockList) {
    if (pfn_VixDiskLib_FreeBlockList) {
        pfn_VixDiskLib_FreeBlockList(reinterpret_cast<VixDiskLibBlockList*>(blockList));
    }
}

VixError VixDiskLib_ReadWrapper(VDDKHandle handle,
                               VixDiskLibSectorType startSector,
                               VixDiskLibSectorType numSectors,
                               uint8_t* buffer) {
    if (!pfn_VixDiskLib_Read) return VIX_E_FAIL;
    return pfn_VixDiskLib_Read(reinterpret_cast<VixDiskLibHandle>(handle),
                             startSector,
                             numSectors,
                             buffer);
}

VixError VixDiskLib_WriteWrapper(VDDKHandle handle,
                                VixDiskLibSectorType startSector,
                                VixDiskLibSectorType numSectors,
                                const uint8_t* buffer) {
    if (!pfn_VixDiskLib_Write) return VIX_E_FAIL;
    return pfn_VixDiskLib_Write(reinterpret_cast<VixDiskLibHandle>(handle),
                              startSector,
                              numSectors,
                              buffer);
}

char* VixDiskLib_GetErrorTextWrapper(VixError error, char* buffer, size_t bufferSize) {
    if (!pfn_VixDiskLib_GetErrorText) return nullptr;
    return pfn_VixDiskLib_GetErrorText(error, buffer, bufferSize);
}

void VixDiskLib_FreeErrorTextWrapper(char* errorText) {
    if (pfn_VixDiskLib_FreeErrorText) pfn_VixDiskLib_FreeErrorText(errorText);
}

} // extern "C" 