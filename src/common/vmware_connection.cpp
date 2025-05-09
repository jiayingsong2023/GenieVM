bool VMwareConnection::createSnapshot(const std::string& vmName,
                                    const std::string& snapshotName,
                                    const std::string& description) {
    if (!connected_) {
        Logger::error("Not connected to vCenter");
        return false;
    }

    VixHandle vmHandle;
    if (!getVMHandle(vmName, vmHandle)) {
        return false;
    }

    VixError vixError = VixVM_CreateSnapshot(
        vmHandle,
        snapshotName.c_str(),
        description.c_str(),
        VIX_SNAPSHOT_INCLUDE_MEMORY,
        VIX_INVALID_HANDLE,
        nullptr,
        nullptr
    );

    if (VIX_FAILED(vixError)) {
        logError("Failed to create snapshot");
        return false;
    }

    Logger::info("Successfully created snapshot: " + snapshotName);
    return true;
}

bool VMwareConnection::removeSnapshot(const std::string& vmName,
                                    const std::string& snapshotName) {
    if (!connected_) {
        Logger::error("Not connected to vCenter");
        return false;
    }

    VixHandle vmHandle;
    if (!getVMHandle(vmName, vmHandle)) {
        return false;
    }

    VixHandle snapshotHandle;
    VixError vixError = VixVM_GetNamedSnapshot(
        vmHandle,
        snapshotName.c_str(),
        &snapshotHandle
    );

    if (VIX_FAILED(vixError)) {
        logError("Failed to get snapshot handle");
        return false;
    }

    vixError = VixVM_RemoveSnapshot(
        vmHandle,
        snapshotHandle,
        VIX_SNAPSHOT_REMOVE_CHILDREN,
        nullptr,
        nullptr
    );

    if (VIX_FAILED(vixError)) {
        logError("Failed to remove snapshot");
        return false;
    }

    Logger::info("Successfully removed snapshot: " + snapshotName);
    return true;
} 