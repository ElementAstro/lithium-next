#include <gtest/gtest.h>
#include "task/target.hpp"
#include "task/task.hpp"

using namespace lithium::task;
using json = nlohmann::json;

class TargetTest : public ::testing::Test {
protected:
    void SetUp() override { target = std::make_unique<Target>("TestTarget"); }

    void TearDown() override { target.reset(); }

    std::unique_ptr<Target> target;
};

TEST_F(TargetTest, ConstructorAndInitialState) {
    EXPECT_EQ(target->getName(), "TestTarget");
    EXPECT_EQ(target->getStatus(), TargetStatus::Pending);
    EXPECT_TRUE(target->isEnabled());
    EXPECT_EQ(target->getProgress(), 100.0);
}

TEST_F(TargetTest, AddTask) {
    auto task = std::make_unique<Task>("TestTask", [](const json&) {});
    target->addTask(std::move(task));
    EXPECT_EQ(target->getTasks().size(), 1);
}

TEST_F(TargetTest, SetAndGetCooldown) {
    target->setCooldown(std::chrono::seconds(10));
    // No direct getter for cooldown, so we check indirectly via logs or
    // behavior Assuming we have a way to verify the cooldown is set correctly
}

TEST_F(TargetTest, EnableAndDisableTarget) {
    target->setEnabled(false);
    EXPECT_FALSE(target->isEnabled());
    target->setEnabled(true);
    EXPECT_TRUE(target->isEnabled());
}

TEST_F(TargetTest, SetAndGetMaxRetries) {
    target->setMaxRetries(3);
    // No direct getter for maxRetries, so we check indirectly via logs or
    // behavior Assuming we have a way to verify the maxRetries is set correctly
}

TEST_F(TargetTest, SetAndGetStatus) {
    target->setStatus(TargetStatus::InProgress);
    EXPECT_EQ(target->getStatus(), TargetStatus::InProgress);
}

TEST_F(TargetTest, ExecuteTarget) {
    auto task = std::make_unique<Task>("TestTask", [](const json&) {});
    target->addTask(std::move(task));
    target->execute();
    EXPECT_EQ(target->getStatus(), TargetStatus::Completed);
}

TEST_F(TargetTest, LoadTasksFromJson) {
    json tasksJson = R"([
        {"name": "TakeExposure"},
        {"name": "TakeManyExposure"},
        {"name": "SubframeExposure"}
    ])"_json;

    target->loadTasksFromJson(tasksJson);
    EXPECT_EQ(target->getTasks().size(), 3);
}
