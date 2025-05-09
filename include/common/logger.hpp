#pragma once

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>

namespace vmware {

class Logger {
public:
    static void init(const std::string& logFile = "vmware-backup.log");
    static void info(const std::string& message);
    static void warning(const std::string& message);
    static void error(const std::string& message);

private:
    static std::ofstream logFile_;
    static std::mutex logMutex_;
    static bool initialized_;

    static void log(const std::string& level, const std::string& message);
    static std::string getTimestamp();
};

} // namespace vmware 