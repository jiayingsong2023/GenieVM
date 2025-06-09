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
LogLevel Logger::currentLevel_ = LogLevel::DEBUG;
bool Logger::initialized_ = false;
std::string Logger::logPath_ = "/tmp/genievm.log";

bool Logger::initialize(const std::string& logPath, LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        std::cout << "Logger already initialized" << std::endl;
        return false;
    }

    try {
        // Create directory if it doesn't exist
        std::filesystem::path logDir = std::filesystem::path(logPath).parent_path();
        if (!logDir.empty() && !std::filesystem::exists(logDir)) {
            std::filesystem::create_directories(logDir);
        }

        // Test if we can write to the log file
        FILE* testFile = fopen(logPath.c_str(), "a");
        if (!testFile) {
            std::cerr << "Failed to open log file: " << strerror(errno) << std::endl;
            return false;
        }
        fclose(testFile);

        logPath_ = logPath;
        currentLevel_ = level;
        initialized_ = true;
        
        std::cout << "Logger initialized with level: " << levelToString(currentLevel_) << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Logger initialization failed: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "Logger initialization failed with unknown error" << std::endl;
        return false;
    }
}

void Logger::shutdown() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        std::cout << "Logger shutting down" << std::endl;
        initialized_ = false;
    }
}

void Logger::setLogLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    currentLevel_ = level;
    std::cout << "Log level changed to: " << levelToString(level) << std::endl;
}

void Logger::log(LogLevel level, const std::string& message) {
    if (!initialized_) {
        return;
    }

    // Only log if the message level is greater than or equal to the current level
    if (level < currentLevel_) {
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
    
    // Write to file using direct file I/O with error handling
    FILE* logFile = fopen(logPath_.c_str(), "a");
    if (logFile) {
        fprintf(logFile, "%s", logMessage.c_str());
        fflush(logFile);
        fclose(logFile);
    } else {
        // If file open fails, write to stderr
        std::cerr << "Failed to open log file: " << strerror(errno) << std::endl;
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