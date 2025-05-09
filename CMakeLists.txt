cmake_minimum_required(VERSION 3.10)
project(vmware-backup-cbt VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find required packages
find_package(OpenSSL REQUIRED)

# VDDK paths - these should be set by the user
set(VDDK_ROOT "" CACHE PATH "Path to VDDK installation")
if(NOT VDDK_ROOT)
    message(FATAL_ERROR "Please set VDDK_ROOT to the VDDK installation directory")
endif()

# vSphere SDK paths
set(VSPHERE_SDK_ROOT "" CACHE PATH "Path to vSphere SDK installation")
if(NOT VSPHERE_SDK_ROOT)
    message(FATAL_ERROR "Please set VSPHERE_SDK_ROOT to the vSphere SDK installation directory")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${VDDK_ROOT}/include
    ${VSPHERE_SDK_ROOT}/include
    ${OPENSSL_INCLUDE_DIR}
)

# Link directories
link_directories(
    ${VDDK_ROOT}/lib64
    ${VSPHERE_SDK_ROOT}/lib64
)

# Add subdirectories
add_subdirectory(src)

# Add executables
add_executable(vmware-backup src/main/backup_main.cpp)
add_executable(vmware-restore src/main/restore_main.cpp)

# Link libraries
target_link_libraries(vmware-backup
    PRIVATE
    vmware-backup-lib
    vixDiskLib
    vim25
    ssl
    crypto
)

target_link_libraries(vmware-restore
    PRIVATE
    vmware-restore-lib
    vixDiskLib
    vim25
    ssl
    crypto
) 