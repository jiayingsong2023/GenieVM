#pragma once

#include <string>
#include <chrono>
#include <vector>

enum class BackupState {
    NotStarted,
    InProgress,
    Completed,
    Failed,
    Cancelled,
    Paused
};

struct BackupStatus {
    BackupState state;
    double progress;
    std::string status;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    std::string error;
};

enum class RestoreState {
    NotStarted,
    InProgress,
    Completed,
    Failed,
    Cancelled,
    Paused
};

struct RestoreStatus {
    RestoreState state;
    double progress;
    std::string status;
    std::chrono::system_clock::time_point startTime;
    std::chrono::system_clock::time_point endTime;
    std::string error;
};

enum class BackupType {
    FULL,
    INCREMENTAL,
    DIFFERENTIAL
};

struct BackupMetadata {
    std::string backupId;
    std::string vmId;
    int64_t timestamp;
    BackupType type;
    int64_t size;
    std::vector<std::string> disks;
    std::string checksum;
}; 