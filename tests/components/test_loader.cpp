#include <gtest/gtest.h>
#include <memory>

#include "components/loader.hpp"

namespace lithium::test {

class ModuleLoaderTest : public ::testing::Test {
protected:
    void SetUp() override {
        loader = std::make_unique<ModuleLoader>("test_modules");
    }

    void TearDown() override { loader.reset(); }

    std::unique_ptr<ModuleLoader> loader;
};

TEST_F(ModuleLoaderTest, CreateSharedDefault) {
    auto sharedLoader = ModuleLoader::createShared();
    EXPECT_NE(sharedLoader, nullptr);
}

TEST_F(ModuleLoaderTest, CreateSharedWithDir) {
    auto sharedLoader = ModuleLoader::createShared("custom_modules");
    EXPECT_NE(sharedLoader, nullptr);
}

TEST_F(ModuleLoaderTest, LoadModule) {
    EXPECT_TRUE(loader->loadModule("path/to/module.so", "testModule"));
    EXPECT_TRUE(loader->hasModule("testModule"));
}

TEST_F(ModuleLoaderTest, UnloadModule) {
    loader->loadModule("path/to/module.so", "testModule");
    EXPECT_TRUE(loader->unloadModule("testModule"));
    EXPECT_FALSE(loader->hasModule("testModule"));
}

TEST_F(ModuleLoaderTest, UnloadAllModules) {
    loader->loadModule("path/to/module1.so", "testModule1");
    loader->loadModule("path/to/module2.so", "testModule2");
    EXPECT_TRUE(loader->unloadAllModules());
    EXPECT_FALSE(loader->hasModule("testModule1"));
    EXPECT_FALSE(loader->hasModule("testModule2"));
}

TEST_F(ModuleLoaderTest, HasModule) {
    loader->loadModule("path/to/module.so", "testModule");
    EXPECT_TRUE(loader->hasModule("testModule"));
    EXPECT_FALSE(loader->hasModule("nonExistentModule"));
}

TEST_F(ModuleLoaderTest, GetModule) {
    loader->loadModule("path/to/module.so", "testModule");
    auto module = loader->getModule("testModule");
    EXPECT_NE(module, nullptr);
    EXPECT_EQ(loader->getModule("nonExistentModule"), nullptr);
}

TEST_F(ModuleLoaderTest, EnableModule) {
    loader->loadModule("path/to/module.so", "testModule");
    EXPECT_TRUE(loader->enableModule("testModule"));
    EXPECT_TRUE(loader->isModuleEnabled("testModule"));
}

TEST_F(ModuleLoaderTest, DisableModule) {
    loader->loadModule("path/to/module.so", "testModule");
    loader->enableModule("testModule");
    EXPECT_TRUE(loader->disableModule("testModule"));
    EXPECT_FALSE(loader->isModuleEnabled("testModule"));
}

TEST_F(ModuleLoaderTest, IsModuleEnabled) {
    loader->loadModule("path/to/module.so", "testModule");
    loader->enableModule("testModule");
    EXPECT_TRUE(loader->isModuleEnabled("testModule"));
    loader->disableModule("testModule");
    EXPECT_FALSE(loader->isModuleEnabled("testModule"));
}

TEST_F(ModuleLoaderTest, GetAllExistedModules) {
    loader->loadModule("path/to/module1.so", "testModule1");
    loader->loadModule("path/to/module2.so", "testModule2");
    auto modules = loader->getAllExistedModules();
    EXPECT_EQ(modules.size(), 2);
    EXPECT_NE(std::find(modules.begin(), modules.end(), "testModule1"),
              modules.end());
    EXPECT_NE(std::find(modules.begin(), modules.end(), "testModule2"),
              modules.end());
}

TEST_F(ModuleLoaderTest, HasFunction) {
    loader->loadModule("path/to/module.so", "testModule");
    EXPECT_TRUE(loader->hasFunction("testModule", "testFunction"));
    EXPECT_FALSE(loader->hasFunction("testModule", "nonExistentFunction"));
}

TEST_F(ModuleLoaderTest, ReloadModule) {
    loader->loadModule("path/to/module.so", "testModule");
    EXPECT_TRUE(loader->reloadModule("testModule"));
    EXPECT_TRUE(loader->hasModule("testModule"));
}

TEST_F(ModuleLoaderTest, GetModuleStatus) {
    loader->loadModule("path/to/module.so", "testModule");
    EXPECT_EQ(loader->getModuleStatus("testModule"),
              ModuleInfo::Status::LOADED);
    loader->unloadModule("testModule");
    EXPECT_EQ(loader->getModuleStatus("testModule"),
              ModuleInfo::Status::UNLOADED);
}

TEST_F(ModuleLoaderTest, ValidateDependencies) {
    loader->loadModule("path/to/module.so", "testModule");
    auto module = loader->getModule("testModule");
    module->dependencies.push_back("dependencyModule");
    EXPECT_FALSE(loader->validateDependencies("testModule"));
    loader->loadModule("path/to/dependency.so", "dependencyModule");
    loader->enableModule("dependencyModule");
    EXPECT_TRUE(loader->validateDependencies("testModule"));
}

TEST_F(ModuleLoaderTest, LoadModulesInOrder) {
    loader->loadModule("path/to/module1.so", "testModule1");
    loader->loadModule("path/to/module2.so", "testModule2");
    auto module1 = loader->getModule("testModule1");
    auto module2 = loader->getModule("testModule2");
    module1->dependencies.push_back("testModule2");
    EXPECT_TRUE(loader->loadModulesInOrder());
    EXPECT_TRUE(loader->isModuleEnabled("testModule1"));
    EXPECT_TRUE(loader->isModuleEnabled("testModule2"));
}

}  // namespace lithium::test