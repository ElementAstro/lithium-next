#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include "system.hpp"

int main() {
    try {
        using namespace lithium::system;

        std::cout << "=== System Dependency Manager Test ===" << std::endl;
        std::cout << "Module version: " << getSystemModuleVersion()
                  << std::endl;

        // 使用工厂函数创建实例
        auto manager =
            createDependencyManager("./config/package_managers.json");

        std::cout << "Current platform: " << manager->getCurrentPlatform()
                  << std::endl;
        std::cout << "Normalized platform: " << getCurrentPlatform()
                  << std::endl;
        std::cout << "Default package manager: " << getDefaultPackageManager()
                  << std::endl;

        // 获取可用的包管理器
        auto pkgManagers = manager->getPackageManagers();
        std::cout << "\nAvailable package managers: " << pkgManagers.size()
                  << std::endl;
        for (const auto& pm : pkgManagers) {
            std::cout << "  - " << pm.name << std::endl;
        }

        // 检查一些常见依赖是否已安装
        std::cout << "\n=== Checking Common Dependencies ===" << std::endl;
        std::vector<std::string> commonDeps = {"cmake", "git", "python3"};

        for (const auto& depName : commonDeps) {
            bool installed = manager->isDependencyInstalled(depName);
            std::cout << depName << ": "
                      << (installed ? "INSTALLED" : "NOT INSTALLED");

            if (installed) {
                auto version = manager->getInstalledVersion(depName);
                if (version) {
                    std::cout << " (v" << versionToString(*version) << ")";
                }
            }
            std::cout << std::endl;
        }

        // 添加一个依赖（不指定包管理器，自动使用默认）
        std::cout << "\n=== Adding Managed Dependency ===" << std::endl;
        DependencyInfo dep;
        dep.name = "cmake";
        dep.version = {3, 20, 0, ""};
        // 不需要手动设置 packageManager，会自动使用平台默认

        manager->addDependency(dep);

        DependencyInfo gitDep;
        gitDep.name = "git";
        gitDep.version = {2, 30, 0, ""};
        manager->addDependency(gitDep);
        std::cout << "Dependencies added successfully!" << std::endl;

        // 刷新包管理器列表并展示
        manager->loadSystemPackageManagers();
        auto managers = manager->getPackageManagers();
        std::cout << "Loaded package managers: " << managers.size()
                  << std::endl;
        for (const auto& info : managers) {
            std::cout << "  * " << info.name << std::endl;
        }

        // 生成依赖报告
        std::cout << "\n=== Dependency Report ===" << std::endl;
        std::cout << manager->generateDependencyReport() << std::endl;

        // 获取依赖图
        std::cout << "\n=== Dependency Graph ===" << std::endl;
        std::cout << manager->getDependencyGraph() << std::endl;

        // 导出配置
        auto configResult = manager->exportConfig();
        if (configResult.value) {
            std::cout << "\n=== Exported Config ===" << std::endl;
            std::cout << *configResult.value << std::endl;
            std::cout << "Re-importing configuration..." << std::endl;
            auto importResult = manager->importConfig(*configResult.value);
            if (!importResult.success) {
                std::cout << "Import failed: " << importResult.error->message()
                          << std::endl;
            }
        }

        // 验证版本兼容性
        std::cout << "\n=== Version Compatibility Check ===" << std::endl;
        auto compatResult =
            manager->checkVersionCompatibility("cmake", "3.10.0");
        if (compatResult.value) {
            std::cout << "cmake >= 3.10.0: "
                      << (*compatResult.value ? "COMPATIBLE" : "INCOMPATIBLE")
                      << std::endl;
        }

        // 测试版本解析工具
        std::cout << "\n=== Version Parsing Test ===" << std::endl;
        std::string testVersion = "3.20.1-beta";
        if (isValidVersion(testVersion)) {
            auto parsed = parseVersion(testVersion);
            std::cout << "Parsed '" << testVersion
                      << "': " << versionToString(parsed) << std::endl;
        }

        // 搜索依赖示例
        std::cout << "\n=== Search Dependency ===" << std::endl;
        auto searchResults = manager->searchDependency("cmake");
        if (searchResults.empty()) {
            std::cout << "Search returned no results (possibly offline test)"
                      << std::endl;
        } else {
            std::cout << "Found candidates:" << std::endl;
            for (const auto& candidate : searchResults) {
                std::cout << "  - " << candidate << std::endl;
            }
        }

        std::cout << "\n=== Dependency Cache Refresh ===" << std::endl;
        manager->refreshCache();
        std::cout << "Cache refresh completed." << std::endl;

        std::cout << "\n=== Test Complete ===" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
