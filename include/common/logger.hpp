#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    FATAL
};

class Logger {
public:
    static bool initialize(const std::string& logPath, LogLevel level = LogLevel::INFO);
    static void shutdown();
    static void setLogLevel(LogLevel level);
    
    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warning(const std::string& message);
    static void error(const std::string& message);
    static void fatal(const std::string& message);

private:
    static void log(LogLevel level, const std::string& message);
    static std::string levelToString(LogLevel level);

    static std::mutex mutex_;
    static std::ofstream logFile_;
    static LogLevel currentLevel_;
    static bool initialized_;
}; 