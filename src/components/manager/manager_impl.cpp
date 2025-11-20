/*
 * manager_impl.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-1-4

Description: Component Manager Implementation

**************************************************/

#include "manager_impl.hpp"
#include "exceptions.hpp"

#include <algorithm>
#include <format>
#include <future>
#include <numeric>

#include "atom/log/loguru.hpp"
#include "../version.hpp"

namespace lithium {

ComponentManagerImpl::ComponentManagerImpl()
    : moduleLoader_(ModuleLoader::createShared()),
      fileTracker_(std::make_unique<FileTracker>(
          "/components", "package.json",
          std::vector<std::string>{".so", ".dll"})),
      component_pool_(std::make_shared<
                      atom::memory::ObjectPool<std::shared_ptr<Component>>>(
          100, 10)),
      memory_pool_(std::make_unique<MemoryPool<char, 4096>>()) {
    LOG_F(INFO, "ComponentManager initialized with memory pools");
}

ComponentManagerImpl::~ComponentManagerImpl() {
    destroy();
    LOG_F(INFO, "ComponentManager destroyed");
}

auto ComponentManagerImpl::initialize() -> bool {
    try {
        fileTracker_->scan();
        fileTracker_->startWatching();
        fileTracker_->setChangeCallback(
            std::function<void(const fs::path&, const std::string&)>(
                [this](const fs::path& path, const std::string& change) {
                    handleFileChange(path, change);
                }));
        LOG_F(INFO, "ComponentManager initialized successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to initialize ComponentManager: {}", e.what());
        return false;
    }
}

auto ComponentManagerImpl::destroy() -> bool {
    try {
        fileTracker_->stopWatching();
        auto unloadResult = moduleLoader_->unloadAllModules();
        if (!unloadResult) {
            LOG_F(ERROR, "Failed to unload all modules");
            return false;
        }
        components_.clear();
        componentOptions_.clear();
        LOG_F(INFO, "ComponentManager destroyed successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to destroy ComponentManager: {}", e.what());
        return false;
    }
}

auto ComponentManagerImpl::loadComponent(const json& params) -> bool {
    try {
        std::string name = params.at("name").get<std::string>();

        // 使用对象池分配Component实例
        auto instance = component_pool_->acquire();
        if (!instance) {
            LOG_F(ERROR, "Failed to acquire component instance from pool");
            return false;
        }

        // 使用内存池分配其他小型数据结构
        ComponentOptions* options = reinterpret_cast<ComponentOptions*>(
            memory_pool_->allocate(sizeof(ComponentOptions)));
        new (options) ComponentOptions();
        std::string path = params.at("path").get<std::string>();
        std::string version = params.value("version", "1.0.0");

        Version ver = Version::parse(version);

        // Check if component already loaded
        if (components_.contains(name)) {
            LOG_F(WARNING, "Component {} already loaded", name);
            return false;
        }

        // Add component to dependency graph
        dependencyGraph_.addNode(name, ver);        // Add dependencies if specified
        if (params.contains("dependencies")) {
            auto deps = params["dependencies"].get<std::vector<std::string>>();
            for (const auto& dep : deps) {
                dependencyGraph_.addDependency(name, dep, ver);
            }
        }

        // Load module
        if (!moduleLoader_->loadModule(path, name)) {
            THROW_FAIL_TO_LOAD_COMPONENT("Failed to load module for component: {}", name);
        }

        std::lock_guard lock(mutex_);
        components_[name] = *instance;
        componentOptions_[name] = *options;
        updateComponentState(name, ComponentState::Created);
        
        notifyListeners(name, ComponentEvent::PostLoad);
        LOG_F(INFO, "Component {} loaded successfully", name);
        return true;

    } catch (const json::exception& e) {
        LOG_F(ERROR, "JSON error while loading component: {}", e.what());
        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to load component: {}", e.what());
        return false;
    }
}

auto ComponentManagerImpl::unloadComponent(const json& params) -> bool {
    try {
        std::string name = params.at("name").get<std::string>();
        
        std::lock_guard lock(mutex_);
        auto it = components_.find(name);
        if (it == components_.end()) {
            LOG_F(WARNING, "Component {} not found for unloading", name);
            return false;
        }

        notifyListeners(name, ComponentEvent::PreUnload);
          // Unload from module loader
        if (!moduleLoader_->unloadModule(name)) {
            LOG_F(WARNING, "Failed to unload module for component: {}", name);
        }
        
        // Remove from containers
        components_.erase(it);
        componentOptions_.erase(name);
        componentStates_.erase(name);
        
        // Remove from dependency graph
        dependencyGraph_.removeNode(name);
        
        notifyListeners(name, ComponentEvent::PostUnload);
        LOG_F(INFO, "Component {} unloaded successfully", name);
        return true;
        
    } catch (const json::exception& e) {
        LOG_F(ERROR, "JSON error while unloading component: {}", e.what());
        return false;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to unload component: {}", e.what());
        return false;
    }
}

auto ComponentManagerImpl::scanComponents(const std::string& path) -> std::vector<std::string> {
    try {
        fileTracker_->scan();
        fileTracker_->compare();
        auto differences = fileTracker_->getDifferences();
        
        std::vector<std::string> newFiles;
        for (auto& [path, info] : differences.items()) {
            if (info["status"] == "new") {
                newFiles.push_back(path);
            }
        }
        return newFiles;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to scan components: {}", e.what());
        return {};
    }
}

auto ComponentManagerImpl::getComponent(const std::string& component_name)
    -> std::optional<std::weak_ptr<Component>> {
    std::lock_guard lock(mutex_);
    auto it = components_.find(component_name);
    if (it != components_.end()) {
        return std::weak_ptr<Component>(it->second);
    }
    return std::nullopt;
}

auto ComponentManagerImpl::getComponentInfo(const std::string& component_name)
    -> std::optional<json> {
    try {
        std::lock_guard lock(mutex_);
        if (!components_.contains(component_name)) {
            return std::nullopt;
        }
        
        json info;
        info["name"] = component_name;
        info["state"] = static_cast<int>(componentStates_[component_name]);
        if (componentOptions_.contains(component_name)) {
            info["options"] = componentOptions_[component_name].config;
        }
        return info;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to get component info: {}", e.what());
        return std::nullopt;
    }
}

auto ComponentManagerImpl::getComponentList() -> std::vector<std::string> {
    try {
        std::lock_guard lock(mutex_);
        std::vector<std::string> result;
        result.reserve(components_.size());
        for (const auto& [name, _] : components_) {
            result.push_back(name);
        }
        return result;
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to get component list: {}", e.what());
        return {};
    }
}

auto ComponentManagerImpl::getComponentDoc(const std::string& component_name) -> std::string {
    try {
        std::lock_guard lock(mutex_);
        auto it = components_.find(component_name);
        if (it != components_.end()) {
            // Return documentation if available
            return "Component documentation for " + component_name;
        }
        return "";
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to get component documentation: {}", e.what());
        return "";
    }
}

auto ComponentManagerImpl::hasComponent(const std::string& component_name) -> bool {
    std::lock_guard lock(mutex_);
    return components_.contains(component_name);
}

void ComponentManagerImpl::updateDependencyGraph(
    const std::string& component_name, const std::string& version,
    const std::vector<std::string>& dependencies,
    const std::vector<std::string>& dependencies_version) {
    try {
        Version ver = Version::parse(version);
        dependencyGraph_.addNode(component_name, ver);
        
        for (size_t i = 0; i < dependencies.size(); ++i) {
            Version depVer = i < dependencies_version.size() 
                           ? Version::parse(dependencies_version[i]) 
                           : Version{1, 0, 0};
            dependencyGraph_.addDependency(component_name, dependencies[i], depVer);
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to update dependency graph: {}", e.what());
    }
}

void ComponentManagerImpl::printDependencyTree() {
    try {
        // Print dependency information using available methods
        auto components = getComponentList();
        LOG_F(INFO, "Dependency Tree:");
        for (const auto& component : components) {
            auto dependencies = dependencyGraph_.getDependencies(component);
            LOG_F(INFO, "  {} -> [{}]", component, 
                  std::accumulate(dependencies.begin(), dependencies.end(), std::string{},
                      [](const std::string& a, const std::string& b) {
                          return a.empty() ? b : a + ", " + b;
                      }));
        }
    } catch (const std::exception& e) {
        LOG_F(ERROR, "Failed to print dependency tree: {}", e.what());
    }
}

auto ComponentManagerImpl::initializeComponent(const std::string& name) -> bool {
    try {
        if (!validateComponentOperation(name)) {
            return false;
        }
        
        auto comp = getComponent(name);
        if (comp) {
            auto component = comp->lock();
            if (component) {
                updateComponentState(name, ComponentState::Initialized);
                return true;
            }
        }
        return false;
    } catch (const std::exception& e) {
        handleError(name, "initialize", e);
        return false;
    }
}

auto ComponentManagerImpl::startComponent(const std::string& name) -> bool {
    if (!validateComponentOperation(name)) {
        return false;
    }

    try {
        auto comp = getComponent(name);
        if (comp) {
            auto component = comp->lock();
            if (component) {
                updateComponentState(name, ComponentState::Running);
                notifyListeners(name, ComponentEvent::StateChanged);
                return true;
            }
        }
        return false;
    } catch (const std::exception& e) {
        handleError(name, "start", e);
        return false;
    }
}

void ComponentManagerImpl::updateConfig(const std::string& name, const json& config) {
    if (!validateComponentOperation(name)) {
        return;
    }

    try {
        std::lock_guard lock(mutex_);
        if (componentOptions_.contains(name)) {
            componentOptions_[name].config = config;
            notifyListeners(name, ComponentEvent::ConfigChanged, config);
        }
    } catch (const std::exception& e) {
        handleError(name, "updateConfig", e);
    }
}

auto ComponentManagerImpl::batchLoad(const std::vector<std::string>& components) -> bool {
    bool success = true;
    std::vector<std::future<bool>> futures;

    // 按优先级排序
    auto sortedComponents = components;
    std::sort(sortedComponents.begin(), sortedComponents.end(),
              [this](const std::string& a, const std::string& b) {
                  int priorityA = componentOptions_.contains(a) ? componentOptions_[a].priority : 0;
                  int priorityB = componentOptions_.contains(b) ? componentOptions_[b].priority : 0;
                  return priorityA > priorityB;
              });

    // 并行加载
    for (const auto& name : sortedComponents) {
        futures.push_back(std::async(std::launch::async, [this, name]() {
            return loadComponentByName(name);
        }));
    }

    // 等待所有加载完成
    for (auto& future : futures) {
        success &= future.get();
    }

    return success;
}

auto ComponentManagerImpl::getPerformanceMetrics() -> json {
    if (!performanceMonitoringEnabled_) {
        return json{};
    }

    json metrics;
    for (const auto& [name, component] : components_) {
        json componentMetrics;
        componentMetrics["name"] = name;
        componentMetrics["state"] = static_cast<int>(componentStates_[name]);
        metrics[name] = componentMetrics;
    }
    return metrics;
}

void ComponentManagerImpl::handleError(const std::string& name, const std::string& operation,
                 const std::exception& e) {
    lastError_ = std::format("{}: {}", operation, e.what());
    updateComponentState(name, ComponentState::Error);
    notifyListeners(name, ComponentEvent::Error,
                    {{"operation", operation}, {"error", e.what()}});
    LOG_F(ERROR, "{} for {}: {}", operation, name, e.what());
}

void ComponentManagerImpl::notifyListeners(const std::string& component, ComponentEvent event,
                     const json& data) {
    auto it = eventListeners_.find(event);
    if (it != eventListeners_.end()) {
        for (const auto& callback : it->second) {
            try {
                callback(component, event, data);
            } catch (const std::exception& e) {
                LOG_F(ERROR, "Event listener error: {}", e.what());
            }
        }
    }
}

void ComponentManagerImpl::handleFileChange(const fs::path& path, const std::string& change) {
    LOG_F(INFO, "Component file {} was {}", path.string(), change);

    if (change == "modified") {
        // Handle file modification
        std::string name = path.stem().string();
        if (hasComponent(name)) {
            LOG_F(INFO, "Reloading component {} due to file change", name);
            json params;
            params["name"] = name;
            unloadComponent(params);
            loadComponentByName(name);
        }
    } else if (change == "added") {
        // Handle new file
        std::string name = path.stem().string();
        loadComponentByName(name);
    }
}

void ComponentManagerImpl::updateComponentState(const std::string& name,
                          ComponentState newState) {
    std::lock_guard lock(mutex_);
    componentStates_[name] = newState;
}

bool ComponentManagerImpl::validateComponentOperation(const std::string& name) {
    std::lock_guard lock(mutex_);
    if (!components_.contains(name)) {
        LOG_F(ERROR, "Component {} not found", name);
        return false;
    }
    // 可添加更多验证逻辑
    return true;
}

bool ComponentManagerImpl::loadComponentByName(const std::string& name) {
    json params;
    params["name"] = name;
    params["path"] = "/path/to/" + name;
    return loadComponent(params);
}

}  // namespace lithium
