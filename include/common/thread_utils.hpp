#pragma once

#include <thread>
#include <chrono>
#include <functional>
#include <future>

// Thread utilities for managing thread pools and async operations
class ThreadUtils {
public:
    static void sleepFor(std::chrono::milliseconds duration) {
        std::this_thread::sleep_for(duration);
    }

    template<typename Func, typename... Args>
    static std::future<typename std::result_of<Func(Args...)>::type>
    async(Func&& func, Args&&... args) {
        return std::async(std::launch::async, 
                         std::forward<Func>(func), 
                         std::forward<Args>(args)...);
    }

    static unsigned int getThreadCount() {
        return std::thread::hardware_concurrency();
    }
};
