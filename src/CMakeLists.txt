add_library(vmware-backup-lib
    backup/backup_manager.cpp
    backup/disk_backup.cpp
    common/vmware_connection.cpp
    common/logger.cpp
)

add_library(vmware-restore-lib
    restore/restore_manager.cpp
    restore/disk_restore.cpp
    common/vmware_connection.cpp
    common/logger.cpp
)

target_include_directories(vmware-backup-lib
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_include_directories(vmware-restore-lib
    PUBLIC
    ${CMAKE_SOURCE_DIR}/include
) 