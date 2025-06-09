add_library(vmware-backup-lib
    backup/vmware/vmware_backup_provider.cpp
    backup/kvm/kvm_backup_provider.cpp
    backup/backup_provider_factory.cpp
    common/vmware_connection.cpp
    common/logger.cpp
    common/job_manager.cpp
    backup/backup_job.cpp
    backup/verify_job.cpp
    restore/restore_job.cpp
    common/parallel_task_manager.cpp
)

add_library(vmware-restore-lib
    common/vmware_connection.cpp
    common/logger.cpp
    common/parallel_task_manager.cpp
)

target_include_directories(vmware-backup-lib
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_include_directories(vmware-restore-lib
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# Add dependencies
target_link_libraries(vmware-backup-lib
    PRIVATE
    vddk_wrapper
    pthread
    dl
    ssl
    crypto
    z
)

target_link_libraries(vmware-restore-lib
    PRIVATE
    vddk_wrapper
    pthread
    dl
    ssl
    crypto
    z
)

# Set RPATH for both libraries to use system libraries
set_target_properties(vmware-backup-lib vmware-restore-lib PROPERTIES
    INSTALL_RPATH "/usr/lib/x86_64-linux-gnu"
    BUILD_WITH_INSTALL_RPATH TRUE
    SKIP_BUILD_RPATH FALSE
    INSTALL_RPATH_USE_LINK_PATH TRUE
    LINK_DEPENDS_NO_SHARED TRUE
) 
