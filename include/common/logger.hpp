#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace vmware {

class Logger {
public:
    enum class Level {
        DEBUG,
        INFO,
        WARNING,
        ERROR,
        FATAL
    };

    static void init(const std::string& logFile, Level minLevel = Level::INFO);
    static void setLevel(Level level);
    
    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warning(const std::string& message);
    static void error(const std::string& message);
    static void fatal(const std::string& message);

private:
    static Logger& getInstance();
    
    Logger();
    ~Logger();
    
    void log(Level level, const std::string& message);
    std::string getTimestamp() const;
    std::string levelToString(Level level) const;
    
    std::ofstream logFile_;
    Level minLevel_;
    std::mutex mutex_;
    bool initialized_;
    
    static Logger* instance_;
};

} // namespace vmware 