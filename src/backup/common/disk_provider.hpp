#pragma once

#include <string>
#include <vector>
#include <memory>
#include <functional>

using ProgressCallback = std::function<void(int progress)>;

class DiskProvider {
public:
    DiskProvider() = default;
    virtual ~DiskProvider() = default;

    virtual bool initialize() = 0;
    virtual bool openDisk(const std::string& path) = 0;
    virtual void closeDisk() = 0;
    virtual bool readBlocks(uint64_t startBlock, uint64_t numBlocks, void* buffer) = 0;
    virtual bool writeBlocks(uint64_t startBlock, uint64_t numBlocks, const void* buffer) = 0;
    virtual uint64_t getDiskSize() const = 0;
    virtual void setProgressCallback(ProgressCallback callback) = 0;
}; 