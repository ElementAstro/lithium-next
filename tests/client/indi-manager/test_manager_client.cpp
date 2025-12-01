/*
 * test_manager_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "client/common/server_client.hpp"
#include "client/indi-manager/manager_client.hpp"

using namespace lithium::client;
using namespace lithium::client::indi_manager;

class ManagerClientTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_shared<ManagerClient>("test_indi_manager");
    }

    void TearDown() override {
        if (client_) {
            client_->destroy();
        }
    }

    std::shared_ptr<ManagerClient> client_;
};

TEST_F(ManagerClientTest, CreateClient) {
    EXPECT_NE(client_, nullptr);
    EXPECT_EQ(client_->getName(), "test_indi_manager");
    EXPECT_EQ(client_->getBackendName(), "INDI");
}

TEST_F(ManagerClientTest, InitializeClient) {
    bool result = client_->initialize();
    if (result) {
        EXPECT_EQ(client_->getState(), ClientState::Initialized);
    } else {
        EXPECT_TRUE(client_->getLastError().hasError());
    }
}

TEST_F(ManagerClientTest, DestroyClient) {
    client_->initialize();
    bool result = client_->destroy();
    EXPECT_TRUE(result);
    EXPECT_EQ(client_->getState(), ClientState::Uninitialized);
}

TEST_F(ManagerClientTest, ScanForServers) {
    client_->initialize();
    auto servers = client_->scan();
    EXPECT_FALSE(servers.empty());
    EXPECT_EQ(servers[0], "localhost:7624");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
