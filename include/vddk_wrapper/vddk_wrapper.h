#ifndef VDDK_WRAPPER_H
#define VDDK_WRAPPER_H

#include <vixDiskLib.h>

#ifdef __cplusplus
extern "C" {
#endif

// Wrapper functions that use C++11 internally but expose C-style interface
VixError VixDiskLib_InitWrapper(uint32_t majorVersion,
                               uint32_t minorVersion,
                               VixDiskLibConnectParams *connectParams,
                               const char *libDir,
                               const char *configFile);

VixError VixDiskLib_ConnectWrapper(const VixDiskLibConnectParams *connectParams,
                                  VixDiskLibConnection *connection);

VixError VixDiskLib_OpenWrapper(const VixDiskLibConnection connection,
                               const char *path,
                               uint32 flags,
                               VixDiskLibHandle *diskHandle);

VixError VixDiskLib_ReadWrapper(VixDiskLibHandle diskHandle,
                               VixDiskLibSectorType startSector,
                               VixDiskLibSectorType numSectors,
                               uint8 *readBuffer);

VixError VixDiskLib_WriteWrapper(VixDiskLibHandle diskHandle,
                                VixDiskLibSectorType startSector,
                                VixDiskLibSectorType numSectors,
                                const uint8 *writeBuffer);

VixError VixDiskLib_CloseWrapper(VixDiskLibHandle *diskHandle);

VixError VixDiskLib_DisconnectWrapper(VixDiskLibConnection *connection);

VixError VixDiskLib_ExitWrapper();

#ifdef __cplusplus
}
#endif

#endif // VDDK_WRAPPER_H 