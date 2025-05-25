add_library(vmware-backup-lib
    backup/disk_backup.cpp
    common/vmware_connection.cpp
    common/logger.cpp
    backup/job_manager.cpp
    backup/backup_job.cpp
    backup/verify_job.cpp
    backup/restore_job.cpp
    common/parallel_task_manager.cpp
)

add_library(vmware-restore-lib
    restore/disk_restore.cpp
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