#pragma once

#include "common/job.hpp"
#include "backup/vm_config.hpp"
#include "common/logger.hpp"
#include "backup/backup_provider.hpp"
#include "common/parallel_task_manager.hpp"
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <future>
#include <mutex>

class RestoreJob : public Job {
public:
    RestoreJob(std::shared_ptr<BackupProvider> provider,
               std::shared_ptr<ParallelTaskManager> taskManager,
               const RestoreConfig& config);
    ~RestoreJob() override;

    // Job interface implementation
    bool start() override;
    bool cancel() override;
    bool pause() override;
    bool resume() override;
    bool isRunning() const override;
    bool isPaused() const override;
    bool isCompleted() const override;
    bool isFailed() const override;
    bool isCancelled() const override;
    int getProgress() const override;
    std::string getStatus() const override;
    std::string getError() const override;
    std::string getId() const override;

    // Configuration
    RestoreConfig getConfig() const { return config_; }
    void setConfig(const RestoreConfig& config) { config_ = config; }

    // Status and information
    std::string getVMId() const;
    std::string getBackupId() const;

private:
    bool restoreDisk(const std::string& diskPath);
    void updateOverallProgress();
    void handleDiskTaskCompletion(const std::string& diskPath, bool success, const std::string& error);

    std::shared_ptr<BackupProvider> provider_;
    std::shared_ptr<ParallelTaskManager> taskManager_;
    RestoreConfig config_;
    std::vector<std::string> diskPaths_;
    std::unordered_map<std::string, std::future<bool>> diskTasks_;
    std::unordered_map<std::string, int> diskProgress_;
    mutable std::mutex mutex_;
}; 