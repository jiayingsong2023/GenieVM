#include <gtest/gtest.h>
#include "backup/backup_provider.hpp"
#include "backup/vmware/vmware_backup_provider.hpp"
#include "backup/vmware/vmware_connection.hpp"
#include "backup/kvm/kvm_backup_provider.hpp"
#include <memory>

class BackupProviderTest : public ::testing::Test {
protected:
    void SetUp() override {
        connection_ = std::make_shared<VMwareConnection>();
        provider_ = std::make_shared<VMwareBackupProvider>(connection_);
    }

    std::shared_ptr<VMwareConnection> connection_;
    std::shared_ptr<VMwareBackupProvider> provider_;

    void TearDown() override {
        // Cleanup code that will be called after each test
    }
};

// Test VMware provider initialization
TEST_F(BackupProviderTest, Initialize) {
    EXPECT_TRUE(provider_->initialize());
}

// Test KVM provider initialization
TEST_F(BackupProviderTest, KVMProviderInitialization) {
    auto provider = std::make_unique<KVMBackupProvider>();
    EXPECT_TRUE(provider->initialize());
}

// Test VMware provider connection
TEST_F(BackupProviderTest, Connect) {
    EXPECT_TRUE(provider_->connect("localhost", "admin", "password"));
    EXPECT_TRUE(provider_->isConnected());
}

// Test KVM provider connection
TEST_F(BackupProviderTest, KVMProviderConnection) {
    auto provider = std::make_unique<KVMBackupProvider>();
    EXPECT_TRUE(provider->initialize());
    EXPECT_TRUE(provider->connect("localhost", "admin", "password"));
    EXPECT_TRUE(provider->isConnected());
    provider->disconnect();
    EXPECT_FALSE(provider->isConnected());
}

TEST_F(BackupProviderTest, Disconnect) {
    provider_->connect("localhost", "admin", "password");
    provider_->disconnect();
    EXPECT_FALSE(provider_->isConnected());
}

TEST_F(BackupProviderTest, ListVMs) {
    provider_->connect("localhost", "admin", "password");
    auto vms = provider_->listVMs();
    EXPECT_FALSE(vms.empty());
}

TEST_F(BackupProviderTest, GetVMDiskPaths) {
    provider_->connect("localhost", "admin", "password");
    std::vector<std::string> diskPaths;
    EXPECT_TRUE(provider_->getVMDiskPaths("vm-1", diskPaths));
    EXPECT_FALSE(diskPaths.empty());
}

TEST_F(BackupProviderTest, GetVMInfo) {
    provider_->connect("localhost", "admin", "password");
    std::string name, status;
    EXPECT_TRUE(provider_->getVMInfo("vm-1", name, status));
    EXPECT_FALSE(name.empty());
    EXPECT_FALSE(status.empty());
}

TEST_F(BackupProviderTest, CBTOperations) {
    provider_->connect("localhost", "admin", "password");
    EXPECT_TRUE(provider_->enableCBT("vm-1"));
    EXPECT_TRUE(provider_->isCBTEnabled("vm-1"));
    EXPECT_TRUE(provider_->disableCBT("vm-1"));
    EXPECT_FALSE(provider_->isCBTEnabled("vm-1"));
}

TEST_F(BackupProviderTest, GetChangedBlocks) {
    provider_->connect("localhost", "admin", "password");
    std::vector<std::pair<uint64_t, uint64_t>> changedBlocks;
    EXPECT_TRUE(provider_->getChangedBlocks("vm-1", "/path/to/disk.vmdk", changedBlocks));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 