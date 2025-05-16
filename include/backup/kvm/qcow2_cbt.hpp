#pragma once

#include "backup/kvm/cbt.hpp"

class QCOW2CBT : public CBT {
public:
    explicit QCOW2CBT(const std::string& diskPath);
    ~QCOW2CBT() override;

    bool isEnabled() const override;
    bool enable() override;
    bool disable() override;
    bool getChangedBlocks(std::vector<std::pair<uint64_t, uint64_t>>& changedBlocks) override;

private:
    bool initialized_;
}; 