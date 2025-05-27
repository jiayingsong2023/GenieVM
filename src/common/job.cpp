#include "common/job.hpp"
#include <chrono>
#include <random>
#include <sstream>
#include <iomanip>

Job::Job() : state_(State::PENDING), progress_(0) {}

void Job::updateProgress(int progress) {
    std::lock_guard<std::mutex> lock(mutex_);
    progress_ = progress;
    if (progressCallback_) {
        progressCallback_(progress);
    }
}

void Job::setError(const std::string& error) {
    std::lock_guard<std::mutex> lock(mutex_);
    error_ = error;
}

void Job::setState(State state) {
    std::lock_guard<std::mutex> lock(mutex_);
    state_ = state;
}

void Job::setStatus(const std::string& status) {
    std::lock_guard<std::mutex> lock(mutex_);
    status_ = status;
    if (statusCallback_) {
        statusCallback_(status);
    }
}

std::string Job::generateId() const {
    auto now = std::chrono::system_clock::now();
    auto now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch());
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 15);
    const char* hex = "0123456789abcdef";
    
    std::stringstream ss;
    ss << std::hex << now_ms.count();
    for (int i = 0; i < 8; ++i) {
        ss << hex[dis(gen)];
    }
    
    return ss.str();
}

int Job::getProgress() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return progress_;
}

std::string Job::getStatus() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return status_;
}

std::string Job::getError() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return error_;
}

std::string Job::getId() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return id_;
} 