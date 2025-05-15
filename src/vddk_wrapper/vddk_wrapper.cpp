#include "vddk_wrapper/vddk_wrapper.h"
#include <vixDiskLib.h>
#include <string>
#include <memory>

extern "C" {

// Wrapper functions that use C++11 internally but expose C-style interface
VixError VixDiskLib_InitWrapper(uint32_t majorVersion,
                               uint32_t minorVersion,
                               VixDiskLibConnectParams *connectParams,
                               const char *libDir,
                               const char *configFile) {
    return VixDiskLib_Init(majorVersion, minorVersion, 
                          nullptr,  // log function
                          nullptr,  // warn function
                          nullptr,  // panic function
                          libDir);
}

VixError VixDiskLib_ConnectWrapper(const VixDiskLibConnectParams *connectParams,
                                  VixDiskLibConnection *connection) {
    return VixDiskLib_Connect(connectParams, connection);
}

VixError VixDiskLib_OpenWrapper(const VixDiskLibConnection connection,
                               const char *path,
                               uint32 flags,
                               VixDiskLibHandle *diskHandle) {
    return VixDiskLib_Open(connection, path, flags, diskHandle);
}

VixError VixDiskLib_ReadWrapper(VixDiskLibHandle diskHandle,
                               VixDiskLibSectorType startSector,
                               VixDiskLibSectorType numSectors,
                               uint8 *readBuffer) {
    return VixDiskLib_Read(diskHandle, startSector, numSectors, readBuffer);
}

VixError VixDiskLib_WriteWrapper(VixDiskLibHandle diskHandle,
                                VixDiskLibSectorType startSector,
                                VixDiskLibSectorType numSectors,
                                const uint8 *writeBuffer) {
    return VixDiskLib_Write(diskHandle, startSector, numSectors, writeBuffer);
}

VixError VixDiskLib_CloseWrapper(VixDiskLibHandle *diskHandle) {
    VixError err = VixDiskLib_Close(*diskHandle);
    if (err == VIX_OK) {
        *diskHandle = nullptr;
    }
    return err;
}

VixError VixDiskLib_DisconnectWrapper(VixDiskLibConnection *connection) {
    VixError err = VixDiskLib_Disconnect(*connection);
    if (err == VIX_OK) {
        *connection = nullptr;
    }
    return err;
}

void VixDiskLib_ExitWrapper() {
    VixDiskLib_Exit();
}

} // extern "C" 