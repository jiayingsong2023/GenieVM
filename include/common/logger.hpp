#pragma once

#include <string>
#include <mutex>

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

class Logger {
public:
    static bool initialize(const std::string& logPath, LogLevel level = LogLevel::DEBUG);
    static void shutdown();
    static void setLogLevel(LogLevel level);
    
    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warning(const std::string& message);
    static void error(const std::string& message);
    static void fatal(const std::string& message);
    static bool isInitialized() { return initialized_; }

private:
    static void log(LogLevel level, const std::string& message);
    static std::string levelToString(LogLevel level);

    static std::mutex mutex_;
    static LogLevel currentLevel_;
    static bool initialized_;
    static std::string logPath_;
}; 