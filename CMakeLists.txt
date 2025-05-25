cmake_minimum_required(VERSION 3.10)

# Set CMake policies
cmake_policy(SET CMP0048 NEW)

project(GenieVM)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Enable testing
enable_testing()

# Set VDDK paths
set(VDDK_ROOT "/usr/local/vddk" CACHE PATH "Path to VDDK installation")
set(VDDK_LIB_DIR "${VDDK_ROOT}/lib64")

# Set RPATH settings to prioritize VDDK's libraries
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH TRUE)
set(CMAKE_INSTALL_RPATH "${VDDK_LIB_DIR}")
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)

# Set library search path
link_directories(${VDDK_LIB_DIR})

# Find required packages
find_package(CURL REQUIRED)
if(NOT CURL_FOUND)
    message(FATAL_ERROR "libcurl not found. Please install libcurl4-openssl-dev package.")
endif()

# Use system's libcurl
set(CURL_LIBRARIES "/usr/lib/x86_64-linux-gnu/libcurl.so")

find_package(nlohmann_json REQUIRED)
if(NOT nlohmann_json_FOUND)
    message(FATAL_ERROR "nlohmann/json not found. Please install nlohmann-json3-dev package.")
endif()

# Add CLI11
include(FetchContent)
FetchContent_Declare(
    CLI11
    GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
    GIT_TAG        v2.4.1
)
FetchContent_MakeAvailable(CLI11)

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
        ${VDDK_LIB_DIR}
    REQUIRED
)

# Find VDDK's dependencies
set(VDDK_DEPS
    "${VDDK_LIB_DIR}/libstdc++.so.6"
    "${VDDK_LIB_DIR}/libcrypto.so.1.0.2"
    "${VDDK_LIB_DIR}/libssl.so.1.0.2"
    "${VDDK_LIB_DIR}/libcurl.so.4"
    "${VDDK_LIB_DIR}/libxml2.so.2"
    "${VDDK_LIB_DIR}/libz.so.1"
    "${VDDK_LIB_DIR}/libcares.so.2"
)

# Check each VDDK dependency
foreach(dep ${VDDK_DEPS})
    if(NOT EXISTS ${dep})
        message(WARNING "VDDK dependency not found: ${dep}")
        list(REMOVE_ITEM VDDK_DEPS ${dep})
    endif()
endforeach()

# Find system OpenSSL 3.0
find_package(OpenSSL 3.0 REQUIRED)

# Include directories
include_directories(
    ${CMAKE_SOURCE_DIR}/include
    ${VDDK_INCLUDE_DIR}
    ${CURL_INCLUDE_DIRS}
    ${LIBVIRT_INCLUDE_DIRS}
    ${GTEST_INCLUDE_DIRS}
    ${OPENSSL_INCLUDE_DIR}
)

# Build VDDK wrapper as a shared library with old ABI
add_library(vddk_wrapper SHARED
    src/vddk_wrapper/vddk_wrapper.cpp
)

# Set include directories for VDDK wrapper
target_include_directories(vddk_wrapper
    PUBLIC
        ${CMAKE_SOURCE_DIR}/include
        ${VDDK_INCLUDE_DIR}
)

# Set old ABI for VDDK wrapper only
target_compile_definitions(vddk_wrapper PRIVATE _GLIBCXX_USE_CXX11_ABI=0)

# Link VDDK wrapper with VDDK's libraries
target_link_libraries(vddk_wrapper
    PRIVATE
    ${VDDK_DEPS}
    ${VDDK_LIBRARY}
    dl
    pthread
)

# Set RPATH for VDDK wrapper
set_target_properties(vddk_wrapper PROPERTIES
    INSTALL_RPATH "${VDDK_LIB_DIR}"
    BUILD_WITH_INSTALL_RPATH TRUE
    SKIP_BUILD_RPATH FALSE
    INSTALL_RPATH_USE_LINK_PATH TRUE
)

# Add source files for main executable
set(SOURCES
    # Main files
    src/main/main.cpp
    src/main/backup_main.cpp
    src/main/restore_main.cpp

    # Backup files
    src/backup/backup_cli.cpp
    src/backup/backup_job.cpp
    src/backup/backup_scheduler.cpp
    src/backup/backup_verifier.cpp
    src/backup/disk_backup.cpp
    src/backup/restore_job.cpp
    src/backup/job.cpp
    src/backup/job_manager.cpp
    src/backup/verify_job.cpp

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
        ${CLI11_INCLUDE_DIRS}
)

# Link libraries for main executable (using system libraries)
target_link_libraries(genievm
    PRIVATE
    vddk_wrapper
    ${CURL_LIBRARIES}
    ${LIBVIRT_LIBRARIES}
    nlohmann_json::nlohmann_json
    CLI11::CLI11
    pthread
    dl
    OpenSSL::SSL
    OpenSSL::Crypto
    z
    stdc++
    -static-libstdc++
    -static-libgcc
)

# Set RPATH for main executable
set_target_properties(genievm PROPERTIES
    INSTALL_RPATH "${VDDK_LIB_DIR}"
    BUILD_WITH_INSTALL_RPATH TRUE
    SKIP_BUILD_RPATH FALSE
    INSTALL_RPATH_USE_LINK_PATH TRUE
    LINK_FLAGS "-Wl,--no-as-needed"
)

# Install
install(TARGETS genievm vddk_wrapper DESTINATION bin)

