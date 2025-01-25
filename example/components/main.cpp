#include <iostream>
#include "components/manager.hpp"

using namespace lithium;

int main() {
    // Create an instance of ComponentManager
    auto componentManager = ComponentManager::createShared();

    // Initialize ComponentManager
    if (!componentManager->initialize()) {
        std::cerr << "Failed to initialize ComponentManager" << std::endl;
        return 1;
    }

    // Load components
    json componentParams1 = {
        {"name", "Component1"},
        {"path", "/path/to/component1.so"},
        {"version", "1.0.0"},
        {"dependencies",
         json::array({{{"name", "Dependency1"}, {"version", "1.0.0"}},
                      {{"name", "Dependency2"}, {"version", "2.0.0"}}})}};

    if (!componentManager->loadComponent(componentParams1)) {
        std::cerr << "Failed to load Component1" << std::endl;
    }

    json componentParams2 = {{"name", "Component2"},
                             {"path", "/path/to/component2.so"},
                             {"version", "1.0.0"}};

    if (!componentManager->loadComponent(componentParams2)) {
        std::cerr << "Failed to load Component2" << std::endl;
    }

    // Get component information
    auto componentInfo = componentManager->getComponentInfo("Component1");
    if (componentInfo) {
        std::cout << "Component1 info: " << componentInfo->dump(4) << std::endl;
    } else {
        std::cerr << "Failed to get Component1 info" << std::endl;
    }

    // Check if component exists
    if (componentManager->hasComponent("Component2")) {
        std::cout << "Component2 is loaded" << std::endl;
    } else {
        std::cerr << "Component2 is not loaded" << std::endl;
    }

    // Print dependency tree
    componentManager->printDependencyTree();

    // Unload component
    if (!componentManager->unloadComponent({{"name", "Component1"}})) {
        std::cerr << "Failed to unload Component1" << std::endl;
    }

    // Scan components directory
    auto newComponents =
        componentManager->scanComponents("/path/to/components");
    std::cout << "New or modified components: " << std::endl;
    for (const auto& comp : newComponents) {
        std::cout << " - " << comp << std::endl;
    }

    // Destroy ComponentManager
    if (!componentManager->destroy()) {
        std::cerr << "Failed to destroy ComponentManager" << std::endl;
        return 1;
    }

    return 0;
}
