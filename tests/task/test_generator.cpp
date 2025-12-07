#include <gtest/gtest.h>

#include "atom/type/json.hpp"
#include "task/core/generator.hpp"

using namespace lithium;

class TaskGeneratorTest : public ::testing::Test {
protected:
    void SetUp() override { generator = TaskGenerator::createShared(); }

    void TearDown() override { generator.reset(); }

    std::shared_ptr<TaskGenerator> generator;
};

TEST_F(TaskGeneratorTest, AddAndRemoveMacro) {
    generator->addMacro(
        "test_macro", [](const std::vector<std::string>& args) -> std::string {
            return "test";
        });
    EXPECT_TRUE(generator->hasMacro("test_macro"));

    generator->removeMacro("test_macro");
    EXPECT_FALSE(generator->hasMacro("test_macro"));
}

TEST_F(TaskGeneratorTest, ListMacros) {
    generator->addMacro(
        "macro1", [](const std::vector<std::string>& args) -> std::string {
            return "macro1";
        });
    generator->addMacro(
        "macro2", [](const std::vector<std::string>& args) -> std::string {
            return "macro2";
        });

    auto macros = generator->listMacros();
    EXPECT_EQ(macros.size(), 2);
    EXPECT_NE(std::find(macros.begin(), macros.end(), "macro1"), macros.end());
    EXPECT_NE(std::find(macros.begin(), macros.end(), "macro2"), macros.end());
}

TEST_F(TaskGeneratorTest, ProcessJson) {
    generator->addMacro(
        "test_macro", [](const std::vector<std::string>& args) -> std::string {
            return "test";
        });

    json j = {{"key1", "${test_macro()}"}};

    generator->processJson(j);
    EXPECT_EQ(j["key1"], "test");
}

TEST_F(TaskGeneratorTest, ProcessJsonWithJsonMacros) {
    json j = {{"macro1", "${test_macro()}"}, {"key1", "${macro1}"}};

    generator->addMacro(
        "test_macro", [](const std::vector<std::string>& args) -> std::string {
            return "test";
        });

    generator->processJsonWithJsonMacros(j);
    EXPECT_EQ(j["key1"], "test");
}

TEST_F(TaskGeneratorTest, ClearMacroCache) {
    generator->addMacro(
        "test_macro", [](const std::vector<std::string>& args) -> std::string {
            return "test";
        });

    json j = {{"key1", "${test_macro()}"}};

    generator->processJson(j);
    EXPECT_EQ(generator->getCacheSize(), 1);

    generator->clearMacroCache();
    EXPECT_EQ(generator->getCacheSize(), 0);
}

TEST_F(TaskGeneratorTest, SetMaxCacheSize) {
    generator->setMaxCacheSize(1);

    generator->addMacro(
        "macro1", [](const std::vector<std::string>& args) -> std::string {
            return "macro1";
        });
    generator->addMacro(
        "macro2", [](const std::vector<std::string>& args) -> std::string {
            return "macro2";
        });

    json j = {{"key1", "${macro1()}"}, {"key2", "${macro2()}"}};

    generator->processJson(j);
    EXPECT_EQ(generator->getCacheSize(), 1);
}

TEST_F(TaskGeneratorTest, GetStatistics) {
    generator->addMacro(
        "test_macro", [](const std::vector<std::string>& args) -> std::string {
            return "test";
        });

    json j = {{"key1", "${test_macro()}"}};

    generator->processJson(j);
    auto stats = generator->getStatistics();
    EXPECT_EQ(stats.cache_hits, 0);
    EXPECT_EQ(stats.cache_misses, 1);
    EXPECT_EQ(stats.macro_evaluations, 1);
}

TEST_F(TaskGeneratorTest, ResetStatistics) {
    generator->addMacro(
        "test_macro", [](const std::vector<std::string>& args) -> std::string {
            return "test";
        });

    json j = {{"key1", "${test_macro()}"}};

    generator->processJson(j);
    generator->resetStatistics();
    auto stats = generator->getStatistics();
    EXPECT_EQ(stats.cache_hits, 0);
    EXPECT_EQ(stats.cache_misses, 0);
    EXPECT_EQ(stats.macro_evaluations, 0);
    EXPECT_EQ(stats.average_evaluation_time, 0.0);
}
