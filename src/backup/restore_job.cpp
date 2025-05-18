#include "backup/restore_job.hpp"
#include "common/logger.hpp"
#include <filesystem>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>

RestoreJob::RestoreJob(const std::string& vmId, const std::string& backupId, const RestoreConfig& config)
    : vmId_(vmId)
    , backupId_(backupId)
    , config_(config)
    , status_(RestoreStatus::PENDING)
    , progress_(0.0)
    , cancelled_(false)
    , vsphereClient_(std::make_shared<VSphereRestClient>(
        config.vsphereHost,
        config.vsphereUsername,
        config.vspherePassword)) {
}

RestoreJob::~RestoreJob() {
    if (status_ == RestoreStatus::RUNNING) {
        stop();
    }
}

bool RestoreJob::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ == RestoreStatus::RUNNING) {
        Logger::error("Restore already in progress");
        return false;
    }

    if (!validateConfig()) {
        status_ = RestoreStatus::FAILED;
        return false;
    }

    status_ = RestoreStatus::RUNNING;
    cancelled_ = false;
    restoreFuture_ = std::async(std::launch::async, &RestoreJob::runRestore, this);
    return true;
}

bool RestoreJob::stop() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ != RestoreStatus::RUNNING && status_ != RestoreStatus::PAUSED) {
        return true;
    }

    cancelled_ = true;
    if (restoreFuture_.valid()) {
        restoreFuture_.wait();
    }
    status_ = RestoreStatus::CANCELLED;
    return true;
}

bool RestoreJob::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ != RestoreStatus::RUNNING) {
        return false;
    }

    status_ = RestoreStatus::PAUSED;
    return true;
}

bool RestoreJob::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (status_ != RestoreStatus::PAUSED) {
        return false;
    }

    status_ = RestoreStatus::RUNNING;
    return true;
}

RestoreStatus RestoreJob::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

std::string RestoreJob::getVMId() const {
    return vmId_;
}

std::string RestoreJob::getBackupId() const {
    return backupId_;
}

const RestoreConfig& RestoreJob::getConfig() const {
    return config_;
}

double RestoreJob::getProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_;
}

std::string RestoreJob::getErrorMessage() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return errorMessage_;
}

void RestoreJob::runRestore() {
    try {
        // Login to vSphere
        if (!vsphereClient_->login()) {
            throw std::runtime_error("Failed to login to vSphere");
        }

        // Get backup information
        std::string backupInfo;
        if (!vsphereClient_->getBackup(backupId_, backupInfo)) {
            throw std::runtime_error("Failed to get backup information");
        }

        // Parse backup information
        nlohmann::json backupJson = nlohmann::json::parse(backupInfo);
        
        // Create VM configuration
        nlohmann::json vmConfig = {
            {"name", config_.vmName},
            {"datastore_id", config_.targetDatastore},
            {"resource_pool_id", config_.targetResourcePool},
            {"num_cpus", config_.numCPUs},
            {"memory_mb", config_.memoryMB},
            {"guest_os", config_.guestOS}
        };

        // Create VM
        nlohmann::json response;
        if (!vsphereClient_->createVM(vmConfig, response)) {
            throw std::runtime_error("Failed to create VM");
        }

        std::string newVmId = response["value"].get<std::string>();
        updateProgress(0.2);  // 20% complete

        if (cancelled_) {
            status_ = RestoreStatus::CANCELLED;
            return;
        }

        // Attach disks
        for (size_t i = 0; i < config_.diskConfigs.size(); ++i) {
            const auto& diskConfig = config_.diskConfigs[i];
            
            nlohmann::json diskAttachConfig = {
                {"path", diskConfig.path},
                {"controller_type", diskConfig.type},
                {"unit_number", static_cast<int>(i)},
                {"thin_provisioned", diskConfig.thinProvisioned}
            };

            if (!vsphereClient_->attachDisk(newVmId, diskAttachConfig, response)) {
                throw std::runtime_error("Failed to attach disk: " + diskConfig.path);
            }

            // Update progress based on disk attachment
            double diskProgress = 0.2 + (0.6 * (i + 1) / config_.diskConfigs.size());
            updateProgress(diskProgress);

            if (cancelled_) {
                status_ = RestoreStatus::CANCELLED;
                return;
            }
        }

        // Verify restore
        if (!vsphereClient_->verifyBackup(backupId_, response)) {
            throw std::runtime_error("Restore verification failed");
        }

        updateProgress(0.9);  // 90% complete

        // Power on VM if configured
        if (config_.powerOnAfterRestore) {
            if (!vsphereClient_->powerOnVM(newVmId)) {
                throw std::runtime_error("Failed to power on VM");
            }
        }

        status_ = RestoreStatus::COMPLETED;
        updateProgress(1.0);
    } catch (const std::exception& e) {
        setError(e.what());
        status_ = RestoreStatus::FAILED;
    }
}

bool RestoreJob::validateConfig() {
    // Validate VM ID
    if (config_.vmId.empty()) {
        errorMessage_ = "VM ID is required";
        return false;
    }

    // Validate backup ID
    if (config_.backupId.empty()) {
        errorMessage_ = "Backup ID is required";
        return false;
    }

    // Validate target datastore
    if (config_.targetDatastore.empty()) {
        errorMessage_ = "Target datastore is required";
        return false;
    }

    // Validate target resource pool
    if (config_.targetResourcePool.empty()) {
        errorMessage_ = "Target resource pool is required";
        return false;
    }

    // Validate disk configurations
    if (config_.diskConfigs.empty()) {
        errorMessage_ = "At least one disk configuration is required";
        return false;
    }

    for (const auto& diskConfig : config_.diskConfigs) {
        // Validate disk path
        if (diskConfig.path.empty()) {
            errorMessage_ = "Disk path is required for all disks";
            return false;
        }

        // Validate disk size
        if (diskConfig.sizeKB <= 0) {
            errorMessage_ = "Disk size must be greater than 0";
            return false;
        }

        // Validate disk format
        if (diskConfig.format.empty()) {
            errorMessage_ = "Disk format is required";
            return false;
        }

        // Validate disk type
        if (diskConfig.type.empty()) {
            errorMessage_ = "Disk type is required";
            return false;
        }
    }

    return true;
}

void RestoreJob::updateProgress(double progress) {
    std::lock_guard<std::mutex> lock(mutex_);
    progress_ = progress;
}

void RestoreJob::setError(const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    errorMessage_ = message;
} 