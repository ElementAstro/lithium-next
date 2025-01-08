#include <gtest/gtest.h>
#include "task/task.hpp"

using namespace lithium::sequencer;
using json = nlohmann::json;

class TaskTest : public ::testing::Test {
protected:
    void SetUp() override {
        task = std::make_unique<Task>("TestTask", [](const json&) {});
    }

    void TearDown() override { task.reset(); }

    std::unique_ptr<Task> task;
};

TEST_F(TaskTest, ValidateParams_ValidParams) {
    task->addParamDefinition("param1", "string", true);
    task->addParamDefinition("param2", "number", true);
    task->addParamDefinition("param3", "boolean", false, false);

    json params = {{"param1", "value1"}, {"param2", 42}, {"param3", true}};

    ASSERT_TRUE(task->validateParams(params));
    ASSERT_TRUE(task->getParamErrors().empty());
}

TEST_F(TaskTest, ValidateParams_MissingRequiredParams) {
    task->addParamDefinition("param1", "string", true);
    task->addParamDefinition("param2", "number", true);

    json params = {{"param1", "value1"}};

    ASSERT_FALSE(task->validateParams(params));
    ASSERT_FALSE(task->getParamErrors().empty());
    EXPECT_EQ(task->getParamErrors().size(), 1);
    EXPECT_EQ(task->getParamErrors()[0], "Missing required parameter: param2");
}

TEST_F(TaskTest, ValidateParams_InvalidParamType) {
    task->addParamDefinition("param1", "string", true);
    task->addParamDefinition("param2", "number", true);

    json params = {{"param1", "value1"}, {"param2", "not_a_number"}};

    ASSERT_FALSE(task->validateParams(params));
    ASSERT_FALSE(task->getParamErrors().empty());
    EXPECT_EQ(task->getParamErrors().size(), 1);
    EXPECT_EQ(task->getParamErrors()[0],
              "Invalid type for parameter param2: expected number");
}

TEST_F(TaskTest, ValidateParams_OptionalParamsWithDefaultValues) {
    task->addParamDefinition("param1", "string", true);
    task->addParamDefinition("param2", "number", false, 42);

    json params = {{"param1", "value1"}};

    ASSERT_TRUE(task->validateParams(params));
    ASSERT_TRUE(task->getParamErrors().empty());
}

TEST_F(TaskTest, ValidateParams_EmptyParams) {
    task->addParamDefinition("param1", "string", true);
    task->addParamDefinition("param2", "number", true);

    json params = {};

    ASSERT_FALSE(task->validateParams(params));
    ASSERT_FALSE(task->getParamErrors().empty());
    EXPECT_EQ(task->getParamErrors().size(), 2);
    EXPECT_EQ(task->getParamErrors()[0], "Missing required parameter: param1");
    EXPECT_EQ(task->getParamErrors()[1], "Missing required parameter: param2");
}

TEST_F(TaskTest, ValidateParams_ExtraParams) {
    task->addParamDefinition("param1", "string", true);

    json params = {{"param1", "value1"}, {"param2", 42}};

    ASSERT_TRUE(task->validateParams(params));
    ASSERT_TRUE(task->getParamErrors().empty());
}

TEST_F(TaskTest, ValidateParams_NestedParams) {
    task->addParamDefinition("param1", "object", true);

    json params = {{"param1", {{"nested_param", "value"}}}};

    ASSERT_TRUE(task->validateParams(params));
    ASSERT_TRUE(task->getParamErrors().empty());
}

TEST_F(TaskTest, ValidateParams_ArrayParams) {
    task->addParamDefinition("param1", "array", true);

    json params = {{"param1", json::array({1, 2, 3})}};

    ASSERT_TRUE(task->validateParams(params));
    ASSERT_TRUE(task->getParamErrors().empty());
}