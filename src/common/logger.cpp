#include "common/logger.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>

std::mutex Logger::mutex_;
std::ofstream Logger::logFile_;
LogLevel Logger::currentLevel_ = LogLevel::INFO;
bool Logger::initialized_ = false;

bool Logger::initialize(const std::string& logPath, LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return false;
    }

    try {
        logFile_.open(logPath, std::ios::app);
        if (!logFile_.is_open()) {
            std::cerr << "Failed to open log file: " << logPath << std::endl;
            return false;
        }

        currentLevel_ = level;
        initialized_ = true;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Failed to initialize logger: " << e.what() << std::endl;
        return false;
    }
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (logFile_.is_open()) {
        logFile_.close();
    }
    initialized_ = false;
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentLevel_ = level;
}

void Logger::log(LogLevel level, const std::string& message) {
    if (!initialized_ || level < currentLevel_) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    
    std::string levelStr;
    switch (level) {
        case LogLevel::DEBUG:
            levelStr = "DEBUG";
            break;
        case LogLevel::INFO:
            levelStr = "INFO";
            break;
        case LogLevel::WARNING:
            levelStr = "WARNING";
            break;
        case LogLevel::ERROR:
            levelStr = "ERROR";
            break;
        case LogLevel::FATAL:
            levelStr = "FATAL";
            break;
        default:
            levelStr = "UNKNOWN";
    }

    std::string logMessage = ss.str() + " [" + levelStr + "] " + message + "\n";
    
    if (logFile_.is_open()) {
        logFile_ << logMessage;
        logFile_.flush();
    }
    
    std::cout << logMessage;
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

void Logger::fatal(const std::string& message) {
    log(LogLevel::FATAL, message);
}

std::string Logger::levelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        case LogLevel::FATAL:   return "FATAL";
        default:            return "UNKNOWN";
    }
} 