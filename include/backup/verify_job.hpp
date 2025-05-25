#pragma once

#include "backup/job.hpp"
#include "backup/backup_provider.hpp"
#include "common/parallel_task_manager.hpp"
#include "common/logger.hpp"
#include <memory>
#include <string>
#include <vector>
#include <functional>
#include <mutex>

class VerifyJob : public Job {
public:
    VerifyJob(std::shared_ptr<BackupProvider> provider,
             std::shared_ptr<ParallelTaskManager> taskManager,
             const VerifyConfig& config);
    ~VerifyJob() override;

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

    // Configuration
    VerifyConfig getConfig() const { return config_; }
    void setConfig(const VerifyConfig& config) { config_ = config; }

private:
    bool verifyBackup();
    void handleVerificationCompletion(bool success, const std::string& error);
    bool validateVerifyConfig() const;
    bool createVerifyDirectory() const;
    bool writeVerifyMetadata() const;
    bool readVerifyMetadata() const;
    bool cleanupVerifyDirectory() const;

    std::shared_ptr<BackupProvider> provider_;
    std::shared_ptr<ParallelTaskManager> taskManager_;
    VerifyConfig config_;
    mutable std::mutex mutex_;
}; 