#include "common/logger.hpp"
#include <iostream>
#include <fstream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <mutex>
#include <filesystem>
#include <cstring>  // for strerror
#include <cerrno>   // for errno

std::mutex Logger::mutex_;
std::ofstream Logger::logFile_;
LogLevel Logger::currentLevel_ = LogLevel::DEBUG;
bool Logger::initialized_ = false;

bool Logger::initialize(const std::string& logPath, LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return false;
    }

    try {
        // Create directory if it doesn't exist
        std::filesystem::path logDir = std::filesystem::path(logPath).parent_path();
        if (!logDir.empty() && !std::filesystem::exists(logDir)) {
            std::filesystem::create_directories(logDir);
        }

        // Test write to log file
        FILE* testFile = fopen(logPath.c_str(), "a");
        if (!testFile) {
            return false;
        }
        fprintf(testFile, "=== Logger Initialization ===\n");
        fflush(testFile);
        fclose(testFile);

        currentLevel_ = level;
        initialized_ = true;
        
        // Write initial log entry
        log(LogLevel::INFO, "Logger initialized with level: " + levelToString(currentLevel_));
        return true;
    } catch (const std::exception& e) {
        return false;
    } catch (...) {
        return false;
    }
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        log(LogLevel::INFO, "Logger shutting down");
        if (logFile_.is_open()) {
            logFile_.close();
        }
        initialized_ = false;
    }
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentLevel_ = level;
    log(LogLevel::INFO, "Log level changed to: " + levelToString(level));
}

void Logger::log(LogLevel level, const std::string& message) {
    if (level > currentLevel_) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    
    std::string levelStr = levelToString(level);
    std::string logMessage = ss.str() + " [" + levelStr + "] " + message + "\n";
    
    // Always write to stdout/stderr based on level
    if (level >= LogLevel::ERROR) {
        std::cerr << logMessage;
        std::cerr.flush();
    } else {
        std::cout << logMessage;
        std::cout.flush();
    }
    
    // Write to file using direct file I/O
    FILE* logFile = fopen("/tmp/genievm.log", "a");
    if (logFile) {
        fprintf(logFile, "%s", logMessage.c_str());
        fflush(logFile);
        fclose(logFile);
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