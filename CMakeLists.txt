project(vmware-backup-cbt VERSION 1.0.0 LANGUAGES CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# VDDK paths
set(VDDK_ROOT "/usr/local/vddk" CACHE PATH "Path to VDDK installation")
if(NOT EXISTS "${VDDK_ROOT}/include/vixDiskLib.h")
    message(FATAL_ERROR "VDDK include files not found at ${VDDK_ROOT}/include/vixDiskLib.h")
endif()

if(NOT EXISTS "${VDDK_ROOT}/lib64/libvixDiskLib.so")
    message(FATAL_ERROR "VDDK library not found at ${VDDK_ROOT}/lib64/libvixDiskLib.so")
endif()

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${VDDK_ROOT}/include
)

# Library directories
link_directories(
    ${VDDK_ROOT}/lib64
)

# Add source files
file(GLOB_RECURSE SOURCES 
    "src/*.cpp"
)

# Create libraries
add_library(vmware-backup-lib STATIC ${SOURCES})
add_library(vmware-restore-lib STATIC ${SOURCES})

# Create executables
add_executable(vmware-backup src/main/backup_main.cpp)
add_executable(vmware-restore src/main/restore_main.cpp)

# Set include directories for libraries
target_include_directories(vmware-backup-lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${VDDK_ROOT}/include
)

target_include_directories(vmware-restore-lib
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${VDDK_ROOT}/include
)

# Set include directories for executables
target_include_directories(vmware-backup
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${VDDK_ROOT}/include
)

target_include_directories(vmware-restore
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${VDDK_ROOT}/include
)

# Link libraries
target_link_libraries(vmware-backup-lib
    vixDiskLib
    pthread
    dl
    ssl
    crypto
    z
)

target_link_libraries(vmware-restore-lib
    vixDiskLib
    pthread
    dl
    ssl
    crypto
    z
)

target_link_libraries(vmware-backup
    vmware-backup-lib
)

target_link_libraries(vmware-restore
    vmware-restore-lib
) 