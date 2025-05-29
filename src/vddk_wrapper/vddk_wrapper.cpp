#include <dlfcn.h>
#include <string>
#include <memory>
#include <vector>
#include <stdexcept>
#include <iostream>
#include <cstring>  // Add for strchr
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

// Load VDDK library
static bool loadVDDKLibrary() {
    static void* vddkHandle = nullptr;
    static bool loaded = false;

    if (loaded) {
        return true;
    }

    // Set OpenSSL FIPS mode environment variable
    setenv("OPENSSL_FIPS", "1", 1);

    // Try to load VDDK library
    vddkHandle = dlopen("libvixDiskLib.so", RTLD_NOW | RTLD_DEEPBIND);
    if (!vddkHandle) {
        std::cerr << "Failed to load VDDK library: " << dlerror() << std::endl;
        return false;
    }

    // Load function pointers
    #define LOAD_FUNCTION(name) \
        pfn_##name = reinterpret_cast<decltype(pfn_##name)>(dlsym(vddkHandle, #name)); \
        if (!pfn_##name) { \
            std::cerr << "Failed to load function " #name ": " << dlerror() << std::endl; \
            dlclose(vddkHandle); \
            return false; \
        }

    LOAD_FUNCTION(VixDiskLib_Init)
    LOAD_FUNCTION(VixDiskLib_Exit)
    LOAD_FUNCTION(VixDiskLib_Connect)
    LOAD_FUNCTION(VixDiskLib_Disconnect)
    LOAD_FUNCTION(VixDiskLib_Open)
    LOAD_FUNCTION(VixDiskLib_Close)
    LOAD_FUNCTION(VixDiskLib_GetInfo)
    LOAD_FUNCTION(VixDiskLib_FreeInfo)
    LOAD_FUNCTION(VixDiskLib_Create)
    LOAD_FUNCTION(VixDiskLib_Clone)
    LOAD_FUNCTION(VixDiskLib_Read)
    LOAD_FUNCTION(VixDiskLib_Write)
    LOAD_FUNCTION(VixDiskLib_QueryAllocatedBlocks)
    LOAD_FUNCTION(VixDiskLib_FreeBlockList)
    LOAD_FUNCTION(VixDiskLib_GetErrorText)
    LOAD_FUNCTION(VixDiskLib_FreeErrorText)

    loaded = true;
    return true;
}

// Helper function to get error text
std::string getVixErrorText(VixError error) {
    char* errorText = nullptr;
    if (pfn_VixDiskLib_GetErrorText) {
        errorText = pfn_VixDiskLib_GetErrorText(error, nullptr, 0);
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
    if (!loadVDDKLibrary()) {
        std::cerr << "Error: Failed to load VDDK library" << std::endl;
        return VIX_E_FAIL;
    }
    
    VixError error = pfn_VixDiskLib_Init(majorVersion, minorVersion, nullptr, nullptr, nullptr, nullptr);
    if (error != VIX_OK) {
        logVixError("VixDiskLib_Init", error);
    }
    return error;
}

// Exit VDDK
void VixDiskLib_ExitWrapper() {
    if (pfn_VixDiskLib_Exit) {
        pfn_VixDiskLib_Exit();
    }
}

// Connect to vCenter
VixError VixDiskLib_ConnectWrapper(const VixDiskLibConnectParams* connectParams, VDDKConnection* connection) {
    if (!pfn_VixDiskLib_Connect) {
        std::cerr << "Error: VDDK library not loaded" << std::endl;
        return VIX_E_FAIL;
    }
    
    VixError error = pfn_VixDiskLib_Connect(connectParams, connection);
    if (error != VIX_OK) {
        logVixError("VixDiskLib_Connect", error);
    }
    return error;
}

// Disconnect from vCenter
VixError VixDiskLib_DisconnectWrapper(VDDKConnection* connection) {
    if (!pfn_VixDiskLib_Disconnect) {
        std::cerr << "Error: VDDK library not loaded" << std::endl;
        return VIX_E_FAIL;
    }
    
    VixError error = pfn_VixDiskLib_Disconnect(*connection);
    if (error != VIX_OK) {
        logVixError("VixDiskLib_Disconnect", error);
    }
    return error;
}

// Open a disk
VixError VixDiskLib_OpenWrapper(const VDDKConnection connection, const char* path, uint32_t flags, VDDKHandle* handle) {
    if (!pfn_VixDiskLib_Open) {
        std::cerr << "Error: VDDK library not loaded" << std::endl;
        return VIX_E_FAIL;
    }
    
    std::cerr << "Debug: Opening disk with path: '" << path << "'" << std::endl;
    std::cerr << "Debug: Using flags: 0x" << std::hex << flags << std::dec << std::endl;
    std::cerr << "Debug: Connection handle: " << connection << std::endl;
    
    // Validate path format
    if (!path || path[0] == '\0') {
        std::cerr << "Error: Invalid disk path (null or empty)" << std::endl;
        return VIX_E_INVALID_ARG;
    }
    
    // Check if path starts with '[' and contains ']'
    if (path[0] != '[' || strchr(path, ']') == nullptr) {
        std::cerr << "Error: Invalid disk path format. Expected format: [datastore] path/to/vmdk" << std::endl;
        return VIX_E_INVALID_ARG;
    }
    
    VixError error = pfn_VixDiskLib_Open(connection, path, flags, handle);
    if (error != VIX_OK) {
        std::cerr << "Error: Failed to open disk. Error code: " << error << std::endl;
        logVixError("VixDiskLib_Open", error);
        
        // Try to get more detailed error information
        char* errorMsg = VixDiskLib_GetErrorText(error, nullptr);
        if (errorMsg) {
            std::cerr << "Error details: " << errorMsg << std::endl;
            VixDiskLib_FreeErrorText(errorMsg);
        }
    } else {
        std::cerr << "Debug: Successfully opened disk" << std::endl;
    }
    return error;
}

// Close a disk
VixError VixDiskLib_CloseWrapper(VDDKHandle* handle) {
    if (!pfn_VixDiskLib_Close) {
        std::cerr << "Error: VDDK library not loaded" << std::endl;
        return VIX_E_FAIL;
    }
    
    VixError error = pfn_VixDiskLib_Close(*handle);
    if (error != VIX_OK) {
        logVixError("VixDiskLib_Close", error);
    }
    return error;
}

// Get disk info
VixError VixDiskLib_GetInfoWrapper(VDDKHandle handle, VDDKInfo** info) {
    if (!pfn_VixDiskLib_GetInfo) {
        std::cerr << "Error: VDDK library not loaded" << std::endl;
        return VIX_E_FAIL;
    }
    
    VixError error = pfn_VixDiskLib_GetInfo(handle, info);
    if (error != VIX_OK) {
        logVixError("VixDiskLib_GetInfo", error);
    }
    return error;
}

// Free disk info
VixError VixDiskLib_FreeInfoWrapper(VDDKInfo* info) {
    if (!pfn_VixDiskLib_FreeInfo) {
        std::cerr << "Error: VDDK library not loaded" << std::endl;
        return VIX_E_FAIL;
    }
    
    VixError error = pfn_VixDiskLib_FreeInfo(info);
    if (error != VIX_OK) {
        logVixError("VixDiskLib_FreeInfo", error);
    }
    return error;
}

// Create a disk
VixError VixDiskLib_CreateWrapper(const VDDKConnection connection, const char* path, const VDDKCreateParams* createParams, void (*progressFunc)(void* data, int percent), void* progressCallbackData) {
    if (!pfn_VixDiskLib_Create) {
        std::cerr << "Error: VDDK library not loaded" << std::endl;
        return VIX_E_FAIL;
    }
    
    std::cerr << "Debug: Creating disk: " << path << std::endl;
    VixError error = pfn_VixDiskLib_Create(connection, path, createParams, progressFunc, progressCallbackData);
    if (error != VIX_OK) {
        logVixError("VixDiskLib_Create", error);
    }
    return error;
}

// Clone a disk
VixError VixDiskLib_CloneWrapper(const VDDKConnection connection, const char* path, const VDDKConnection srcConnection, const char* srcPath, const VDDKCreateParams* createParams, void (*progressFunc)(void* data, int percent), void* progressCallbackData, bool doInflate) {
    if (!pfn_VixDiskLib_Clone) {
        std::cerr << "Error: VDDK library not loaded" << std::endl;
        return VIX_E_FAIL;
    }
    
    std::cerr << "Debug: Cloning disk from " << srcPath << " to " << path << std::endl;
    VixError error = pfn_VixDiskLib_Clone(connection, path, srcConnection, srcPath, createParams, progressFunc, progressCallbackData, doInflate);
    if (error != VIX_OK) {
        logVixError("VixDiskLib_Clone", error);
    }
    return error;
}

// Read from a disk
VixError VixDiskLib_ReadWrapper(VDDKHandle handle, VixDiskLibSectorType startSector, VixDiskLibSectorType numSectors, uint8_t* buffer) {
    if (!pfn_VixDiskLib_Read) {
        std::cerr << "Error: VDDK library not loaded" << std::endl;
        return VIX_E_FAIL;
    }
    
    VixError error = pfn_VixDiskLib_Read(handle, startSector, numSectors, buffer);
    if (error != VIX_OK) {
        logVixError("VixDiskLib_Read", error);
    }
    return error;
}

// Write to a disk
VixError VixDiskLib_WriteWrapper(VDDKHandle handle, VixDiskLibSectorType startSector, VixDiskLibSectorType numSectors, const uint8_t* buffer) {
    if (!pfn_VixDiskLib_Write) {
        std::cerr << "Error: VDDK library not loaded" << std::endl;
        return VIX_E_FAIL;
    }
    
    VixError error = pfn_VixDiskLib_Write(handle, startSector, numSectors, buffer);
    if (error != VIX_OK) {
        logVixError("VixDiskLib_Write", error);
    }
    return error;
}

// Query allocated blocks
VixError VixDiskLib_QueryAllocatedBlocksWrapper(VDDKHandle handle, VixDiskLibSectorType startSector, VixDiskLibSectorType numSectors, VDDKBlockList** blockList) {
    if (!pfn_VixDiskLib_QueryAllocatedBlocks) {
        std::cerr << "Error: VDDK library not loaded" << std::endl;
        return VIX_E_FAIL;
    }
    
    VixError error = pfn_VixDiskLib_QueryAllocatedBlocks(handle, startSector, numSectors, blockList);
    if (error != VIX_OK) {
        logVixError("VixDiskLib_QueryAllocatedBlocks", error);
    }
    return error;
}

// Free block list
void VixDiskLib_FreeBlockListWrapper(VDDKBlockList* blockList) {
    if (pfn_VixDiskLib_FreeBlockList) {
        pfn_VixDiskLib_FreeBlockList(blockList);
    }
}

// Get error text
char* VixDiskLib_GetErrorTextWrapper(VixError error, char* buffer, size_t bufferSize) {
    if (!pfn_VixDiskLib_GetErrorText) {
        std::cerr << "Error: VDDK library not loaded" << std::endl;
        return nullptr;
    }
    return pfn_VixDiskLib_GetErrorText(error, buffer, bufferSize);
}

// Free error text
void VixDiskLib_FreeErrorTextWrapper(char* errorText) {
    if (pfn_VixDiskLib_FreeErrorText) {
        pfn_VixDiskLib_FreeErrorText(errorText);
    }
} 