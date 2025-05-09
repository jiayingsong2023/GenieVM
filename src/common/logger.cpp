#include "common/logger.hpp"
#include <chrono>
#include <iomanip>
#include <sstream>

namespace vmware {

std::ofstream Logger::logFile_;
std::mutex Logger::logMutex_;
bool Logger::initialized_ = false;

void Logger::init(const std::string& logFile) {
    std::lock_guard<std::mutex> lock(logMutex_);
    if (!initialized_) {
        logFile_.open(logFile, std::ios::app);
        initialized_ = true;
    }
}

void Logger::info(const std::string& message) {
    log("INFO", message);
}

void Logger::warning(const std::string& message) {
    log("WARNING", message);
}

void Logger::error(const std::string& message) {
    log("ERROR", message);
}

void Logger::log(const std::string& level, const std::string& message) {
    std::lock_guard<std::mutex> lock(logMutex_);
    
    if (!initialized_) {
        init();
    }

    std::string logMessage = getTimestamp() + " [" + level + "] " + message;
    
    // Write to log file
    logFile_ << logMessage << std::endl;
    logFile_.flush();
    
    // Also write to console
    std::cout << logMessage << std::endl;
}

std::string Logger::getTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    return ss.str();
}

} // namespace vmware 