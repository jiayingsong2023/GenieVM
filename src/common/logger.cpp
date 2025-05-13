#include "common/logger.hpp"
#include <iostream>
#include <fstream>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace vmware {

std::ofstream Logger::logFile_;
LogLevel Logger::currentLevel_ = LogLevel::INFO;

void Logger::init(const std::string& logFilePath) {
    logFile_.open(logFilePath, std::ios::app);
}

void Logger::setLogLevel(LogLevel level) {
    currentLevel_ = level;
}

std::string Logger::getCurrentTimestamp() {
    time_t now = time(nullptr);
    struct tm* timeinfo = localtime(&now);
    char buffer[80];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", timeinfo);
    return std::string(buffer);
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level < currentLevel_) {
        return;
    }

    std::string levelStr;
    switch (level) {
        case LogLevel::DEBUG:   levelStr = "DEBUG"; break;
        case LogLevel::INFO:    levelStr = "INFO"; break;
        case LogLevel::WARNING: levelStr = "WARNING"; break;
        case LogLevel::ERROR:   levelStr = "ERROR"; break;
    }

    std::string logMessage = getCurrentTimestamp() + " [" + levelStr + "] " + message;
    
    // Log to console
    std::cout << logMessage << std::endl;
    
    // Log to file if open
    if (logFile_.is_open()) {
        logFile_ << logMessage << std::endl;
        logFile_.flush();
    }
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

} // namespace vmware 