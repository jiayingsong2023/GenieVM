#pragma once

#include <string>
#include <vector>
#include <memory>

struct BlockRange {
    uint64_t start;
    uint64_t length;
};

enum class CBTType {
    QCOW2,
    LVM,
    VDDK  // For VMware
};

class CBTProvider {
public:
    virtual ~CBTProvider() = default;
    virtual bool enableCBT() = 0;
    virtual bool disableCBT() = 0;
    virtual std::vector<BlockRange> getChangedBlocks() = 0;
    virtual bool resetCBT() = 0;
    virtual CBTType getType() const = 0;
}; 