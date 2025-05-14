#pragma once

#include <string>
#include <memory>
#include <functional>
#include "common/vmware_connection.hpp"

class DiskRestore {
public:
    using ProgressCallback = std::function<void(double)>;

    DiskRestore(std::shared_ptr<VMwareConnection> connection);
    ~DiskRestore();

    bool initialize();
    bool startRestore(const std::string& vmId, const std::string& backupPath);
    bool stopRestore();
    bool pauseRestore();
    bool resumeRestore();
    bool verifyRestore();
    void setProgressCallback(ProgressCallback callback);

private:
    std::shared_ptr<VMwareConnection> connection_;
    std::string vmId_;
    std::string backupPath_;
    ProgressCallback progressCallback_;
    bool isRunning_;
    bool isPaused_;
}; 