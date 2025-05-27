#pragma once

#include "common/job.hpp"
#include "backup/backup_provider.hpp"
#include "backup/vm_config.hpp"
#include "common/parallel_task_manager.hpp"
#include "common/logger.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <mutex>

class BackupJob : public Job {
public:
    BackupJob(std::shared_ptr<BackupProvider> provider,
             std::shared_ptr<ParallelTaskManager> taskManager,
             const BackupConfig& config);
    ~BackupJob() override;

    // Job interface implementation
    bool start() override;
    bool pause() override;
    bool resume() override;
    bool cancel() override;
    bool isRunning() const override;
    bool isPaused() const override;
    bool isCompleted() const override;
    bool isFailed() const override;
    bool isCancelled() const override;
    int getProgress() const override;
    std::string getStatus() const override;
    std::string getError() const override;
    std::string getId() const override;

    // Backup-specific methods
    bool verifyBackup();
    bool cleanupOldBackups();
    bool getChangedBlocks(std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks);

    // Configuration
    BackupConfig getConfig() const { return config_; }
    void setConfig(const BackupConfig& config) { config_ = config; }

private:
    void executeBackup();
    void handleBackupProgress(int progress);
    void handleBackupStatus(const std::string& status);
    void handleBackupError(const std::string& error);
    bool validateBackupConfig() const;
    bool createBackupDirectory() const;
    bool writeBackupMetadata() const;
    bool readBackupMetadata() const;
    bool cleanupBackupDirectory() const;

    std::shared_ptr<BackupProvider> provider_;
    std::shared_ptr<ParallelTaskManager> taskManager_;
    BackupConfig config_;
    mutable std::mutex mutex_;
}; 