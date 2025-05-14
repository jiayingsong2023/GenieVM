#pragma once

#include <string>
#include <memory>
#include <vector>
#include <functional>
#include <mutex>
#include <condition_variable>
#include <thread>
#include <queue>
#include <future>
#include <atomic>
#include <chrono>
#include <stdexcept>
#include <filesystem>
#include <nlohmann/json.hpp>
#include "common/logger.hpp"
#include "common/vsphere_rest_client.hpp"

class VSphereManager {
public:
    VSphereManager(const std::string& host, const std::string& username, const std::string& password);
    ~VSphereManager();

    bool connect();
    void disconnect();

    bool createVM(const std::string& vmName, const std::string& datastoreName, const std::string& resourcePoolName);
    bool attachDisks(const std::string& vmName, const std::vector<std::string>& diskPaths);
    bool getVM(const std::string& vmName, std::string& vmId);
    bool getDatastore(const std::string& datastoreName, std::string& datastoreId);
    bool getResourcePool(const std::string& poolName, std::string& poolId);

private:
    std::string host_;
    std::string username_;
    std::string password_;
    std::unique_ptr<VSphereRestClient> restClient_;
    bool connected_;
}; 