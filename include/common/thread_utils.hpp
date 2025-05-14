#pragma once

#include <ctime>
#include <unistd.h>  // for sleep()

namespace thread_utils {

// Sleep for specified number of seconds
inline void sleep_for_seconds(int seconds) {
    sleep(seconds);  // Using POSIX sleep() instead of chrono
}

// Get current time in seconds since epoch
inline time_t get_current_time() {
    return std::time(nullptr);
}

// Check if current time has passed the target time
inline bool has_time_passed(time_t target_time) {
    return std::time(nullptr) >= target_time;
}

} // namespace thread_utils
