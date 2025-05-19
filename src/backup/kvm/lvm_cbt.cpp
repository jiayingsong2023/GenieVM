#include "lvm_cbt.hpp"
#include "common/logger.hpp"
#include <stdexcept>
#include <system_error>
#include <filesystem>
#include <cstdio>

LVMCBT::LVMCBT(const std::string& lvPath)
    : lvPath_(lvPath), isEnabled_(false) {
    // Generate snapshot path
    std::filesystem::path path(lvPath);
    snapshotPath_ = path.parent_path() / (path.filename().string() + "_cbt_snapshot");
}

LVMCBT::~LVMCBT() {
    if (isEnabled_) {
        disableCBT();
    }
}

bool LVMCBT::enableCBT() {
    if (isEnabled_) {
        return true;
    }

    try {
        // Create LVM snapshot
        std::string cmd = "lvcreate -s -n " + snapshotPath_ + " -l 100%ORIGIN " + lvPath_;
        int result = system(cmd.c_str());
        if (result != 0) {
            Logger::error("Failed to create LVM snapshot");
            return false;
        }
        isEnabled_ = true;
        return true;
    } catch (const std::exception& e) {
        Logger::error("Exception in enableCBT: " + std::string(e.what()));
        return false;
    }
}

bool LVMCBT::disableCBT() {
    if (!isEnabled_) {
        return true;
    }

    try {
        // Remove LVM snapshot
        std::string cmd = "lvremove -f " + snapshotPath_;
        int result = system(cmd.c_str());
        if (result != 0) {
            Logger::error("Failed to remove LVM snapshot");
            return false;
        }
        isEnabled_ = false;
        return true;
    } catch (const std::exception& e) {
        Logger::error("Exception in disableCBT: " + std::string(e.what()));
        return false;
    }
}

std::vector<BlockRange> LVMCBT::getChangedBlocks() {
    if (!isEnabled_) {
        return {};
    }

    try {
        // Get changed blocks by comparing original and snapshot
        std::string cmd = "dd if=" + lvPath_ + " of=/dev/null bs=4M 2>&1 | grep -o '[0-9]\\+ bytes'";
        FILE* pipe = popen(cmd.c_str(), "r");
        if (!pipe) {
            Logger::error("Failed to get LVM changed blocks");
            return {};
        }

        std::vector<BlockRange> blocks;
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe)) {
            // Parse dd output to get changed blocks
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

bool LVMCBT::resetCBT() {
    if (!isEnabled_) {
        return true;
    }

    try {
        // Reset LVM snapshot by removing and recreating it
        if (!disableCBT()) {
            return false;
        }
        return enableCBT();
    } catch (const std::exception& e) {
        Logger::error("Exception in resetCBT: " + std::string(e.what()));
        return false;
    }
} 