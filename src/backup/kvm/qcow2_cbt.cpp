#include "qcow2_cbt.hpp"
#include "common/logger.hpp"
#include <stdexcept>
#include <system_error>
#include <cstdio>

QCOW2CBT::QCOW2CBT(const std::string& diskPath)
    : diskPath_(diskPath), isEnabled_(false) {
}

QCOW2CBT::~QCOW2CBT() {
    if (isEnabled_) {
        disableCBT();
    }
}

bool QCOW2CBT::enableCBT() {
    if (isEnabled_) {
        return true;
    }

    try {
        // Create QEMU dirty bitmap
        std::string cmd = "qemu-img bitmap add " + diskPath_ + " cbt_bitmap";
        int result = system(cmd.c_str());
        if (result != 0) {
            Logger::error("Failed to create QEMU dirty bitmap");
            return false;
        }
        isEnabled_ = true;
        return true;
    } catch (const std::exception& e) {
        Logger::error("Exception in enableCBT: " + std::string(e.what()));
        return false;
    }
}

bool QCOW2CBT::disableCBT() {
    if (!isEnabled_) {
        return true;
    }

    try {
        // Remove QEMU dirty bitmap
        std::string cmd = "qemu-img bitmap remove " + diskPath_ + " cbt_bitmap";
        int result = system(cmd.c_str());
        if (result != 0) {
            Logger::error("Failed to remove QEMU dirty bitmap");
            return false;
        }
        isEnabled_ = false;
        return true;
    } catch (const std::exception& e) {
        Logger::error("Exception in disableCBT: " + std::string(e.what()));
        return false;
    }
}

std::vector<BlockRange> QCOW2CBT::getChangedBlocks() {
    if (!isEnabled_) {
        return {};
    }

    try {
        // Query QEMU dirty bitmap
        std::string cmd = "qemu-img bitmap query " + diskPath_ + " cbt_bitmap";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            Logger::error("Failed to query QEMU dirty bitmap");
            return {};
        }

        std::vector<BlockRange> blocks;
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            // Parse bitmap output to get changed blocks
            // Format: offset size
            uint64_t offset, size;
            if (sscanf(buffer, "%lu %lu", &offset, &size) == 2) {
                blocks.push_back({offset, size});
            }
        }
        pclose(pipe);
        return blocks;
    } catch (const std::exception& e) {
        Logger::error("Exception in getChangedBlocks: " + std::string(e.what()));
        return {};
    }
}

bool QCOW2CBT::resetCBT() {
    if (!isEnabled_) {
        return true;
    }

    try {
        // Reset QEMU dirty bitmap
        std::string cmd = "qemu-img bitmap clear " + diskPath_ + " cbt_bitmap";
        int result = system(cmd.c_str());
        if (result != 0) {
            Logger::error("Failed to reset QEMU dirty bitmap");
            return false;
        }
        return true;
    } catch (const std::exception& e) {
        Logger::error("Exception in resetCBT: " + std::string(e.what()));
        return false;
    }
} 