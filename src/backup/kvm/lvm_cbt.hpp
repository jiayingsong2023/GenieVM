#pragma once

#include "../common/cbt_provider.hpp"
#include <string>

class LVMCBT : public CBTProvider {
public:
    explicit LVMCBT(const std::string& lvPath);
    ~LVMCBT() override;

    bool enableCBT() override;
    bool disableCBT() override;
    std::vector<BlockRange> getChangedBlocks() override;
    bool resetCBT() override;
    CBTType getType() const override { return CBTType::LVM; }

private:
    std::string lvPath_;
    std::string snapshotPath_;
    bool isEnabled_;
    // Add LVM connection or other necessary members
}; 