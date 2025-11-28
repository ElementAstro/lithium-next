/*
 * manager_impl.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-1-4

Description: Component Manager Implementation (Private)

**************************************************/

#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/memory/memory.hpp"
#include "atom/memory/object.hpp"
#include "atom/type/json_fwd.hpp"

#include "../dependency.hpp"
#include "../loader.hpp"
#include "../tracker.hpp"
#include "types.hpp"

using json = nlohmann::json;

namespace fs = std::filesystem;

namespace lithium {

class ComponentManagerImpl {
public:
    ComponentManagerImpl();
    ~ComponentManagerImpl();

    auto initialize() -> bool;
    auto destroy() -> bool;

    auto loadComponent(const json& params) -> bool;
    auto unloadComponent(const json& params) -> bool;
    auto scanComponents(const std::string& path) -> std::vector<std::string>;

    auto getComponent(const std::string& component_name)
        -> std::optional<std::weak_ptr<Component>>;
    auto getComponentInfo(const std::string& component_name)
        -> std::optional<json>;
    auto getComponentList() -> std::vector<std::string>;
    auto getComponentDoc(const std::string& component_name) -> std::string;
    auto hasComponent(const std::string& component_name) -> bool;

    void updateDependencyGraph(
        const std::string& component_name, const std::string& version,
        const std::vector<std::string>& dependencies,
        const std::vector<std::string>& dependencies_version);
    void printDependencyTree();

    auto initializeComponent(const std::string& name) -> bool;
    auto startComponent(const std::string& name) -> bool;
    void updateConfig(const std::string& name, const json& config);
    auto batchLoad(const std::vector<std::string>& components) -> bool;
    auto getPerformanceMetrics() -> json;

    void handleError(const std::string& name, const std::string& operation,
                     const std::exception& e);
    void notifyListeners(const std::string& component, ComponentEvent event,
                         const json& data = {});
    void handleFileChange(const fs::path& path, const std::string& change);

    // Member variables
    std::shared_ptr<ModuleLoader> moduleLoader_;
    std::unique_ptr<FileTracker> fileTracker_;
    DependencyGraph dependencyGraph_;
    std::unordered_map<std::string, std::shared_ptr<Component>> components_;
    std::unordered_map<std::string, ComponentOptions> componentOptions_;
    std::unordered_map<std::string, ComponentState> componentStates_;
    std::mutex mutex_;       // Protects access to components_
    std::string lastError_;  // 最后错误信息
    bool performanceMonitoringEnabled_ = true;
    std::shared_ptr<atom::memory::ObjectPool<std::shared_ptr<Component>>>
        component_pool_;
    std::unique_ptr<MemoryPool<char, 4096>> memory_pool_;

    // Event listeners
    std::unordered_map<ComponentEvent,
                       std::vector<std::function<void(
                           const std::string&, ComponentEvent, const json&)>>>
        eventListeners_;

    // Helper methods
    void updateComponentState(const std::string& name, ComponentState newState);
    bool validateComponentOperation(const std::string& name);
    bool loadComponentByName(const std::string& name);
    std::vector<std::string> discoverComponents(const std::string& directory);
    std::string componentsDirectory_;
};

}  // namespace lithium
