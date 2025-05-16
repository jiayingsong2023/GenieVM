#pragma once

#include <string>

// Print the backup command usage information
void printBackupUsage();

// Parse a date/time string in format "YYYY-MM-DD HH:MM:SS"
time_t parseDateTime(const std::string& dateTimeStr);

// Main entry point for backup commands
int backupMain(int argc, char* argv[]); 