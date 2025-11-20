#include "system/dependency_system.hpp"
#include <iostream>

int main() {
    try {
        // 使用新的命名空间
        lithium::system::DependencyManager manager;
        
        std::cout << "Current platform: " << manager.getCurrentPlatform() << std::endl;
        
        // 添加一个依赖
        lithium::system::DependencyInfo dep;
        dep.name = "cmake";
        dep.version = {3, 20, 0, ""};
        dep.packageManager = "apt";
        
        manager.addDependency(dep);
        
        std::cout << "Dependency added successfully!" << std::endl;
        std::cout << "Dependency report:" << std::endl;
        std::cout << manager.generateDependencyReport() << std::endl;
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
