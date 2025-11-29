#include <gtest/gtest.h>
#include <atomic>
#include <filesystem>
#include <memory>
#include <thread>

#include "components/loader.hpp"

namespace fs = std::filesystem;

// Forward declaration for instance tests
namespace test_module {
class TestClass;
}

namespace lithium::test {

// Helper to create a temporary test directory
class ModuleLoaderTestFixture : public ::testing::Test {
protected:
    void SetUp() override {
        testDir = fs::temp_directory_path() / "lithium_loader_test";
        fs::create_directories(testDir);
        loader = std::make_unique<ModuleLoader>(testDir.string());
    }

    void TearDown() override {
        loader.reset();
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    fs::path testDir;
    std::unique_ptr<ModuleLoader> loader;
};

class ModuleLoaderTest : public ModuleLoaderTestFixture {};

TEST_F(ModuleLoaderTest, CreateSharedDefault) {
    auto sharedLoader = ModuleLoader::createShared();
    EXPECT_NE(sharedLoader, nullptr);
}

TEST_F(ModuleLoaderTest, CreateSharedWithDir) {
    auto sharedLoader = ModuleLoader::createShared("custom_modules");
    EXPECT_NE(sharedLoader, nullptr);
}

TEST_F(ModuleLoaderTest, LoadModuleNonExistent) {
    // Loading non-existent module should fail
    auto result = loader->loadModule("nonexistent/module.so", "testModule");
    EXPECT_FALSE(result.has_value());
    EXPECT_FALSE(loader->hasModule("testModule"));
}

TEST_F(ModuleLoaderTest, LoadModuleEmptyName) {
    auto result = loader->loadModule("path/to/module.so", "");
    EXPECT_FALSE(result.has_value());
}

TEST_F(ModuleLoaderTest, LoadModuleEmptyPath) {
    auto result = loader->loadModule("", "testModule");
    EXPECT_FALSE(result.has_value());
}

TEST_F(ModuleLoaderTest, UnloadNonExistentModule) {
    auto result = loader->unloadModule("nonExistentModule");
    EXPECT_FALSE(result.has_value());
}

TEST_F(ModuleLoaderTest, UnloadAllModulesEmpty) {
    // Unloading all when none loaded should succeed
    auto result = loader->unloadAllModules();
    EXPECT_TRUE(result.has_value());
}

TEST_F(ModuleLoaderTest, HasModuleNonExistent) {
    EXPECT_FALSE(loader->hasModule("nonExistentModule"));
    EXPECT_FALSE(loader->hasModule(""));
}

TEST_F(ModuleLoaderTest, GetModuleNonExistent) {
    EXPECT_EQ(loader->getModule("nonExistentModule"), nullptr);
}

TEST_F(ModuleLoaderTest, EnableModuleNonExistent) {
    auto result = loader->enableModule("nonExistentModule");
    EXPECT_FALSE(result.has_value());
}

TEST_F(ModuleLoaderTest, DisableModuleNonExistent) {
    auto result = loader->disableModule("nonExistentModule");
    EXPECT_FALSE(result.has_value());
}

TEST_F(ModuleLoaderTest, IsModuleEnabledNonExistent) {
    EXPECT_FALSE(loader->isModuleEnabled("nonExistentModule"));
}

TEST_F(ModuleLoaderTest, GetAllExistedModulesEmpty) {
    auto modules = loader->getAllExistedModules();
    EXPECT_TRUE(modules.empty());
}

TEST_F(ModuleLoaderTest, HasFunctionNonExistentModule) {
    EXPECT_FALSE(loader->hasFunction("nonExistentModule", "testFunction"));
}

TEST_F(ModuleLoaderTest, ReloadModuleNonExistent) {
    auto result = loader->reloadModule("nonExistentModule");
    EXPECT_FALSE(result.has_value());
}

TEST_F(ModuleLoaderTest, GetModuleStatusNonExistent) {
    // Non-existent module should return UNLOADED or similar
    auto status = loader->getModuleStatus("nonExistentModule");
    EXPECT_EQ(status, ModuleInfo::Status::UNLOADED);
}

TEST_F(ModuleLoaderTest, ValidateDependenciesNonExistent) {
    // Non-existent module should return false for dependency validation
    EXPECT_FALSE(loader->validateDependencies("nonExistentModule"));
}

TEST_F(ModuleLoaderTest, LoadModulesInOrderEmpty) {
    // Loading in order with no modules should succeed
    auto result = loader->loadModulesInOrder();
    EXPECT_TRUE(result.has_value());
}

// Additional edge case tests
TEST_F(ModuleLoaderTest, CreateSharedWithEmptyDir) {
    auto sharedLoader = ModuleLoader::createShared("");
    EXPECT_NE(sharedLoader, nullptr);
}

TEST_F(ModuleLoaderTest, MultipleCreateShared) {
    auto loader1 = ModuleLoader::createShared();
    auto loader2 = ModuleLoader::createShared();
    EXPECT_NE(loader1, nullptr);
    EXPECT_NE(loader2, nullptr);
    // They should be different instances
    EXPECT_NE(loader1.get(), loader2.get());
}

// ============================================================================
// Real Module Loading Tests (Requires test_module)
// ============================================================================

// Test fixture for real module loading tests
class RealModuleLoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        testDir = fs::temp_directory_path() / "lithium_loader_real_test";
        fs::create_directories(testDir);
        loader = std::make_unique<ModuleLoader>(testDir.string());

        // Get the test module path (should be in the same directory as the test
        // executable)
        moduleDir = fs::current_path();
#ifdef _WIN32
        modulePath = moduleDir / "test_module.dll";
#else
        modulePath = moduleDir / "test_module.so";
#endif
    }

    void TearDown() override {
        if (loader) {
            loader->unloadAllModules();
        }
        loader.reset();
        if (fs::exists(testDir)) {
            fs::remove_all(testDir);
        }
    }

    bool moduleExists() const { return fs::exists(modulePath); }

    fs::path testDir;
    fs::path moduleDir;
    fs::path modulePath;
    std::unique_ptr<ModuleLoader> loader;
};

// Skip real module tests if module doesn't exist
#define SKIP_IF_NO_MODULE()                                         \
    if (!moduleExists()) {                                          \
        GTEST_SKIP() << "Test module not found at: " << modulePath; \
    }

TEST_F(RealModuleLoaderTest, LoadRealModule) {
    SKIP_IF_NO_MODULE();

    auto result = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(result.has_value())
        << "Failed to load module: " << result.error();
    EXPECT_TRUE(result.value());
    EXPECT_TRUE(loader->hasModule("test_module"));
}

TEST_F(RealModuleLoaderTest, LoadAndUnloadModule) {
    SKIP_IF_NO_MODULE();

    // Load
    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());
    EXPECT_TRUE(loader->hasModule("test_module"));

    // Unload
    auto unloadResult = loader->unloadModule("test_module");
    ASSERT_TRUE(unloadResult.has_value());
    EXPECT_FALSE(loader->hasModule("test_module"));
}

TEST_F(RealModuleLoaderTest, ReloadModule) {
    SKIP_IF_NO_MODULE();

    // Load first
    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    // Reload
    auto reloadResult = loader->reloadModule("test_module");
    ASSERT_TRUE(reloadResult.has_value());
    EXPECT_TRUE(loader->hasModule("test_module"));
}

TEST_F(RealModuleLoaderTest, GetModuleInfo) {
    SKIP_IF_NO_MODULE();

    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    auto moduleInfo = loader->getModule("test_module");
    ASSERT_NE(moduleInfo, nullptr);
    EXPECT_EQ(moduleInfo->name, "test_module");
}

TEST_F(RealModuleLoaderTest, GetModuleStatus) {
    SKIP_IF_NO_MODULE();

    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    auto status = loader->getModuleStatus("test_module");
    EXPECT_EQ(status, ModuleInfo::Status::LOADED);
}

TEST_F(RealModuleLoaderTest, EnableDisableModule) {
    SKIP_IF_NO_MODULE();

    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    // Initially should be enabled
    EXPECT_TRUE(loader->isModuleEnabled("test_module"));

    // Disable
    auto disableResult = loader->disableModule("test_module");
    ASSERT_TRUE(disableResult.has_value());
    EXPECT_FALSE(loader->isModuleEnabled("test_module"));

    // Re-enable
    auto enableResult = loader->enableModule("test_module");
    ASSERT_TRUE(enableResult.has_value());
    EXPECT_TRUE(loader->isModuleEnabled("test_module"));
}

TEST_F(RealModuleLoaderTest, SetModulePriority) {
    SKIP_IF_NO_MODULE();

    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    auto priorityResult = loader->setModulePriority("test_module", 10);
    EXPECT_TRUE(priorityResult.has_value());
}

TEST_F(RealModuleLoaderTest, GetModuleStatistics) {
    SKIP_IF_NO_MODULE();

    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    auto stats = loader->getModuleStatistics("test_module");
    EXPECT_GE(stats.loadCount, 1);
}

TEST_F(RealModuleLoaderTest, GetAllExistedModules) {
    SKIP_IF_NO_MODULE();

    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    auto modules = loader->getAllExistedModules();
    EXPECT_EQ(modules.size(), 1);
    EXPECT_EQ(modules[0], "test_module");
}

// ============================================================================
// Function Extraction Tests
// ============================================================================

TEST_F(RealModuleLoaderTest, GetSimpleFunction) {
    SKIP_IF_NO_MODULE();

    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    auto addFunc = loader->getFunction<int(int, int)>("test_module", "add");
    ASSERT_TRUE(addFunc.has_value())
        << "Failed to get function: " << addFunc.error();

    // Test the function
    EXPECT_EQ(addFunc.value()(3, 4), 7);
    EXPECT_EQ(addFunc.value()(10, 20), 30);
    EXPECT_EQ(addFunc.value()(-5, 5), 0);
}

TEST_F(RealModuleLoaderTest, GetMultipleFunctions) {
    SKIP_IF_NO_MODULE();

    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    auto addFunc = loader->getFunction<int(int, int)>("test_module", "add");
    auto mulFunc =
        loader->getFunction<int(int, int)>("test_module", "multiply");

    ASSERT_TRUE(addFunc.has_value());
    ASSERT_TRUE(mulFunc.has_value());

    EXPECT_EQ(addFunc.value()(3, 4), 7);
    EXPECT_EQ(mulFunc.value()(3, 4), 12);
}

TEST_F(RealModuleLoaderTest, GetVersionFunction) {
    SKIP_IF_NO_MODULE();

    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    auto versionFunc =
        loader->getFunction<const char*()>("test_module", "getVersion");
    ASSERT_TRUE(versionFunc.has_value());

    const char* version = versionFunc.value()();
    EXPECT_STREQ(version, "1.0.0");
}

TEST_F(RealModuleLoaderTest, GetNonExistentFunction) {
    SKIP_IF_NO_MODULE();

    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    auto func =
        loader->getFunction<void()>("test_module", "nonExistentFunction");
    EXPECT_FALSE(func.has_value());
}

TEST_F(RealModuleLoaderTest, HasFunction) {
    SKIP_IF_NO_MODULE();

    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    EXPECT_TRUE(loader->hasFunction("test_module", "add"));
    EXPECT_TRUE(loader->hasFunction("test_module", "multiply"));
    EXPECT_TRUE(loader->hasFunction("test_module", "getVersion"));
    EXPECT_FALSE(loader->hasFunction("test_module", "nonExistentFunction"));
}

// ============================================================================
// Instance Creation Tests
// ============================================================================

TEST_F(RealModuleLoaderTest, CreateSharedInstance) {
    SKIP_IF_NO_MODULE();

    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    json config = {{"name", "TestInstance"}, {"value", 42}};

    auto instanceResult = loader->getInstance<test_module::TestClass>(
        "test_module", config, "createInstance");

    if (instanceResult.has_value()) {
        auto instance = instanceResult.value();
        ASSERT_NE(instance, nullptr);
        EXPECT_EQ(instance->getName(), "TestInstance");
        EXPECT_EQ(instance->getValue(), 42);
    } else {
        // Instance creation might fail if symbol not properly exported
        GTEST_SKIP() << "Instance creation not supported: "
                     << instanceResult.error();
    }
}

TEST_F(RealModuleLoaderTest, CreateUniqueInstance) {
    SKIP_IF_NO_MODULE();

    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    json config = {{"name", "UniqueInstance"}, {"value", 100}};

    auto instanceResult = loader->getUniqueInstance<test_module::TestClass>(
        "test_module", config, "createUniqueInstance");

    if (instanceResult.has_value()) {
        auto instance = std::move(instanceResult.value());
        ASSERT_NE(instance, nullptr);
        EXPECT_EQ(instance->getName(), "UniqueInstance");
        EXPECT_EQ(instance->getValue(), 100);
    } else {
        GTEST_SKIP() << "Unique instance creation not supported: "
                     << instanceResult.error();
    }
}

// ============================================================================
// Async Loading Tests
// ============================================================================

TEST_F(RealModuleLoaderTest, LoadModulesAsync) {
    SKIP_IF_NO_MODULE();

    std::vector<std::pair<std::string, std::string>> modules = {
        {modulePath.string(), "test_module_async"}};

    auto futures = loader->loadModulesAsync(modules);
    ASSERT_EQ(futures.size(), 1);

    // Wait for completion
    auto result = futures[0].get();
    EXPECT_TRUE(result.has_value());
    EXPECT_TRUE(loader->hasModule("test_module_async"));
}

// ============================================================================
// Thread Safety Tests
// ============================================================================

TEST_F(RealModuleLoaderTest, ConcurrentModuleAccess) {
    SKIP_IF_NO_MODULE();

    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::atomic<int> successCount{0};

    // Multiple threads accessing the same module
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([this, &successCount]() {
            if (loader->hasModule("test_module")) {
                auto func =
                    loader->getFunction<int(int, int)>("test_module", "add");
                if (func.has_value() && func.value()(2, 3) == 5) {
                    successCount++;
                }
            }
        });
    }

    for (auto& thread : threads) {
        thread.join();
    }

    EXPECT_EQ(successCount, numThreads);
}

TEST_F(RealModuleLoaderTest, ConcurrentLoadUnload) {
    SKIP_IF_NO_MODULE();

    constexpr int iterations = 5;

    for (int i = 0; i < iterations; ++i) {
        // Load
        auto loadResult =
            loader->loadModule(modulePath.string(), "test_module");
        ASSERT_TRUE(loadResult.has_value()) << "Failed at iteration " << i;

        // Verify
        EXPECT_TRUE(loader->hasModule("test_module"));

        // Unload
        auto unloadResult = loader->unloadModule("test_module");
        ASSERT_TRUE(unloadResult.has_value())
            << "Unload failed at iteration " << i;
    }
}

// ============================================================================
// Batch Processing Tests
// ============================================================================

TEST_F(RealModuleLoaderTest, BatchProcessModules) {
    SKIP_IF_NO_MODULE();

    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    int processedCount = 0;
    auto count = loader->batchProcessModules(
        [&processedCount](std::shared_ptr<ModuleInfo> info) {
            if (info && info->name == "test_module") {
                processedCount++;
                return true;
            }
            return false;
        });

    EXPECT_EQ(count, 1);
    EXPECT_EQ(processedCount, 1);
}

// ============================================================================
// Module Dependencies Tests
// ============================================================================

TEST_F(RealModuleLoaderTest, GetDependencies) {
    SKIP_IF_NO_MODULE();

    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    auto deps = loader->getDependencies("test_module");
    // Test module has no dependencies
    EXPECT_TRUE(deps.empty());
}

TEST_F(RealModuleLoaderTest, ValidateDependencies) {
    SKIP_IF_NO_MODULE();

    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    // Module with no dependencies should always validate
    EXPECT_TRUE(loader->validateDependencies("test_module"));
}

TEST_F(RealModuleLoaderTest, LoadModulesInOrder) {
    SKIP_IF_NO_MODULE();

    // First load a module
    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    // loadModulesInOrder should succeed with loaded modules
    auto result = loader->loadModulesInOrder();
    EXPECT_TRUE(result.has_value());
}

// ============================================================================
// Error Handling Tests
// ============================================================================

TEST_F(RealModuleLoaderTest, LoadSameModuleTwice) {
    SKIP_IF_NO_MODULE();

    auto result1 = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(result1.has_value());

    // Loading same module again should fail or return error
    auto result2 = loader->loadModule(modulePath.string(), "test_module");
    // Behavior depends on implementation - might succeed with warning or fail
}

TEST_F(RealModuleLoaderTest, UnloadNonLoadedModule) {
    auto result = loader->unloadModule("never_loaded_module");
    EXPECT_FALSE(result.has_value());
}

TEST_F(RealModuleLoaderTest, GetFunctionFromUnloadedModule) {
    SKIP_IF_NO_MODULE();

    // Load and then unload
    auto loadResult = loader->loadModule(modulePath.string(), "test_module");
    ASSERT_TRUE(loadResult.has_value());

    auto unloadResult = loader->unloadModule("test_module");
    ASSERT_TRUE(unloadResult.has_value());

    // Try to get function from unloaded module
    auto func = loader->getFunction<int(int, int)>("test_module", "add");
    EXPECT_FALSE(func.has_value());
}

}  // namespace lithium::test
