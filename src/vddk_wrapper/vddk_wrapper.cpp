#include <dlfcn.h>
#include <string>
#include <memory>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <cstring>  // Add for strchr
#include <vixDiskLib.h>
#include "vddk_wrapper/vddk_wrapper.h"
#include "common/logger.hpp"  // Add Logger header

// Define progress function type since it's not in vixDiskLib.h
typedef void (*VixDiskLibGenericProgressFunc)(void* data, int percent);

// Function pointers for VDDK functions
static VixError (*pfn_VixDiskLib_Init)(uint32_t majorVersion, uint32_t minorVersion, 
                                      VixDiskLibGenericLogFunc* log, 
                                      VixDiskLibGenericLogFunc* warn, 
                                      VixDiskLibGenericLogFunc* panic, 
                                      const char* libDir) = nullptr;
static void (*pfn_VixDiskLib_Exit)(void) = nullptr;
static VixError (*pfn_VixDiskLib_Connect)(const VixDiskLibConnectParams* connectParams, VixDiskLibConnection* connection) = nullptr;
static VixError (*pfn_VixDiskLib_Disconnect)(VixDiskLibConnection connection) = nullptr;
static VixError (*pfn_VixDiskLib_Open)(const VixDiskLibConnection connection, const char* path, uint32_t flags, VixDiskLibHandle* handle) = nullptr;
static VixError (*pfn_VixDiskLib_Close)(VixDiskLibHandle handle) = nullptr;
static VixError (*pfn_VixDiskLib_GetInfo)(VixDiskLibHandle handle, VixDiskLibInfo** info) = nullptr;
static VixError (*pfn_VixDiskLib_FreeInfo)(VixDiskLibInfo* info) = nullptr;
static VixError (*pfn_VixDiskLib_Create)(const VixDiskLibConnection connection, const char* path, const VixDiskLibCreateParams* createParams, VixDiskLibProgressFunc progressFunc, void* progressCallbackData) = nullptr;
static VixError (*pfn_VixDiskLib_Clone)(const VixDiskLibConnection connection, const char* path, const VixDiskLibConnection srcConnection, const char* srcPath, const VixDiskLibCreateParams* createParams, VixDiskLibProgressFunc progressFunc, void* progressCallbackData, bool doInflate) = nullptr;
static VixError (*pfn_VixDiskLib_Read)(VixDiskLibHandle handle, VixDiskLibSectorType startSector, VixDiskLibSectorType numSectors, uint8_t* buffer) = nullptr;
static VixError (*pfn_VixDiskLib_Write)(VixDiskLibHandle handle, VixDiskLibSectorType startSector, VixDiskLibSectorType numSectors, const uint8_t* buffer) = nullptr;
static VixError (*pfn_VixDiskLib_QueryAllocatedBlocks)(VixDiskLibHandle handle, VixDiskLibSectorType startSector, VixDiskLibSectorType numSectors, VixDiskLibSectorType chunkSize, VixDiskLibBlockList** blockList) = nullptr;
static void (*pfn_VixDiskLib_FreeBlockList)(VixDiskLibBlockList* blockList) = nullptr;
static char* (*pfn_VixDiskLib_GetErrorText)(VixError error, const char* locale) = nullptr;
static void (*pfn_VixDiskLib_FreeErrorText)(char* errorText) = nullptr;

// Macro to load a function from the VDDK library
#define LOAD_FUNCTION(name) \
    pfn_##name = reinterpret_cast<decltype(pfn_##name)>(dlsym(vddkHandle, #name)); \
    if (!pfn_##name) { \
        Logger::error("Failed to load function " #name ": " + std::string(dlerror())); \
        dlclose(vddkHandle); \
        return false; \
    } \
    Logger::debug("Successfully loaded function: " #name)

// Load VDDK library
static bool loadVDDKLibrary() {
    static void* vddkHandle = nullptr;
    static bool loaded = false;

    if (loaded) {
        Logger::debug("VDDK library already loaded");
        return true;
    }

    // Try to load VDDK library
    Logger::debug("Attempting to load libvixDiskLib.so...");
    vddkHandle = dlopen("libvixDiskLib.so", RTLD_NOW | RTLD_DEEPBIND);
    if (!vddkHandle) {
        Logger::error("Failed to load VDDK library: " + std::string(dlerror()));
        return false;
    }
    Logger::debug("Successfully loaded libvixDiskLib.so");

    // Load function pointers
    LOAD_FUNCTION(VixDiskLib_Init);
    LOAD_FUNCTION(VixDiskLib_Exit);
    LOAD_FUNCTION(VixDiskLib_Connect);
    LOAD_FUNCTION(VixDiskLib_Disconnect);
    LOAD_FUNCTION(VixDiskLib_Open);
    LOAD_FUNCTION(VixDiskLib_Close);
    LOAD_FUNCTION(VixDiskLib_GetInfo);
    LOAD_FUNCTION(VixDiskLib_FreeInfo);
    LOAD_FUNCTION(VixDiskLib_Create);
    LOAD_FUNCTION(VixDiskLib_Clone);
    LOAD_FUNCTION(VixDiskLib_Read);
    LOAD_FUNCTION(VixDiskLib_Write);
    LOAD_FUNCTION(VixDiskLib_QueryAllocatedBlocks);
    LOAD_FUNCTION(VixDiskLib_FreeBlockList);
    LOAD_FUNCTION(VixDiskLib_GetErrorText);
    LOAD_FUNCTION(VixDiskLib_FreeErrorText);

    loaded = true;
    Logger::debug("All VDDK functions loaded successfully");
    return true;
}

// Helper function to get error text
std::string getVixErrorText(VixError error) {
    char* errorText = nullptr;
    if (pfn_VixDiskLib_GetErrorText) {
        errorText = pfn_VixDiskLib_GetErrorText(error, nullptr);
    }
    std::string result(errorText ? errorText : "Unknown error");
    if (errorText && pfn_VixDiskLib_FreeErrorText) {
        pfn_VixDiskLib_FreeErrorText(errorText);
    }
    return result;
}

// Helper function to log VDDK errors
void logVixError(const std::string& operation, VixError error) {
    std::cerr << "Error: " << operation << " failed: " << getVixErrorText(error) << std::endl;
}

// Initialize VDDK
VixError VixDiskLib_InitWrapper(uint32_t majorVersion, uint32_t minorVersion, void* unused) {
    Logger::debug("Starting VixDiskLib_InitWrapper...");
    Logger::debug("Major version: " + std::to_string(majorVersion));
    Logger::debug("Minor version: " + std::to_string(minorVersion));
    
    
    Logger::debug("Calling VixDiskLib_Init directly...");
    try {
        //VixError error = VixDiskLib_Init(majorVersion, minorVersion, 
        //                               nullptr,  // log callback
        //                               nullptr,  // warning callback
        //                               nullptr,  // panic callback
        //
        VixError error = VixDiskLib_InitEx(6, 8, 
                                        nullptr,  // log callback
                                        nullptr,  // warning callback
                                        nullptr,  // panic callback
                                        "/usr/local/vddk/lib64",
                                        nullptr);  // libDir
        if (error != VIX_OK) {
            Logger::error("VixDiskLib_Init failed with error: " + std::to_string(error));
            char* errorMsg = VixDiskLib_GetErrorText(error, nullptr);
            if (errorMsg) {
                Logger::error("Error details: " + std::string(errorMsg));
                VixDiskLib_FreeErrorText(errorMsg);
            }
        } else {
            Logger::debug("VixDiskLib_Init completed successfully");
        }
        return error;
    } catch (const std::exception& e) {
        Logger::error("Exception during VixDiskLib_Init: " + std::string(e.what()));
        return VIX_E_FAIL;
    } catch (...) {
        Logger::error("Unknown exception during VixDiskLib_Init");
        return VIX_E_FAIL;
    }
}

// Exit VDDK
void VixDiskLib_ExitWrapper() {
    VixDiskLib_Exit();
}

// Connect to vCenter
VixError VixDiskLib_ConnectWrapper(const VixDiskLibConnectParams* connectParams, VDDKConnection* connection) {
    return VixDiskLib_Connect(connectParams, connection);
}

// Disconnect from vCenter
VixError VixDiskLib_DisconnectWrapper(VDDKConnection* connection) {
    return VixDiskLib_Disconnect(*connection);
}

// Open a disk
VixError VixDiskLib_OpenWrapper(const VDDKConnection connection, const char* path, uint32_t flags, VDDKHandle* handle) {
    return VixDiskLib_Open(connection, path, flags, handle);
}

// Close a disk
VixError VixDiskLib_CloseWrapper(VDDKHandle* handle) {
    return VixDiskLib_Close(*handle);
}

// Get disk info
VixError VixDiskLib_GetInfoWrapper(VDDKHandle handle, VDDKInfo** info) {
    return VixDiskLib_GetInfo(handle, info);
}

// Free disk info
VixError VixDiskLib_FreeInfoWrapper(VDDKInfo* info) {
    VixDiskLib_FreeInfo(info);
    return VIX_OK;
}

// Create a disk
VixError VixDiskLib_CreateWrapper(const VDDKConnection connection, 
                                 const char* path, 
                                 const VDDKCreateParams* createParams, 
                                 VixDiskLibProgressFunc progressFunc, 
                                 void* progressCallbackData) {
    return VixDiskLib_Create(connection, path, createParams, progressFunc, progressCallbackData);
}

// Clone a disk
VixError VixDiskLib_CloneWrapper(const VDDKConnection connection, 
                                const char* path, 
                                const VDDKConnection srcConnection, 
                                const char* srcPath, 
                                const VDDKCreateParams* createParams, 
                                VixDiskLibProgressFunc progressFunc, 
                                void* progressCallbackData, 
                                bool doInflate) {
    return VixDiskLib_Clone(connection, path, srcConnection, srcPath, createParams, progressFunc, progressCallbackData, doInflate);
}

// Read from a disk
VixError VixDiskLib_ReadWrapper(VDDKHandle handle, 
                               VixDiskLibSectorType startSector, 
                               VixDiskLibSectorType numSectors, 
                               uint8_t* buffer) {
    return VixDiskLib_Read(handle, startSector, numSectors, buffer);
}

// Write to a disk
VixError VixDiskLib_WriteWrapper(VDDKHandle handle, 
                                VixDiskLibSectorType startSector, 
                                VixDiskLibSectorType numSectors, 
                                const uint8_t* buffer) {
    return VixDiskLib_Write(handle, startSector, numSectors, buffer);
}

// Query allocated blocks
VixError VixDiskLib_QueryAllocatedBlocksWrapper(VDDKHandle handle, 
                                               VixDiskLibSectorType startSector, 
                                               VixDiskLibSectorType numSectors, 
                                               VDDKBlockList** blockList) {
    return VixDiskLib_QueryAllocatedBlocks(handle, startSector, numSectors, 0, blockList);
}

// Free block list
void VixDiskLib_FreeBlockListWrapper(VDDKBlockList* blockList) {
    VixDiskLib_FreeBlockList(blockList);
}

// Get error text
char* VixDiskLib_GetErrorTextWrapper(VixError error, char* buffer, size_t bufferSize) {
    return VixDiskLib_GetErrorText(error, nullptr);
}

// Free error text
void VixDiskLib_FreeErrorTextWrapper(char* errorText) {
    VixDiskLib_FreeErrorText(errorText);
} 