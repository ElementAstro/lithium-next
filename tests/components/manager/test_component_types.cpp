/**
 * @file test_component_types.cpp
 * @brief Unit tests for ComponentEvent, ComponentState, and ComponentOptions
 *
 * Tests the types defined in components/manager/types.hpp
 */

#include <gtest/gtest.h>
#include <string>

#include "components/manager/types.hpp"

namespace lithium::test {

// ============================================================================
// ComponentEvent Tests
// ============================================================================

class ComponentEventTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ComponentEventTest, EventValues) {
    // Test that all event types are distinct
    EXPECT_NE(ComponentEvent::PreLoad, ComponentEvent::PostLoad);
    EXPECT_NE(ComponentEvent::PreUnload, ComponentEvent::PostUnload);
    EXPECT_NE(ComponentEvent::ConfigChanged, ComponentEvent::StateChanged);
    EXPECT_NE(ComponentEvent::Error, ComponentEvent::PreLoad);
}

TEST_F(ComponentEventTest, EventAssignment) {
    ComponentEvent event = ComponentEvent::PreLoad;
    EXPECT_EQ(event, ComponentEvent::PreLoad);

    event = ComponentEvent::PostLoad;
    EXPECT_EQ(event, ComponentEvent::PostLoad);

    event = ComponentEvent::PreUnload;
    EXPECT_EQ(event, ComponentEvent::PreUnload);

    event = ComponentEvent::PostUnload;
    EXPECT_EQ(event, ComponentEvent::PostUnload);

    event = ComponentEvent::ConfigChanged;
    EXPECT_EQ(event, ComponentEvent::ConfigChanged);

    event = ComponentEvent::StateChanged;
    EXPECT_EQ(event, ComponentEvent::StateChanged);

    event = ComponentEvent::Error;
    EXPECT_EQ(event, ComponentEvent::Error);
}

TEST_F(ComponentEventTest, EventComparison) {
    ComponentEvent e1 = ComponentEvent::PreLoad;
    ComponentEvent e2 = ComponentEvent::PreLoad;
    ComponentEvent e3 = ComponentEvent::PostLoad;

    EXPECT_EQ(e1, e2);
    EXPECT_NE(e1, e3);
}

TEST_F(ComponentEventTest, EventInSwitch) {
    ComponentEvent event = ComponentEvent::StateChanged;
    std::string result;

    switch (event) {
        case ComponentEvent::PreLoad:
            result = "PreLoad";
            break;
        case ComponentEvent::PostLoad:
            result = "PostLoad";
            break;
        case ComponentEvent::PreUnload:
            result = "PreUnload";
            break;
        case ComponentEvent::PostUnload:
            result = "PostUnload";
            break;
        case ComponentEvent::ConfigChanged:
            result = "ConfigChanged";
            break;
        case ComponentEvent::StateChanged:
            result = "StateChanged";
            break;
        case ComponentEvent::Error:
            result = "Error";
            break;
    }

    EXPECT_EQ(result, "StateChanged");
}

// ============================================================================
// ComponentState Tests
// ============================================================================

class ComponentStateTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ComponentStateTest, StateValues) {
    // Test that all state types are distinct
    EXPECT_NE(ComponentState::Created, ComponentState::Initialized);
    EXPECT_NE(ComponentState::Running, ComponentState::Paused);
    EXPECT_NE(ComponentState::Stopped, ComponentState::Error);
}

TEST_F(ComponentStateTest, StateAssignment) {
    ComponentState state = ComponentState::Created;
    EXPECT_EQ(state, ComponentState::Created);

    state = ComponentState::Initialized;
    EXPECT_EQ(state, ComponentState::Initialized);

    state = ComponentState::Running;
    EXPECT_EQ(state, ComponentState::Running);

    state = ComponentState::Paused;
    EXPECT_EQ(state, ComponentState::Paused);

    state = ComponentState::Stopped;
    EXPECT_EQ(state, ComponentState::Stopped);

    state = ComponentState::Error;
    EXPECT_EQ(state, ComponentState::Error);
}

TEST_F(ComponentStateTest, StateComparison) {
    ComponentState s1 = ComponentState::Running;
    ComponentState s2 = ComponentState::Running;
    ComponentState s3 = ComponentState::Paused;

    EXPECT_EQ(s1, s2);
    EXPECT_NE(s1, s3);
}

TEST_F(ComponentStateTest, StateInSwitch) {
    ComponentState state = ComponentState::Running;
    std::string result;

    switch (state) {
        case ComponentState::Created:
            result = "Created";
            break;
        case ComponentState::Initialized:
            result = "Initialized";
            break;
        case ComponentState::Running:
            result = "Running";
            break;
        case ComponentState::Paused:
            result = "Paused";
            break;
        case ComponentState::Stopped:
            result = "Stopped";
            break;
        case ComponentState::Error:
            result = "Error";
            break;
    }

    EXPECT_EQ(result, "Running");
}

TEST_F(ComponentStateTest, StateTransitions) {
    // Simulate state transitions
    ComponentState state = ComponentState::Created;
    EXPECT_EQ(state, ComponentState::Created);

    state = ComponentState::Initialized;
    EXPECT_EQ(state, ComponentState::Initialized);

    state = ComponentState::Running;
    EXPECT_EQ(state, ComponentState::Running);

    state = ComponentState::Paused;
    EXPECT_EQ(state, ComponentState::Paused);

    state = ComponentState::Running;
    EXPECT_EQ(state, ComponentState::Running);

    state = ComponentState::Stopped;
    EXPECT_EQ(state, ComponentState::Stopped);
}

// ============================================================================
// ComponentOptions Tests
// ============================================================================

class ComponentOptionsTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(ComponentOptionsTest, DefaultValues) {
    ComponentOptions options;

    EXPECT_TRUE(options.autoStart);
    EXPECT_FALSE(options.lazy);
    EXPECT_EQ(options.priority, 0);
    EXPECT_TRUE(options.group.empty());
    EXPECT_TRUE(options.config.is_null());
}

TEST_F(ComponentOptionsTest, SetAutoStart) {
    ComponentOptions options;

    options.autoStart = false;
    EXPECT_FALSE(options.autoStart);

    options.autoStart = true;
    EXPECT_TRUE(options.autoStart);
}

TEST_F(ComponentOptionsTest, SetLazy) {
    ComponentOptions options;

    options.lazy = true;
    EXPECT_TRUE(options.lazy);

    options.lazy = false;
    EXPECT_FALSE(options.lazy);
}

TEST_F(ComponentOptionsTest, SetPriority) {
    ComponentOptions options;

    options.priority = 10;
    EXPECT_EQ(options.priority, 10);

    options.priority = -5;
    EXPECT_EQ(options.priority, -5);

    options.priority = 0;
    EXPECT_EQ(options.priority, 0);
}

TEST_F(ComponentOptionsTest, SetGroup) {
    ComponentOptions options;

    options.group = "core";
    EXPECT_EQ(options.group, "core");

    options.group = "plugins";
    EXPECT_EQ(options.group, "plugins");

    options.group = "";
    EXPECT_TRUE(options.group.empty());
}

TEST_F(ComponentOptionsTest, SetConfig) {
    ComponentOptions options;

    options.config = {{"key1", "value1"}, {"key2", 42}};

    EXPECT_FALSE(options.config.is_null());
    EXPECT_EQ(options.config["key1"], "value1");
    EXPECT_EQ(options.config["key2"], 42);
}

TEST_F(ComponentOptionsTest, CompleteOptions) {
    ComponentOptions options;
    options.autoStart = false;
    options.lazy = true;
    options.priority = 100;
    options.group = "high-priority";
    options.config = {
        {"setting1", true}, {"setting2", "value"}, {"setting3", 3.14}};

    EXPECT_FALSE(options.autoStart);
    EXPECT_TRUE(options.lazy);
    EXPECT_EQ(options.priority, 100);
    EXPECT_EQ(options.group, "high-priority");
    EXPECT_TRUE(options.config["setting1"]);
    EXPECT_EQ(options.config["setting2"], "value");
    EXPECT_DOUBLE_EQ(options.config["setting3"], 3.14);
}

TEST_F(ComponentOptionsTest, NestedConfig) {
    ComponentOptions options;
    options.config = {
        {"database",
         {{"host", "localhost"}, {"port", 5432}, {"name", "testdb"}}},
        {"cache", {{"enabled", true}, {"ttl", 3600}}}};

    EXPECT_EQ(options.config["database"]["host"], "localhost");
    EXPECT_EQ(options.config["database"]["port"], 5432);
    EXPECT_TRUE(options.config["cache"]["enabled"]);
}

TEST_F(ComponentOptionsTest, ArrayConfig) {
    ComponentOptions options;
    options.config = {
        {"endpoints", json::array({"http://api1.com", "http://api2.com"})},
        {"ports", json::array({8080, 8081, 8082})}};

    EXPECT_EQ(options.config["endpoints"].size(), 2);
    EXPECT_EQ(options.config["ports"].size(), 3);
    EXPECT_EQ(options.config["endpoints"][0], "http://api1.com");
    EXPECT_EQ(options.config["ports"][0], 8080);
}

TEST_F(ComponentOptionsTest, EmptyConfig) {
    ComponentOptions options;
    options.config = json::object();

    EXPECT_TRUE(options.config.is_object());
    EXPECT_TRUE(options.config.empty());
}

TEST_F(ComponentOptionsTest, CopyOptions) {
    ComponentOptions original;
    original.autoStart = false;
    original.lazy = true;
    original.priority = 50;
    original.group = "test-group";
    original.config = {{"key", "value"}};

    ComponentOptions copy = original;

    EXPECT_EQ(copy.autoStart, original.autoStart);
    EXPECT_EQ(copy.lazy, original.lazy);
    EXPECT_EQ(copy.priority, original.priority);
    EXPECT_EQ(copy.group, original.group);
    EXPECT_EQ(copy.config, original.config);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST_F(ComponentOptionsTest, HighPriority) {
    ComponentOptions options;
    options.priority = 999999;
    EXPECT_EQ(options.priority, 999999);
}

TEST_F(ComponentOptionsTest, NegativePriority) {
    ComponentOptions options;
    options.priority = -999999;
    EXPECT_EQ(options.priority, -999999);
}

TEST_F(ComponentOptionsTest, LongGroupName) {
    ComponentOptions options;
    std::string longGroup(1000, 'g');
    options.group = longGroup;
    EXPECT_EQ(options.group.length(), 1000);
}

TEST_F(ComponentOptionsTest, SpecialCharsInGroup) {
    ComponentOptions options;
    options.group = "group-with_special.chars@v1";
    EXPECT_EQ(options.group, "group-with_special.chars@v1");
}

TEST_F(ComponentOptionsTest, LargeConfig) {
    ComponentOptions options;

    for (int i = 0; i < 100; ++i) {
        options.config["key_" + std::to_string(i)] =
            "value_" + std::to_string(i);
    }

    EXPECT_EQ(options.config.size(), 100);
}

}  // namespace lithium::test
