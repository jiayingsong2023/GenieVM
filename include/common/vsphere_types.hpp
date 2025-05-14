#pragma once

#include <string>
#include <vector>
#include <nlohmann/json.hpp>

struct VirtualMachine {
    std::string id;
    std::string name;
    std::string powerState;
    int numCPUs;
    int memoryMB;
    std::string guestOS;
    std::string version;
    std::vector<std::string> diskIds;
    nlohmann::json additionalInfo;
};

struct VirtualDisk {
    std::string id;
    std::string name;
    std::string path;
    uint64_t capacityKB;
    std::string diskType;
    bool thinProvisioned;
    std::string controllerType;
    int unitNumber;
    nlohmann::json additionalInfo;
}; 