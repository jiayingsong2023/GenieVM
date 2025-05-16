cmake_minimum_required(VERSION 3.10)

# Set CMake policies
cmake_policy(SET CMP0048 NEW)

project(GenieVM)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Ensure ABI compatibility
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -D_GLIBCXX_USE_CXX11_ABI=0")

# Enable testing
enable_testing()

# Find required packages
find_package(CURL REQUIRED)
if(NOT CURL_FOUND)
    message(FATAL_ERROR "libcurl not found. Please install libcurl4-openssl-dev package.")
endif()

find_package(nlohmann_json REQUIRED)
if(NOT nlohmann_json_FOUND)
    message(FATAL_ERROR "nlohmann/json not found. Please install nlohmann-json3-dev package.")
endif()

# Find libvirt
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBVIRT REQUIRED libvirt)
if(NOT LIBVIRT_FOUND)
    message(FATAL_ERROR "libvirt not found. Please install libvirt-dev package.")
endif()

# Find GTest
find_package(GTest REQUIRED)
if(NOT GTest_FOUND)
    message(FATAL_ERROR "Google Test not found. Please install googletest package.")
endif()

# Set VDDK paths
set(VDDK_ROOT "/usr/local/vddk" CACHE PATH "Path to VDDK installation")

# Find VDDK
find_path(VDDK_INCLUDE_DIR
    NAMES vixDiskLib.h
    PATHS
        ${VDDK_ROOT}/include
    REQUIRED
)

find_library(VDDK_LIBRARY
    NAMES vixDiskLib
    PATHS
        ${VDDK_ROOT}/lib64
    REQUIRED
)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${VDDK_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${LIBVIRT_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS}
)

# Library directories
link_directories(
    ${VDDK_INCLUDE_DIR}
    ${LIBVIRT_LIBRARY_DIRS}
)

# Add source files
set(SOURCES
    # Main files
    src/main/main.cpp
    src/main/backup_main.cpp
    src/main/restore_main.cpp

    # Backup files
    src/backup/backup_cli.cpp
    src/backup/backup_job.cpp
    src/backup/backup_manager.cpp
    src/backup/backup_scheduler.cpp
    src/backup/backup_verifier.cpp
    src/backup/disk_backup.cpp
    src/backup/restore_job.cpp

    # Backup KVM files
    src/backup/kvm/cbt_factory.cpp
    src/backup/kvm/kvm_backup_provider.cpp
    src/backup/kvm/lvm_cbt.cpp
    src/backup/kvm/qcow2_cbt.cpp
    src/backup/kvm/storage_detector.cpp

    # Backup VMware files
    src/backup/vmware/vmware_backup_provider.cpp

    # Common files
    src/common/logger.cpp
    src/common/parallel_task_manager.cpp
    src/common/scheduler.cpp
    src/common/vmware_connection.cpp
    src/common/vsphere_manager.cpp
    src/common/vsphere_rest_client.cpp

    # Restore files
    src/restore/disk_restore.cpp
    src/restore/restore_manager.cpp

    # VDDK wrapper files
    src/vddk_wrapper/vddk_wrapper.cpp
)

# Create executable
add_executable(genievm ${SOURCES})

# Set include directories for executables
target_include_directories(genievm
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${VDDK_INCLUDE_DIR}
        ${CURL_INCLUDE_DIRS}
        ${LIBVIRT_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(genievm
    ${VDDK_LIBRARY}
    pthread
    dl
    ssl
    crypto
    z
    /usr/lib/x86_64-linux-gnu/libcurl.so.4
    ${LIBVIRT_LIBRARIES}
    nlohmann_json::nlohmann_json
)

# Set RPATH
set_target_properties(genievm PROPERTIES
    INSTALL_RPATH "/usr/lib/x86_64-linux-gnu:${VDDK_ROOT}/lib64"
    BUILD_WITH_INSTALL_RPATH TRUE
    SKIP_BUILD_RPATH FALSE
    LINK_FLAGS "-Wl,--no-as-needed -Wl,--exclude-libs,libvixDiskLib.so -Wl,--disable-new-dtags -Wl,--exclude-libs,libcurl.so.4"
)

# Install
install(TARGETS genievm DESTINATION bin)

# Install VMware SDK libraries
install(FILES ${VDDK_LIBRARY} DESTINATION lib)

# Install VMware SDK include files
install(DIRECTORY ${VDDK_INCLUDE_DIR}/ DESTINATION include)

# Install VMware SDK include files
install(FILES ${CMAKE_SOURCE_DIR}/third_party/vmware-sdk/include/vixDiskLib.h DESTINATION include)