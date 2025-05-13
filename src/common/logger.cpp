#include "common/logger.hpp"
#include <iostream>
#include <filesystem>

namespace vmware {

Logger* Logger::instance_ = nullptr;

Logger& Logger::getInstance() {
    if (!instance_) {
        instance_ = new Logger();
    }
    return *instance_;
}

Logger::Logger()
    : minLevel_(Level::INFO)
    , initialized_(false) {
}

Logger::~Logger() {
    if (logFile_.is_open()) {
        logFile_.close();
    }
}

void Logger::init(const std::string& logFile, Level minLevel) {
    Logger& logger = getInstance();
    std::lock_guard<std::mutex> lock(logger.mutex_);

    if (logger.initialized_) {
        return;
    }

    // Create log directory if it doesn't exist
    std::filesystem::path logPath(logFile);
    std::filesystem::create_directories(logPath.parent_path());

    logger.logFile_.open(logFile, std::ios::app);
    if (!logger.logFile_.is_open()) {
        std::cerr << "Failed to open log file: " << logFile << std::endl;
        return;
    }

    logger.minLevel_ = minLevel;
    logger.initialized_ = true;
    logger.info("Logger initialized");
}

void Logger::setLevel(Level level) {
    Logger& logger = getInstance();
    std::lock_guard<std::mutex> lock(logger.mutex_);
    logger.minLevel_ = level;
}

void Logger::debug(const std::string& message) {
    getInstance().log(Level::DEBUG, message);
}

void Logger::info(const std::string& message) {
    getInstance().log(Level::INFO, message);
}

void Logger::warning(const std::string& message) {
    getInstance().log(Level::WARNING, message);
}

void Logger::error(const std::string& message) {
    getInstance().log(Level::ERROR, message);
}

void Logger::fatal(const std::string& message) {
    getInstance().log(Level::FATAL, message);
}

void Logger::log(Level level, const std::string& message) {
    if (!initialized_ || level < minLevel_) {
        return;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    
    std::string logEntry = getTimestamp() + " [" + levelToString(level) + "] " + message;
    
    logFile_ << logEntry << std::endl;
    logFile_.flush();

    // Also output to console for ERROR and FATAL levels
    if (level >= Level::ERROR) {
        std::cerr << logEntry << std::endl;
    }
}

std::string Logger::getTimestamp() const {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;

    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S")
       << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return ss.str();
}

std::string Logger::levelToString(Level level) const {
    switch (level) {
        case Level::DEBUG:   return "DEBUG";
        case Level::INFO:    return "INFO";
        case Level::WARNING: return "WARNING";
        case Level::ERROR:   return "ERROR";
        case Level::FATAL:   return "FATAL";
        default:            return "UNKNOWN";
    }
}

} // namespace vmware 