/**
 * @file test_module_info.cpp
 * @brief Unit tests for ModuleInfo and FunctionInfo structures
 *
 * Tests the module information structures defined in components/module.hpp
 */

#include <gtest/gtest.h>
#include <chrono>
#include <memory>
#include <string>
#include <vector>

#include "components/core/module.hpp"

namespace lithium::test {

// ============================================================================
// FunctionInfo Tests
// ============================================================================

class FunctionInfoTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
};

TEST_F(FunctionInfoTest, DefaultConstructor) {
    FunctionInfo info;
    EXPECT_TRUE(info.name.empty());
    EXPECT_EQ(info.address, nullptr);
    EXPECT_TRUE(info.parameters.empty());
}

TEST_F(FunctionInfoTest, SetName) {
    FunctionInfo info;
    info.name = "testFunction";
    EXPECT_EQ(info.name, "testFunction");
}

TEST_F(FunctionInfoTest, SetAddress) {
    FunctionInfo info;
    int dummy = 42;
    info.address = &dummy;
    EXPECT_EQ(info.address, &dummy);
}

TEST_F(FunctionInfoTest, SetParameters) {
    FunctionInfo info;
    info.parameters = {"int", "std::string", "double"};
    EXPECT_EQ(info.parameters.size(), 3);
    EXPECT_EQ(info.parameters[0], "int");
    EXPECT_EQ(info.parameters[1], "std::string");
    EXPECT_EQ(info.parameters[2], "double");
}

TEST_F(FunctionInfoTest, CompleteInitialization) {
    FunctionInfo info;
    info.name = "complexFunction";
    int dummy = 0;
    info.address = &dummy;
    info.parameters = {"param1", "param2"};

    EXPECT_EQ(info.name, "complexFunction");
    EXPECT_NE(info.address, nullptr);
    EXPECT_EQ(info.parameters.size(), 2);
}

TEST_F(FunctionInfoTest, EmptyParameters) {
    FunctionInfo info;
    info.name = "noParamFunction";
    EXPECT_TRUE(info.parameters.empty());
}

TEST_F(FunctionInfoTest, ManyParameters) {
    FunctionInfo info;
    for (int i = 0; i < 100; ++i) {
        info.parameters.push_back("param" + std::to_string(i));
    }
    EXPECT_EQ(info.parameters.size(), 100);
}

// ============================================================================
// ModuleInfo Tests
// ============================================================================

class ModuleInfoTest : public ::testing::Test {
protected:
    void SetUp() override { moduleInfo = std::make_unique<ModuleInfo>(); }

    void TearDown() override { moduleInfo.reset(); }

    std::unique_ptr<ModuleInfo> moduleInfo;
};

TEST_F(ModuleInfoTest, DefaultValues) {
    EXPECT_TRUE(moduleInfo->name.empty());
    EXPECT_TRUE(moduleInfo->description.empty());
    EXPECT_TRUE(moduleInfo->version.empty());
    EXPECT_TRUE(moduleInfo->status.empty());
    EXPECT_TRUE(moduleInfo->type.empty());
    EXPECT_TRUE(moduleInfo->author.empty());
    EXPECT_TRUE(moduleInfo->license.empty());
    EXPECT_TRUE(moduleInfo->path.empty());
    EXPECT_TRUE(moduleInfo->configPath.empty());
    EXPECT_TRUE(moduleInfo->configFile.empty());
    EXPECT_TRUE(moduleInfo->functions.empty());
    EXPECT_TRUE(moduleInfo->dependencies.empty());
    EXPECT_EQ(moduleInfo->priority, 0);
}

TEST_F(ModuleInfoTest, SetBasicInfo) {
    moduleInfo->name = "TestModule";
    moduleInfo->description = "A test module";
    moduleInfo->version = "1.0.0";
    moduleInfo->author = "Test Author";
    moduleInfo->license = "MIT";

    EXPECT_EQ(moduleInfo->name, "TestModule");
    EXPECT_EQ(moduleInfo->description, "A test module");
    EXPECT_EQ(moduleInfo->version, "1.0.0");
    EXPECT_EQ(moduleInfo->author, "Test Author");
    EXPECT_EQ(moduleInfo->license, "MIT");
}

TEST_F(ModuleInfoTest, SetPaths) {
    moduleInfo->path = "/usr/lib/modules/test.so";
    moduleInfo->configPath = "/etc/modules/";
    moduleInfo->configFile = "test.json";

    EXPECT_EQ(moduleInfo->path, "/usr/lib/modules/test.so");
    EXPECT_EQ(moduleInfo->configPath, "/etc/modules/");
    EXPECT_EQ(moduleInfo->configFile, "test.json");
}

TEST_F(ModuleInfoTest, EnabledFlag) {
    moduleInfo->enabled = true;
    EXPECT_TRUE(moduleInfo->enabled.load());

    moduleInfo->enabled = false;
    EXPECT_FALSE(moduleInfo->enabled.load());
}

TEST_F(ModuleInfoTest, AtomicEnabledOperations) {
    moduleInfo->enabled.store(true);
    EXPECT_TRUE(moduleInfo->enabled.load());

    moduleInfo->enabled.store(false);
    EXPECT_FALSE(moduleInfo->enabled.load());

    // Test exchange
    bool oldValue = moduleInfo->enabled.exchange(true);
    EXPECT_FALSE(oldValue);
    EXPECT_TRUE(moduleInfo->enabled.load());
}

TEST_F(ModuleInfoTest, AddFunctions) {
    auto func1 = std::make_unique<FunctionInfo>();
    func1->name = "function1";

    auto func2 = std::make_unique<FunctionInfo>();
    func2->name = "function2";

    moduleInfo->functions.push_back(std::move(func1));
    moduleInfo->functions.push_back(std::move(func2));

    EXPECT_EQ(moduleInfo->functions.size(), 2);
    EXPECT_EQ(moduleInfo->functions[0]->name, "function1");
    EXPECT_EQ(moduleInfo->functions[1]->name, "function2");
}

TEST_F(ModuleInfoTest, AddDependencies) {
    moduleInfo->dependencies = {"dep1", "dep2", "dep3"};

    EXPECT_EQ(moduleInfo->dependencies.size(), 3);
    EXPECT_EQ(moduleInfo->dependencies[0], "dep1");
    EXPECT_EQ(moduleInfo->dependencies[1], "dep2");
    EXPECT_EQ(moduleInfo->dependencies[2], "dep3");
}

TEST_F(ModuleInfoTest, SetLoadTime) {
    auto now = std::chrono::system_clock::now();
    moduleInfo->loadTime = now;

    EXPECT_EQ(moduleInfo->loadTime, now);
}

TEST_F(ModuleInfoTest, SetHash) {
    moduleInfo->hash = 12345678;
    EXPECT_EQ(moduleInfo->hash, 12345678);
}

TEST_F(ModuleInfoTest, StatusEnum) {
    moduleInfo->currentStatus = ModuleInfo::Status::UNLOADED;
    EXPECT_EQ(moduleInfo->currentStatus, ModuleInfo::Status::UNLOADED);

    moduleInfo->currentStatus = ModuleInfo::Status::LOADING;
    EXPECT_EQ(moduleInfo->currentStatus, ModuleInfo::Status::LOADING);

    moduleInfo->currentStatus = ModuleInfo::Status::LOADED;
    EXPECT_EQ(moduleInfo->currentStatus, ModuleInfo::Status::LOADED);

    moduleInfo->currentStatus = ModuleInfo::Status::ERROR;
    EXPECT_EQ(moduleInfo->currentStatus, ModuleInfo::Status::ERROR);
}

TEST_F(ModuleInfoTest, LastError) {
    moduleInfo->lastError = "Failed to load module";
    EXPECT_EQ(moduleInfo->lastError, "Failed to load module");

    moduleInfo->lastError.clear();
    EXPECT_TRUE(moduleInfo->lastError.empty());
}

TEST_F(ModuleInfoTest, Priority) {
    moduleInfo->priority = 10;
    EXPECT_EQ(moduleInfo->priority, 10);

    moduleInfo->priority = -5;
    EXPECT_EQ(moduleInfo->priority, -5);
}

// ============================================================================
// ModuleInfo::Statistics Tests
// ============================================================================

class ModuleStatisticsTest : public ::testing::Test {
protected:
    void SetUp() override {
        stats = std::make_unique<ModuleInfo::Statistics>();
    }

    void TearDown() override { stats.reset(); }

    std::unique_ptr<ModuleInfo::Statistics> stats;
};

TEST_F(ModuleStatisticsTest, DefaultValues) {
    EXPECT_EQ(stats->functionCalls, 0);
    EXPECT_EQ(stats->errors, 0);
    EXPECT_DOUBLE_EQ(stats->avgResponseTime, 0.0);
}

TEST_F(ModuleStatisticsTest, IncrementFunctionCalls) {
    stats->functionCalls = 100;
    EXPECT_EQ(stats->functionCalls, 100);

    stats->functionCalls++;
    EXPECT_EQ(stats->functionCalls, 101);
}

TEST_F(ModuleStatisticsTest, IncrementErrors) {
    stats->errors = 5;
    EXPECT_EQ(stats->errors, 5);

    stats->errors++;
    EXPECT_EQ(stats->errors, 6);
}

TEST_F(ModuleStatisticsTest, SetAverageResponseTime) {
    stats->avgResponseTime = 15.5;
    EXPECT_DOUBLE_EQ(stats->avgResponseTime, 15.5);
}

TEST_F(ModuleStatisticsTest, SetAverageLoadTime) {
    stats->averageLoadTime = 100.0;
    EXPECT_DOUBLE_EQ(stats->averageLoadTime, 100.0);
}

TEST_F(ModuleStatisticsTest, SetLoadCount) {
    stats->loadCount = 50;
    EXPECT_EQ(stats->loadCount, 50);
}

TEST_F(ModuleStatisticsTest, SetFailureCount) {
    stats->failureCount = 3;
    EXPECT_EQ(stats->failureCount, 3);
}

TEST_F(ModuleStatisticsTest, SetLastAccess) {
    auto now = std::chrono::system_clock::now();
    stats->lastAccess = now;
    EXPECT_EQ(stats->lastAccess, now);
}

TEST_F(ModuleStatisticsTest, CompleteStatistics) {
    stats->functionCalls = 1000;
    stats->errors = 10;
    stats->avgResponseTime = 5.5;
    stats->averageLoadTime = 200.0;
    stats->loadCount = 100;
    stats->failureCount = 5;
    stats->lastAccess = std::chrono::system_clock::now();

    EXPECT_EQ(stats->functionCalls, 1000);
    EXPECT_EQ(stats->errors, 10);
    EXPECT_DOUBLE_EQ(stats->avgResponseTime, 5.5);
    EXPECT_DOUBLE_EQ(stats->averageLoadTime, 200.0);
    EXPECT_EQ(stats->loadCount, 100);
    EXPECT_EQ(stats->failureCount, 5);
}

// ============================================================================
// ModuleInfo with Statistics Tests
// ============================================================================

TEST_F(ModuleInfoTest, ModuleWithStatistics) {
    moduleInfo->name = "StatsModule";
    moduleInfo->stats.functionCalls = 500;
    moduleInfo->stats.errors = 2;
    moduleInfo->stats.avgResponseTime = 10.0;

    EXPECT_EQ(moduleInfo->name, "StatsModule");
    EXPECT_EQ(moduleInfo->stats.functionCalls, 500);
    EXPECT_EQ(moduleInfo->stats.errors, 2);
    EXPECT_DOUBLE_EQ(moduleInfo->stats.avgResponseTime, 10.0);
}

TEST_F(ModuleInfoTest, CompleteModuleInfo) {
    // Set all fields
    moduleInfo->name = "CompleteModule";
    moduleInfo->description = "A complete test module";
    moduleInfo->version = "2.0.0";
    moduleInfo->status = "active";
    moduleInfo->type = "plugin";
    moduleInfo->author = "Developer";
    moduleInfo->license = "Apache-2.0";
    moduleInfo->path = "/path/to/module.so";
    moduleInfo->configPath = "/config/";
    moduleInfo->configFile = "module.json";
    moduleInfo->enabled = true;
    moduleInfo->hash = 987654321;
    moduleInfo->currentStatus = ModuleInfo::Status::LOADED;
    moduleInfo->priority = 5;

    moduleInfo->dependencies = {"core", "utils"};

    auto func = std::make_unique<FunctionInfo>();
    func->name = "init";
    moduleInfo->functions.push_back(std::move(func));

    moduleInfo->stats.functionCalls = 100;
    moduleInfo->stats.loadCount = 10;

    // Verify all fields
    EXPECT_EQ(moduleInfo->name, "CompleteModule");
    EXPECT_EQ(moduleInfo->description, "A complete test module");
    EXPECT_EQ(moduleInfo->version, "2.0.0");
    EXPECT_EQ(moduleInfo->status, "active");
    EXPECT_EQ(moduleInfo->type, "plugin");
    EXPECT_EQ(moduleInfo->author, "Developer");
    EXPECT_EQ(moduleInfo->license, "Apache-2.0");
    EXPECT_EQ(moduleInfo->path, "/path/to/module.so");
    EXPECT_EQ(moduleInfo->configPath, "/config/");
    EXPECT_EQ(moduleInfo->configFile, "module.json");
    EXPECT_TRUE(moduleInfo->enabled.load());
    EXPECT_EQ(moduleInfo->hash, 987654321);
    EXPECT_EQ(moduleInfo->currentStatus, ModuleInfo::Status::LOADED);
    EXPECT_EQ(moduleInfo->priority, 5);
    EXPECT_EQ(moduleInfo->dependencies.size(), 2);
    EXPECT_EQ(moduleInfo->functions.size(), 1);
    EXPECT_EQ(moduleInfo->stats.functionCalls, 100);
    EXPECT_EQ(moduleInfo->stats.loadCount, 10);
}

// ============================================================================
// Edge Cases and Boundary Tests
// ============================================================================

TEST_F(ModuleInfoTest, EmptyStrings) {
    moduleInfo->name = "";
    moduleInfo->description = "";
    moduleInfo->version = "";

    EXPECT_TRUE(moduleInfo->name.empty());
    EXPECT_TRUE(moduleInfo->description.empty());
    EXPECT_TRUE(moduleInfo->version.empty());
}

TEST_F(ModuleInfoTest, LongStrings) {
    std::string longString(10000, 'a');
    moduleInfo->name = longString;
    moduleInfo->description = longString;

    EXPECT_EQ(moduleInfo->name.length(), 10000);
    EXPECT_EQ(moduleInfo->description.length(), 10000);
}

TEST_F(ModuleInfoTest, SpecialCharactersInStrings) {
    moduleInfo->name = "module-with_special.chars@v1";
    moduleInfo->path = "/path/with spaces/and\ttabs";
    moduleInfo->description = "Description with\nnewlines\nand unicode: 中文";

    EXPECT_EQ(moduleInfo->name, "module-with_special.chars@v1");
    EXPECT_FALSE(moduleInfo->path.empty());
    EXPECT_FALSE(moduleInfo->description.empty());
}

TEST_F(ModuleInfoTest, ManyFunctions) {
    for (int i = 0; i < 1000; ++i) {
        auto func = std::make_unique<FunctionInfo>();
        func->name = "function_" + std::to_string(i);
        moduleInfo->functions.push_back(std::move(func));
    }

    EXPECT_EQ(moduleInfo->functions.size(), 1000);
}

TEST_F(ModuleInfoTest, ManyDependencies) {
    for (int i = 0; i < 100; ++i) {
        moduleInfo->dependencies.push_back("dependency_" + std::to_string(i));
    }

    EXPECT_EQ(moduleInfo->dependencies.size(), 100);
}

}  // namespace lithium::test
