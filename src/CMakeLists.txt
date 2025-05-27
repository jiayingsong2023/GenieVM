add_library(vmware-backup-lib
    backup/vmware/disk_backup.cpp
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
    restore/vmware/disk_restore.cpp
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
    pthread
    dl
    ssl
    crypto
    z
)

target_link_libraries(vmware-restore-lib
    PRIVATE
    pthread
    dl
    ssl
    crypto
    z
) 
