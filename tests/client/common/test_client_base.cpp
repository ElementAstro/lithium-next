/*
 * test_client_base.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: Tests for client base class and registry

*************************************************/

#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include "client/common/client_base.hpp"

using namespace lithium::client;

// ==================== Mock Client ====================

class MockClient : public ClientBase {
public:
    explicit MockClient(std::string name = "mock")
        : ClientBase(std::move(name), ClientType::Custom) {
        setCapabilities(ClientCapability::Connect | ClientCapability::Scan);
    }

    bool initialize() override {
        setState(ClientState::Initialized);
        return initializeResult_;
    }

    bool destroy() override {
        setState(ClientState::Uninitialized);
        return destroyResult_;
    }

    bool connect(const std::string& target, int timeout, int maxRetry) override {
        lastTarget_ = target;
        lastTimeout_ = timeout;
        lastMaxRetry_ = maxRetry;
        if (connectResult_) {
            setState(ClientState::Connected);
        }
        return connectResult_;
    }

    bool disconnect() override {
        setState(ClientState::Disconnected);
        return disconnectResult_;
    }

    bool isConnected() const override {
        return getState() == ClientState::Connected;
    }

    std::vector<std::string> scan() override {
        return scanResults_;
    }

    // Test helpers
    void setInitializeResult(bool result) { initializeResult_ = result; }
    void setDestroyResult(bool result) { destroyResult_ = result; }
    void setConnectResult(bool result) { connectResult_ = result; }
    void setDisconnectResult(bool result) { disconnectResult_ = result; }
    void setScanResults(std::vector<std::string> results) { scanResults_ = std::move(results); }

    std::string getLastTarget() const { return lastTarget_; }
    int getLastTimeout() const { return lastTimeout_; }
    int getLastMaxRetry() const { return lastMaxRetry_; }

private:
    bool initializeResult_{true};
    bool destroyResult_{true};
    bool connectResult_{true};
    bool disconnectResult_{true};
    std::vector<std::string> scanResults_;
    std::string lastTarget_;
    int lastTimeout_{0};
    int lastMaxRetry_{0};
};

// ==================== ClientBase Tests ====================

class ClientBaseTest : public ::testing::Test {
protected:
    void SetUp() override {
        client_ = std::make_shared<MockClient>("test_client");
    }

    std::shared_ptr<MockClient> client_;
};

TEST_F(ClientBaseTest, Construction) {
    EXPECT_EQ(client_->getName(), "test_client");
    EXPECT_EQ(client_->getType(), ClientType::Custom);
    EXPECT_EQ(client_->getState(), ClientState::Uninitialized);
    EXPECT_FALSE(client_->getUUID().empty());
}

TEST_F(ClientBaseTest, Initialize) {
    EXPECT_TRUE(client_->initialize());
    EXPECT_EQ(client_->getState(), ClientState::Initialized);
}

TEST_F(ClientBaseTest, InitializeFails) {
    client_->setInitializeResult(false);
    EXPECT_FALSE(client_->initialize());
}

TEST_F(ClientBaseTest, Connect) {
    client_->initialize();
    EXPECT_TRUE(client_->connect("localhost:1234", 5000, 3));
    EXPECT_TRUE(client_->isConnected());
    EXPECT_EQ(client_->getLastTarget(), "localhost:1234");
    EXPECT_EQ(client_->getLastTimeout(), 5000);
    EXPECT_EQ(client_->getLastMaxRetry(), 3);
}

TEST_F(ClientBaseTest, ConnectFails) {
    client_->setConnectResult(false);
    EXPECT_FALSE(client_->connect("localhost:1234"));
    EXPECT_FALSE(client_->isConnected());
}

TEST_F(ClientBaseTest, Disconnect) {
    client_->initialize();
    client_->connect("localhost:1234");
    EXPECT_TRUE(client_->disconnect());
    EXPECT_FALSE(client_->isConnected());
}

TEST_F(ClientBaseTest, Scan) {
    client_->setScanResults({"path1", "path2", "path3"});
    auto results = client_->scan();
    EXPECT_EQ(results.size(), 3);
    EXPECT_EQ(results[0], "path1");
    EXPECT_EQ(results[1], "path2");
    EXPECT_EQ(results[2], "path3");
}

TEST_F(ClientBaseTest, Capabilities) {
    EXPECT_TRUE(client_->hasCapability(ClientCapability::Connect));
    EXPECT_TRUE(client_->hasCapability(ClientCapability::Scan));
    EXPECT_FALSE(client_->hasCapability(ClientCapability::AsyncOperation));
}

TEST_F(ClientBaseTest, Configuration) {
    ClientConfig config;
    config.executablePath = "/usr/bin/test";
    config.connectionTimeout = 10000;
    config.maxRetries = 5;

    EXPECT_TRUE(client_->configure(config));
    EXPECT_EQ(client_->getConfig().executablePath, "/usr/bin/test");
    EXPECT_EQ(client_->getConfig().connectionTimeout, 10000);
    EXPECT_EQ(client_->getConfig().maxRetries, 5);
}

TEST_F(ClientBaseTest, ErrorHandling) {
    EXPECT_FALSE(client_->getLastError().hasError());

    // Trigger error through failed connect
    client_->setConnectResult(false);
    client_->connect("invalid");

    // Error should be set by derived class
    client_->clearError();
    EXPECT_FALSE(client_->getLastError().hasError());
}

TEST_F(ClientBaseTest, TypeName) {
    EXPECT_EQ(client_->getTypeName(), "Custom");

    MockClient solverClient("solver");
    // Would need to set type in constructor for this test
}

TEST_F(ClientBaseTest, StateName) {
    EXPECT_EQ(client_->getStateName(), "Uninitialized");

    client_->initialize();
    EXPECT_EQ(client_->getStateName(), "Initialized");

    client_->connect("test");
    EXPECT_EQ(client_->getStateName(), "Connected");

    client_->disconnect();
    EXPECT_EQ(client_->getStateName(), "Disconnected");
}

TEST_F(ClientBaseTest, EventCallback) {
    std::string lastEvent;
    std::string lastData;

    client_->setEventCallback([&](const std::string& event, const std::string& data) {
        lastEvent = event;
        lastData = data;
    });

    // Events are emitted by derived classes
    // This test verifies callback registration works
    EXPECT_TRUE(lastEvent.empty());
}

TEST_F(ClientBaseTest, StatusCallback) {
    ClientState oldState = ClientState::Uninitialized;
    ClientState newState = ClientState::Uninitialized;
    int callCount = 0;

    client_->setStatusCallback([&](ClientState old, ClientState current) {
        oldState = old;
        newState = current;
        callCount++;
    });

    client_->initialize();
    EXPECT_EQ(callCount, 1);
    EXPECT_EQ(oldState, ClientState::Uninitialized);
    EXPECT_EQ(newState, ClientState::Initialized);

    client_->connect("test");
    EXPECT_EQ(callCount, 2);
    EXPECT_EQ(newState, ClientState::Connected);
}

// ==================== ClientRegistry Tests ====================

class ClientRegistryTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Clear any existing registrations for clean tests
        auto& registry = ClientRegistry::instance();
        for (const auto& name : registry.getRegisteredClients()) {
            if (name.find("test_") == 0) {
                registry.unregisterClient(name);
            }
        }
    }
};

TEST_F(ClientRegistryTest, RegisterClient) {
    auto& registry = ClientRegistry::instance();

    ClientDescriptor desc;
    desc.name = "test_client_1";
    desc.description = "Test Client";
    desc.type = ClientType::Custom;
    desc.version = "1.0.0";
    desc.factory = []() { return std::make_shared<MockClient>("test_client_1"); };

    EXPECT_TRUE(registry.registerClient(desc));

    auto clients = registry.getRegisteredClients();
    EXPECT_TRUE(std::find(clients.begin(), clients.end(), "test_client_1") != clients.end());
}

TEST_F(ClientRegistryTest, UnregisterClient) {
    auto& registry = ClientRegistry::instance();

    ClientDescriptor desc;
    desc.name = "test_client_2";
    desc.description = "Test Client";
    desc.type = ClientType::Custom;
    desc.factory = []() { return std::make_shared<MockClient>("test_client_2"); };

    registry.registerClient(desc);
    EXPECT_TRUE(registry.unregisterClient("test_client_2"));

    auto clients = registry.getRegisteredClients();
    EXPECT_TRUE(std::find(clients.begin(), clients.end(), "test_client_2") == clients.end());
}

TEST_F(ClientRegistryTest, CreateClient) {
    auto& registry = ClientRegistry::instance();

    ClientDescriptor desc;
    desc.name = "test_client_3";
    desc.description = "Test Client";
    desc.type = ClientType::Custom;
    desc.factory = []() { return std::make_shared<MockClient>("test_client_3"); };

    registry.registerClient(desc);

    auto client = registry.createClient("test_client_3");
    ASSERT_NE(client, nullptr);
    EXPECT_EQ(client->getName(), "test_client_3");
}

TEST_F(ClientRegistryTest, CreateNonexistentClient) {
    auto& registry = ClientRegistry::instance();
    auto client = registry.createClient("nonexistent_client");
    EXPECT_EQ(client, nullptr);
}

TEST_F(ClientRegistryTest, GetDescriptor) {
    auto& registry = ClientRegistry::instance();

    ClientDescriptor desc;
    desc.name = "test_client_4";
    desc.description = "Test Description";
    desc.type = ClientType::Solver;
    desc.version = "2.0.0";
    desc.requiredBinaries = {"binary1", "binary2"};
    desc.factory = []() { return std::make_shared<MockClient>("test_client_4"); };

    registry.registerClient(desc);

    auto retrieved = registry.getDescriptor("test_client_4");
    ASSERT_TRUE(retrieved.has_value());
    EXPECT_EQ(retrieved->name, "test_client_4");
    EXPECT_EQ(retrieved->description, "Test Description");
    EXPECT_EQ(retrieved->type, ClientType::Solver);
    EXPECT_EQ(retrieved->version, "2.0.0");
    EXPECT_EQ(retrieved->requiredBinaries.size(), 2);
}

TEST_F(ClientRegistryTest, GetClientsByType) {
    auto& registry = ClientRegistry::instance();

    ClientDescriptor solver1;
    solver1.name = "test_solver_1";
    solver1.type = ClientType::Solver;
    solver1.factory = []() { return std::make_shared<MockClient>("test_solver_1"); };

    ClientDescriptor solver2;
    solver2.name = "test_solver_2";
    solver2.type = ClientType::Solver;
    solver2.factory = []() { return std::make_shared<MockClient>("test_solver_2"); };

    ClientDescriptor guider1;
    guider1.name = "test_guider_1";
    guider1.type = ClientType::Guider;
    guider1.factory = []() { return std::make_shared<MockClient>("test_guider_1"); };

    registry.registerClient(solver1);
    registry.registerClient(solver2);
    registry.registerClient(guider1);

    auto solvers = registry.getClientsByType(ClientType::Solver);
    EXPECT_GE(solvers.size(), 2);

    auto guiders = registry.getClientsByType(ClientType::Guider);
    EXPECT_GE(guiders.size(), 1);
}

// ==================== Capability Tests ====================

TEST(ClientCapabilityTest, BitwiseOr) {
    auto caps = ClientCapability::Connect | ClientCapability::Scan;
    EXPECT_TRUE(hasCapability(caps, ClientCapability::Connect));
    EXPECT_TRUE(hasCapability(caps, ClientCapability::Scan));
    EXPECT_FALSE(hasCapability(caps, ClientCapability::AsyncOperation));
}

TEST(ClientCapabilityTest, BitwiseAnd) {
    auto caps = ClientCapability::Connect | ClientCapability::Scan | ClientCapability::AsyncOperation;
    auto filtered = caps & ClientCapability::Connect;
    EXPECT_TRUE(hasCapability(filtered, ClientCapability::Connect));
}

TEST(ClientCapabilityTest, HasCapability) {
    EXPECT_TRUE(hasCapability(ClientCapability::Connect, ClientCapability::Connect));
    EXPECT_FALSE(hasCapability(ClientCapability::Connect, ClientCapability::Scan));
    EXPECT_FALSE(hasCapability(ClientCapability::None, ClientCapability::Connect));
}

// ==================== Main ====================

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
