#pragma once

#include "backup/kvm/cbt.hpp"

class LVMCBT : public CBT {
public:
    explicit LVMCBT(const std::string& diskPath);
    ~LVMCBT() override;

    bool isEnabled() const override;
    bool enable() override;
    bool disable() override;
    bool getChangedBlocks(std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) override;

private:
    bool initialized_;
}; 