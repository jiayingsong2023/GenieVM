#include "backup/verify_job.hpp"
#include "backup/backup_provider.hpp"
#include "common/parallel_task_manager.hpp"
#include "common/logger.hpp"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <chrono>
#include <thread>

using namespace std::filesystem;

VerifyJob::VerifyJob(BackupProvider* provider,
                    std::shared_ptr<ParallelTaskManager> taskManager,
                    const VerifyConfig& config)
    : provider_(provider)
    , taskManager_(taskManager)
    , config_(config) {
    setId(generateId());
    setStatus("pending");
}

VerifyJob::~VerifyJob() {
    if (isRunning()) {
        cancel();
    }
}

bool VerifyJob::start() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (isRunning()) {
        return false;
    }

    if (!validateVerifyConfig()) {
        setError("Invalid verification configuration");
        setState(State::FAILED);
        return false;
    }

    setState(State::RUNNING);
    setStatus("running");
    updateProgress(0);

    // Start verification in a separate thread
    std::thread([this]() {
        try {
            bool success = verifyBackup();
            handleVerificationCompletion(success, success ? "" : provider_->getLastError());
        } catch (const std::exception& e) {
            std::lock_guard<std::mutex> lock(mutex_);
            setError(std::string("Verification failed: ") + e.what());
            setState(State::FAILED);
        }
    }).detach();

    return true;
}

bool VerifyJob::cancel() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRunning()) {
        return false;
    }

    setState(State::CANCELLED);
    setStatus("cancelled");
    return true;
}

bool VerifyJob::pause() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRunning() || isPaused()) {
        return false;
    }

    setState(State::PAUSED);
    setStatus("paused");
    return true;
}

bool VerifyJob::resume() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!isRunning() || !isPaused()) {
        return false;
    }

    setState(State::RUNNING);
    setStatus("running");
    return true;
}

bool VerifyJob::isRunning() const {
    return getState() == State::RUNNING;
}

bool VerifyJob::isPaused() const {
    return getState() == State::PAUSED;
}

bool VerifyJob::isCompleted() const {
    return getState() == State::COMPLETED;
}

bool VerifyJob::isFailed() const {
    return getState() == State::FAILED;
}

bool VerifyJob::isCancelled() const {
    return getState() == State::CANCELLED;
}

int VerifyJob::getProgress() const {
    return Job::getProgress();
}

std::string VerifyJob::getStatus() const {
    return Job::getStatus();
}

std::string VerifyJob::getError() const {
    return Job::getError();
}

std::string VerifyJob::getId() const {
    return Job::getId();
}

bool VerifyJob::validateVerifyConfig() const {
    if (config_.backupId.empty()) {
        return false;
    }
    return true;
}

bool VerifyJob::verifyBackup() {
    try {
        // Check if job is paused
        while (isPaused()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }

        // Check if job is cancelled
        if (!isRunning()) {
            return false;
        }

        // Perform verification
        if (!provider_->verifyBackup(config_.backupId)) {
            return false;
        }

        return true;
    } catch (const std::exception& e) {
        setError(e.what());
        return false;
    }
}

void VerifyJob::handleVerificationCompletion(bool success, const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!success) {
        setError("Verification failed: " + error);
        setState(State::FAILED);
        setStatus("failed");
        return;
    }

    setState(State::COMPLETED);
    setStatus("completed");
    updateProgress(100);
} 