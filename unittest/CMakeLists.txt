# Add test executables
add_executable(backup_provider_test
    backup_provider_test.cpp
    backup_job_test.cpp
    vmware_backup_provider_test.cpp
    kvm_backup_provider_test.cpp
)

add_executable(cbt_test
    cbt_test.cpp
    qcow2_cbt_test.cpp
    lvm_cbt_test.cpp
)

# Link test executables with required libraries
target_link_libraries(backup_provider_test
    PRIVATE
        vmware-backup-lib
        vddk-wrapper
        ${GTEST_LIBRARIES}
        ${GTEST_MAIN_LIBRARIES}
        pthread
)

target_link_libraries(cbt_test
    PRIVATE
        vmware-backup-lib
        vddk-wrapper
        ${GTEST_LIBRARIES}
        ${GTEST_MAIN_LIBRARIES}
        pthread
)

# Add tests to CTest
add_test(NAME backup_provider_test COMMAND backup_provider_test)
add_test(NAME cbt_test COMMAND cbt_test)

# Set test properties
set_tests_properties(backup_provider_test PROPERTIES
    ENVIRONMENT "LD_LIBRARY_PATH=${VDDK_LIB_DIR}:$ENV{LD_LIBRARY_PATH}"
)

set_tests_properties(cbt_test PROPERTIES
    ENVIRONMENT "LD_LIBRARY_PATH=${VDDK_LIB_DIR}:$ENV{LD_LIBRARY_PATH}"
) 