#pragma once

#include "../common/cbt_provider.hpp"
#include <string>

class QCOW2CBT : public CBTProvider {
public:
    explicit QCOW2CBT(const std::string& diskPath);
    ~QCOW2CBT() override;

    bool enableCBT() override;
    bool disableCBT() override;
    std::vector<BlockRange> getChangedBlocks() override;
    bool resetCBT() override;
    CBTType getType() const override { return CBTType::QCOW2; }

private:
    std::string diskPath_;
    bool isEnabled_;
    // Add QEMU connection or other necessary members
}; 