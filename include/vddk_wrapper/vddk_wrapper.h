#ifndef VDDK_WRAPPER_H
#define VDDK_WRAPPER_H

#include <cstdint>
#include <cstddef>
#include <vixDiskLib.h>

#ifdef __cplusplus
extern "C" {
#endif

// Type aliases for VDDK types
typedef VixDiskLibConnection VDDKConnection;
typedef VixDiskLibHandle VDDKHandle;
typedef VixDiskLibInfo VDDKInfo;
typedef VixDiskLibBlockList VDDKBlockList;
typedef VixDiskLibConnectParams VDDKConnectParams;
typedef VixDiskLibCreateParams VDDKCreateParams;
typedef VixError VixError;

// Error codes
#define VIX_OK 0
#define VIX_E_FAIL 1

// Only define these if they haven't been defined by vixDiskLib.h
#ifndef VIX_FAILED
#define VIX_FAILED(x) ((x) != VIX_OK)
#endif

// Only define these if they haven't been defined by vixDiskLib.h
#ifndef VIXDISKLIB_FLAG_OPEN_READ_ONLY
#define VIXDISKLIB_FLAG_OPEN_READ_ONLY (1 << 2)
#endif

#ifndef VIXDISKLIB_FLAG_OPEN_UNBUFFERED
#define VIXDISKLIB_FLAG_OPEN_UNBUFFERED (1 << 0)
#endif

#ifndef VIXDISKLIB_FLAG_OPEN_SINGLE_LINK
#define VIXDISKLIB_FLAG_OPEN_SINGLE_LINK (1 << 1)
#endif

#ifndef VIXDISKLIB_FLAG_OPEN_USE_SAN
#define VIXDISKLIB_FLAG_OPEN_USE_SAN (1 << 3)
#endif

#ifndef VIXDISKLIB_FLAG_OPEN_USE_VMFS
#define VIXDISKLIB_FLAG_OPEN_USE_VMFS (1 << 4)
#endif

#ifndef VIXDISKLIB_FLAG_OPEN_USE_VSOCK
#define VIXDISKLIB_FLAG_OPEN_USE_VSOCK (1 << 5)
#endif

// VDDK disk types
#ifndef VIXDISKLIB_DISK_MONOLITHIC_SPARSE
#define VIXDISKLIB_DISK_MONOLITHIC_SPARSE 0
#endif

#ifndef VIXDISKLIB_DISK_MONOLITHIC_FLAT
#define VIXDISKLIB_DISK_MONOLITHIC_FLAT 1
#endif

#ifndef VIXDISKLIB_DISK_SPLIT_SPARSE
#define VIXDISKLIB_DISK_SPLIT_SPARSE 2
#endif

#ifndef VIXDISKLIB_DISK_SPLIT_FLAT
#define VIXDISKLIB_DISK_SPLIT_FLAT 3
#endif

#ifndef VIXDISKLIB_DISK_VMFS_FLAT
#define VIXDISKLIB_DISK_VMFS_FLAT 4
#endif

#ifndef VIXDISKLIB_DISK_VMFS_SPARSE
#define VIXDISKLIB_DISK_VMFS_SPARSE 5
#endif

#ifndef VIXDISKLIB_DISK_VMFS_RDM
#define VIXDISKLIB_DISK_VMFS_RDM 6
#endif

#ifndef VIXDISKLIB_DISK_VMFS_PASSTHRU_RAW
#define VIXDISKLIB_DISK_VMFS_PASSTHRU_RAW 7
#endif

#ifndef VIXDISKLIB_DISK_STREAM_OPTIMIZED
#define VIXDISKLIB_DISK_STREAM_OPTIMIZED 8
#endif

#ifndef VIXDISKLIB_DISK_SESPARSE
#define VIXDISKLIB_DISK_SESPARSE 9
#endif

// VDDK adapter types
#ifndef VIXDISKLIB_ADAPTER_IDE
#define VIXDISKLIB_ADAPTER_IDE 0
#endif

#ifndef VIXDISKLIB_ADAPTER_SCSI_BUSLOGIC
#define VIXDISKLIB_ADAPTER_SCSI_BUSLOGIC 1
#endif

#ifndef VIXDISKLIB_ADAPTER_SCSI_LSILOGIC
#define VIXDISKLIB_ADAPTER_SCSI_LSILOGIC 2
#endif

#ifndef VIXDISKLIB_ADAPTER_SCSI_LSISAS
#define VIXDISKLIB_ADAPTER_SCSI_LSISAS 3
#endif

#ifndef VIXDISKLIB_ADAPTER_SCSI_PVSCSI
#define VIXDISKLIB_ADAPTER_SCSI_PVSCSI 4
#endif

// VDDK hardware versions
#ifndef VIXDISKLIB_HWVERSION_WORKSTATION_5
#define VIXDISKLIB_HWVERSION_WORKSTATION_5 5
#endif

#define VIXDISKLIB_HWVERSION_WORKSTATION_6 6
#define VIXDISKLIB_HWVERSION_WORKSTATION_7 7
#define VIXDISKLIB_HWVERSION_WORKSTATION_8 8
#define VIXDISKLIB_HWVERSION_WORKSTATION_9 9
#define VIXDISKLIB_HWVERSION_WORKSTATION_10 10
#define VIXDISKLIB_HWVERSION_WORKSTATION_11 11
#define VIXDISKLIB_HWVERSION_WORKSTATION_12 12
#define VIXDISKLIB_HWVERSION_WORKSTATION_14 14
#define VIXDISKLIB_HWVERSION_WORKSTATION_15 15
#define VIXDISKLIB_HWVERSION_WORKSTATION_16 16
#define VIXDISKLIB_HWVERSION_WORKSTATION_17 17
#define VIXDISKLIB_HWVERSION_WORKSTATION_18 18
#define VIXDISKLIB_HWVERSION_WORKSTATION_19 19
#define VIXDISKLIB_HWVERSION_WORKSTATION_20 20

// VDDK constants
#define VIXDISKLIB_SECTOR_SIZE 512
#define VIXDISKLIB_MIN_SECTOR_NUMBER 0
#define VIXDISKLIB_MAX_SECTOR_NUMBER 0xFFFFFFFFFFFFFFFFULL

// Boolean constants
#define TRUE 1
#define FALSE 0

// Function declarations
VixError VixDiskLib_InitWrapper(uint32_t majorVersion,
                               uint32_t minorVersion,
                               const char* configFile);

void VixDiskLib_ExitWrapper();

VixError VixDiskLib_ConnectWrapper(const VDDKConnectParams* connectParams,
                                  VDDKConnection* connection);

VixError VixDiskLib_DisconnectWrapper(VDDKConnection* connection);

VixError VixDiskLib_OpenWrapper(const VDDKConnection connection,
                               const char* path,
                               uint32_t flags,
                               VDDKHandle* handle);

VixError VixDiskLib_CloseWrapper(VDDKHandle* handle);

VixError VixDiskLib_GetInfoWrapper(VDDKHandle handle,
                                  VDDKInfo** info);

VixError VixDiskLib_FreeInfoWrapper(VDDKInfo* info);

VixError VixDiskLib_CreateWrapper(const VDDKConnection connection,
                                 const char* path,
                                 const VDDKCreateParams* createParams,
                                 void (*progressFunc)(void* data, int percent),
                                 void* progressCallbackData);

VixError VixDiskLib_CloneWrapper(const VDDKConnection connection,
                                const char* path,
                                const VDDKConnection srcConnection,
                                const char* srcPath,
                                const VDDKCreateParams* createParams,
                                void (*progressFunc)(void* data, int percent),
                                void* progressCallbackData,
                                bool doInflate);

VixError VixDiskLib_QueryAllocatedBlocksWrapper(VDDKHandle handle,
                                               VixDiskLibSectorType startSector,
                                               VixDiskLibSectorType numSectors,
                                               VDDKBlockList** blockList);

void VixDiskLib_FreeBlockListWrapper(VDDKBlockList* blockList);

VixError VixDiskLib_ReadWrapper(VDDKHandle handle,
                               VixDiskLibSectorType startSector,
                               VixDiskLibSectorType numSectors,
                               uint8_t* buffer);

VixError VixDiskLib_WriteWrapper(VDDKHandle handle,
                                VixDiskLibSectorType startSector,
                                VixDiskLibSectorType numSectors,
                                const uint8_t* buffer);

char* VixDiskLib_GetErrorTextWrapper(VixError error, char* buffer, size_t bufferSize);

void VixDiskLib_FreeErrorTextWrapper(char* errorText);

#ifdef __cplusplus
}
#endif

#endif // VDDK_WRAPPER_H 