#include "backup/backup_job.hpp"
#include "backup/backup_provider.hpp"
#include "common/parallel_task_manager.hpp"
#include "common/logger.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>
#include <nlohmann/json.hpp>

using namespace std::filesystem;
using json = nlohmann::json;

BackupJob::BackupJob(std::shared_ptr<BackupProvider> provider,
                    std::shared_ptr<ParallelTaskManager> taskManager,
                    const BackupConfig& config)
    : provider_(provider)
    , taskManager_(taskManager)
    , config_(config) {
    // Generate a unique job ID using our own implementation
    setId(generateId());
    setStatus("pending");
}

BackupJob::~BackupJob() {
    if (isRunning()) {
        cancel();
    }
}

bool BackupJob::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (isRunning() || isCompleted() || isFailed() || isCancelled()) {
        setError("Cannot start job in current state");
        return false;
    }

    if (!validateBackupConfig()) {
        setError("Invalid backup configuration");
        setState(State::FAILED);
        return false;
    }

    if (!createBackupDirectory()) {
        setError("Failed to create backup directory");
        setState(State::FAILED);
        return false;
    }

    setState(State::RUNNING);
    setStatus("Starting backup");
    updateProgress(0);

    // Start backup in a separate thread
    std::thread([this]() {
        try {
            executeBackup();
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(mutex_);
            setError(std::string("Backup failed: ") + e.what());
            setState(State::FAILED);
        }
    }).detach();

    return true;
}

bool BackupJob::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRunning() || isPaused()) {
        setError("Cannot pause job in current state");
        return false;
    }
    setState(State::PAUSED);
    setStatus("Backup paused");
    return true;
}

bool BackupJob::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRunning() || !isPaused()) {
        setError("Cannot resume job in current state");
        return false;
    }
    setState(State::RUNNING);
    setStatus("Backup resumed");
    return true;
}

bool BackupJob::cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRunning()) {
        setError("Cannot cancel job in current state");
        return false;
    }
    setState(State::CANCELLED);
    setStatus("Backup cancelled");
    return true;
}

bool BackupJob::isRunning() const {
    return getState() == State::RUNNING;
}

bool BackupJob::isPaused() const {
    return getState() == State::PAUSED;
}

bool BackupJob::isCompleted() const {
    return getState() == State::COMPLETED;
}

bool BackupJob::isFailed() const {
    return getState() == State::FAILED;
}

bool BackupJob::isCancelled() const {
    return getState() == State::CANCELLED;
}

int BackupJob::getProgress() const {
    return Job::getProgress();
}

std::string BackupJob::getStatus() const {
    return Job::getStatus();
}

std::string BackupJob::getError() const {
    return Job::getError();
}

std::string BackupJob::getId() const {
    return Job::getId();
}

bool BackupJob::verifyBackup() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isCompleted()) {
        setError("Cannot verify incomplete backup");
        return false;
    }

    setStatus("Verifying backup");
    updateProgress(0);

    try {
        // Verify each disk in the backup
        std::vector<std::string> diskPaths;
        if (!provider_->getVMDiskPaths(config_.vmId, diskPaths)) {
            setError("Failed to get VM disk paths: " + provider_->getLastError());
            return false;
        }

        int totalDisks = diskPaths.size();
        int verifiedDisks = 0;

        for (const auto& diskPath : diskPaths) {
            if (isCancelled()) {
                setError("Verification cancelled");
                return false;
            }

            if (isPaused()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            if (!provider_->verifyDisk(diskPath)) {
                setError("Failed to verify disk " + diskPath + ": " + provider_->getLastError());
                return false;
            }

            verifiedDisks++;
            updateProgress((verifiedDisks * 100) / totalDisks);
        }

        setStatus("Backup verified successfully");
        return true;
    } catch (const std::exception& e) {
        setError(std::string("Verification failed: ") + e.what());
        return false;
    }
}

bool BackupJob::cleanupOldBackups() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isCompleted()) {
        setError("Cannot cleanup incomplete backup");
        return false;
    }

    try {
        // Get list of backup directories
        std::vector<std::string> backupDirs;
        if (!provider_->listBackups(backupDirs)) {
            setError("Failed to list backups: " + provider_->getLastError());
            return false;
        }

        // Sort backups by date
        std::sort(backupDirs.begin(), backupDirs.end());

        // Keep only the most recent N backups
        if (backupDirs.size() > config_.maxBackups) {
            for (size_t i = 0; i < backupDirs.size() - config_.maxBackups; ++i) {
                if (!provider_->deleteBackup(backupDirs[i])) {
                    setError("Failed to delete old backup: " + provider_->getLastError());
                    return false;
                }
            }
        }

        return true;
    } catch (const std::exception& e) {
        setError(std::string("Cleanup failed: ") + e.what());
        return false;
    }
}

void BackupJob::executeBackup() {
    try {
        // Create snapshot before backup
        std::string snapshotId;
        if (!provider_->createSnapshot(config_.vmId, snapshotId)) {
            setError("Failed to create snapshot: " + provider_->getLastError());
            setState(State::FAILED);
            return;
        }

        // Get VM disk paths
        std::vector<std::string> diskPaths;
        if (!provider_->getVMDiskPaths(config_.vmId, diskPaths)) {
            provider_->removeSnapshot(config_.vmId, snapshotId); // Cleanup snapshot
            setError("Failed to get VM disk paths: " + provider_->getLastError());
            setState(State::FAILED);
            return;
        }

        // Backup each disk
        int totalDisks = diskPaths.size();
        int backedUpDisks = 0;

        for (const auto& diskPath : diskPaths) {
            if (isCancelled()) {
                provider_->removeSnapshot(config_.vmId, snapshotId); // Cleanup snapshot
                setError("Backup cancelled");
                setState(State::CANCELLED);
                return;
            }

            if (isPaused()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            if (!provider_->backupDisk(config_.vmId, diskPath, config_)) {
                provider_->removeSnapshot(config_.vmId, snapshotId); // Cleanup snapshot
                setError("Failed to backup disk " + diskPath + ": " + provider_->getLastError());
                setState(State::FAILED);
                return;
            }

            backedUpDisks++;
            updateProgress((backedUpDisks * 100) / totalDisks);
        }

        // Remove snapshot after successful backup
        if (!provider_->removeSnapshot(config_.vmId, snapshotId)) {
            setError("Warning: Failed to remove snapshot: " + provider_->getLastError());
            // Continue anyway as the backup was successful
        }

        setState(State::COMPLETED);
        setStatus("Backup completed successfully");
        updateProgress(100);
    } catch (const std::exception& e) {
        setError(std::string("Backup failed: ") + e.what());
        setState(State::FAILED);
    }
}

void BackupJob::handleBackupProgress(int progress) {
    updateProgress(progress);
}

void BackupJob::handleBackupStatus(const std::string& status) {
    setStatus(status);
}

void BackupJob::handleBackupError(const std::string& error) {
    setError(error);
    setState(State::FAILED);
}

bool BackupJob::validateBackupConfig() const {
    if (config_.vmId.empty()) {
        return false;
    }
    if (config_.backupPath.empty()) {
        return false;
    }
    return true;
}

bool BackupJob::createBackupDirectory() const {
    try {
        create_directories(config_.backupPath);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

bool BackupJob::writeBackupMetadata() const {
    try {
        std::string metadataFile = config_.backupPath + "/metadata.json";
        json metadata;
        metadata["vmId"] = config_.vmId;
        metadata["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        metadata["config"] = {
            {"backupPath", config_.backupPath},
            {"enableCBT", config_.enableCBT},
            {"incremental", config_.incremental},
            {"retentionDays", config_.retentionDays},
            {"maxBackups", config_.maxBackups},
            {"compressionLevel", config_.compressionLevel},
            {"maxConcurrentDisks", config_.maxConcurrentDisks}
        };

        std::ofstream file(metadataFile);
        if (!file.is_open()) {
            Logger::error("Failed to open metadata file for writing: " + metadataFile);
            return false;
        }
        file << metadata.dump(4);
        return true;
    } catch (const std::exception& e) {
        Logger::error("Failed to write backup metadata: " + std::string(e.what()));
        return false;
    }
}

bool BackupJob::readBackupMetadata() const {
    try {
        std::string metadataFile = config_.backupPath + "/metadata.json";
        std::ifstream file(metadataFile);
        if (!file.is_open()) {
            Logger::error("Failed to open metadata file for reading: " + metadataFile);
            return false;
        }
        json metadata;
        file >> metadata;
        return true;
    } catch (const std::exception& e) {
        Logger::error("Failed to read backup metadata: " + std::string(e.what()));
        return false;
    }
}

bool BackupJob::cleanupBackupDirectory() const {
    try {
        remove_all(config_.backupPath);
        return true;
    } catch (const std::exception& e) {
        return false;
    }
} 