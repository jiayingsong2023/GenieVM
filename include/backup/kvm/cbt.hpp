#pragma once

#include <string>
#include <vector>
#include <memory>

class CBT {
public:
    virtual ~CBT() = default;
    
    virtual bool isEnabled() const = 0;
    virtual bool enable() = 0;
    virtual bool disable() = 0;
    virtual bool getChangedBlocks(std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) = 0;
    
protected:
    CBT(const std::string& diskPath) : diskPath_(diskPath) {}
    std::string diskPath_;
}; 