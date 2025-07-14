/**
 * @file test_sequence_manager.cpp
 * @brief Unit tests for the sequence manager
 */

#include "task/sequence_manager.hpp"
#include "task/sequencer.hpp"
#include "task/task.hpp"
#include "task/target.hpp"
#include "task/generator.hpp"
#include "task/exception.hpp"

#include <gtest/gtest.h>
#include <gmock/gmock.h>

using namespace lithium::task;
using json = nlohmann::json;

// Mock task function
class MockTaskFunction {
public:
    MOCK_METHOD(void, Call, (const json&));
};

// Test fixture
class SequenceManagerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create sequence manager with default options
        manager = SequenceManager::createShared();
    }

    void TearDown() override {
        // Clean up
    }

    // Create a simple target for testing
    std::unique_ptr<Target> createTestTarget(const std::string& name, int taskCount) {
        auto target = std::make_unique<Target>(name, std::chrono::seconds(1), 1);

        for (int i = 0; i < taskCount; ++i) {
            auto task = std::make_unique<Task>(
                "TestTask" + std::to_string(i),
                "test",
                [](const json& params) {
                    // Simple task that just logs
                });

            target->addTask(std::move(task));
        }

        return target;
    }

    std::shared_ptr<SequenceManager> manager;
};

// Test creating a sequence
TEST_F(SequenceManagerTest, CreateSequence) {
    auto sequence = manager->createSequence("TestSequence");
    ASSERT_NE(sequence, nullptr);
}

// Test adding targets to sequence
TEST_F(SequenceManagerTest, AddTargets) {
    auto sequence = manager->createSequence("TestSequence");

    // Add targets
    sequence->addTarget(createTestTarget("Target1", 2));
    sequence->addTarget(createTestTarget("Target2", 3));

    // Verify targets added
    auto targetNames = sequence->getTargetNames();
    ASSERT_EQ(targetNames.size(), 2);
    EXPECT_THAT(targetNames, ::testing::UnorderedElementsAre("Target1", "Target2"));
}

// Test sequence template creation
TEST_F(SequenceManagerTest, CreateFromTemplate) {
    // Register a test template
    lithium::TaskGenerator::ScriptTemplate testTemplate{
        .name = "TestTemplate",
        .description = "Test template",
        .content = R"({
            "targets": [
                {
                    "name": "{{targetName}}",
                    "enabled": true,
                    "tasks": [
                        {
                            "name": "TestTask",
                            "type": "test",
                            "params": {
                                "value": {{value}}
                            }
                        }
                    ]
                }
            ]
        })",
        .requiredParams = {"targetName", "value"},
        .parameterSchema = json::parse(R"({
            "targetName": {"type": "string"},
            "value": {"type": "number"}
        })"),
        .category = "Test",
        .version = "1.0.0"
    };

    manager->registerTaskTemplate("TestTemplate", testTemplate);

    // Create from template
    json params = {
        {"targetName", "TemplateTarget"},
        {"value", 42}
    };

    // This will throw if template processing fails
    auto sequence = manager->createSequenceFromTemplate("TestTemplate", params);
    ASSERT_NE(sequence, nullptr);

    // Verify template was applied
    auto targetNames = sequence->getTargetNames();
    ASSERT_EQ(targetNames.size(), 1);
    EXPECT_EQ(targetNames[0], "TemplateTarget");
}

// Test sequence validation
TEST_F(SequenceManagerTest, ValidateSequence) {
    // Valid sequence JSON
    json validJson = json::parse(R"({
        "targets": [
            {
                "name": "ValidTarget",
                "enabled": true,
                "tasks": [
                    {
                        "name": "TestTask",
                        "type": "test",
                        "params": {}
                    }
                ]
            }
        ]
    })");

    // Invalid sequence JSON (missing name)
    json invalidJson = json::parse(R"({
        "targets": [
            {
                "enabled": true,
                "tasks": [
                    {
                        "name": "TestTask",
                        "type": "test",
                        "params": {}
                    }
                ]
            }
        ]
    })");

    // Validate
    std::string errorMsg;
    EXPECT_TRUE(manager->validateSequenceJson(validJson, errorMsg));
    EXPECT_FALSE(manager->validateSequenceJson(invalidJson, errorMsg));
    EXPECT_FALSE(errorMsg.empty());
}

// Test error handling
TEST_F(SequenceManagerTest, ExceptionHandling) {
    // Create a sequence with a task that throws an exception
    auto sequence = manager->createSequence("ErrorSequence");

    auto target = std::make_unique<Target>("ErrorTarget", std::chrono::seconds(1), 0);

    auto task = std::make_unique<Task>(
        "ErrorTask",
        "error_test",
        [](const json& params) {
            throw TaskExecutionException(
                "Deliberate test error",
                "ErrorTask",
                "Testing exception handling");
        });

    target->addTask(std::move(task));
    sequence->addTarget(std::move(target));

    // Execute and expect exception
    auto result = manager->executeSequence(sequence, false);

    ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->success);
    EXPECT_EQ(result->completedTargets.size(), 0);
    EXPECT_EQ(result->failedTargets.size(), 1);
    EXPECT_EQ(result->failedTargets[0], "ErrorTarget");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
