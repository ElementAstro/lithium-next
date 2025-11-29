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
#include <cstdlib>
#include <format>
#include <future>
#include <numeric>

#include "../version.hpp"
#include "atom/log/spdlog_logger.hpp"

namespace lithium {

ComponentManagerImpl::ComponentManagerImpl()
    : moduleLoader_(ModuleLoader::createShared()),
      component_pool_(
          std::make_shared<
              atom::memory::ObjectPool<std::shared_ptr<Component>>>(100, 10)),
      memory_pool_(std::make_unique<MemoryPool<char, 4096>>()) {
#ifdef _WIN32
    constexpr const char* kDefaultComponentsDir = "components";
#else
    constexpr const char* kDefaultComponentsDir = "./components";
#endif

    if (const char* customDir = std::getenv("LITHIUM_COMPONENTS_DIR")) {
        componentsDirectory_ = customDir;
        LOG_INFO("Using custom components directory from env: {}",
                 componentsDirectory_);
    } else {
        componentsDirectory_ = kDefaultComponentsDir;
    }

    if (fs::exists(componentsDirectory_)) {
        fileTracker_ = std::make_unique<FileTracker>(
            componentsDirectory_, "package.json",
            std::vector<std::string>{".so", ".dll", ".dylib"});
    } else {
        LOG_WARN("Components directory '{}' does not exist",
                 componentsDirectory_);
    }

    LOG_INFO(
        "ComponentManager initialized with memory pools (components dir: {})",
        componentsDirectory_);
}

ComponentManagerImpl::~ComponentManagerImpl() {
    destroy();
    LOG_INFO("ComponentManager destroyed");
}

auto ComponentManagerImpl::initialize() -> bool {
    try {
        // Only initialize FileTracker if it was successfully created
        if (fileTracker_) {
            fileTracker_->scan();
            fileTracker_->startWatching();
            fileTracker_->setChangeCallback(
                std::function<void(const fs::path&, const std::string&)>(
                    [this](const fs::path& path, const std::string& change) {
                        handleFileChange(path, change);
                    }));
            LOG_INFO("FileTracker initialized and watching for changes");
        } else {
            LOG_WARN(
                "FileTracker not initialized - components directory may not "
                "exist");
        }

        if (!componentsDirectory_.empty()) {
            auto discovered = discoverComponents(componentsDirectory_);
            if (!discovered.empty()) {
                LOG_INFO("Discovered {} components in {}", discovered.size(),
                         componentsDirectory_);
                batchLoad(discovered);
            } else {
                LOG_INFO("No components discovered in {}",
                         componentsDirectory_);
            }
        }

        LOG_INFO("ComponentManager initialized successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to initialize ComponentManager: {}", e.what());
        return false;
    }
}

auto ComponentManagerImpl::destroy() -> bool {
    try {
        // Stop file watching if FileTracker exists
        if (fileTracker_) {
            fileTracker_->stopWatching();
        }

        auto unloadResult = moduleLoader_->unloadAllModules();
        if (!unloadResult) {
            LOG_ERROR("Failed to unload all modules");
            return false;
        }
        components_.clear();
        componentOptions_.clear();
        componentStates_.clear();
        LOG_INFO("ComponentManager destroyed successfully");
        return true;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to destroy ComponentManager: {}", e.what());
        return false;
    }
}

auto ComponentManagerImpl::loadComponent(const json& params) -> bool {
    try {
        std::string name = params.at("name").get<std::string>();

        // 使用对象池分配Component实例
        auto instance = component_pool_->acquire();
        if (!instance) {
            LOG_ERROR("Failed to acquire component instance from pool");
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
            LOG_WARN("Component {} already loaded", name);
            return false;
        }

        // Add component to dependency graph
        dependencyGraph_.addNode(name, ver);  // Add dependencies if specified
        if (params.contains("dependencies")) {
            auto deps = params["dependencies"].get<std::vector<std::string>>();
            for (const auto& dep : deps) {
                dependencyGraph_.addDependency(name, dep, ver);
            }
        }

        // Load module
        if (!moduleLoader_->loadModule(path, name)) {
            THROW_FAIL_TO_LOAD_COMPONENT(
                "Failed to load module for component: {}", name);
        }

        std::lock_guard lock(mutex_);
        components_[name] = *instance;
        componentOptions_[name] = *options;
        updateComponentState(name, ComponentState::Created);

        notifyListeners(name, ComponentEvent::PostLoad);
        LOG_INFO("Component {} loaded successfully", name);
        return true;

    } catch (const json::exception& e) {
        LOG_ERROR("JSON error while loading component: {}", e.what());
        return false;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to load component: {}", e.what());
        return false;
    }
}

auto ComponentManagerImpl::unloadComponent(const json& params) -> bool {
    try {
        std::string name = params.at("name").get<std::string>();

        std::lock_guard lock(mutex_);
        auto it = components_.find(name);
        if (it == components_.end()) {
            LOG_WARN("Component {} not found for unloading", name);
            return false;
        }

        notifyListeners(name, ComponentEvent::PreUnload);
        // Unload from module loader
        if (!moduleLoader_->unloadModule(name)) {
            LOG_WARN("Failed to unload module for component: {}", name);
        }

        // Remove from containers
        components_.erase(it);
        componentOptions_.erase(name);
        componentStates_.erase(name);

        // Remove from dependency graph
        dependencyGraph_.removeNode(name);

        notifyListeners(name, ComponentEvent::PostUnload);
        LOG_INFO("Component {} unloaded successfully", name);
        return true;

    } catch (const json::exception& e) {
        LOG_ERROR("JSON error while unloading component: {}", e.what());
        return false;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to unload component: {}", e.what());
        return false;
    }
}

auto ComponentManagerImpl::scanComponents(const std::string& path)
    -> std::vector<std::string> {
    try {
        const std::string targetPath =
            path.empty() ? componentsDirectory_ : path;
        if (targetPath.empty()) {
            LOG_WARN("No components directory configured; skip scanning");
            return {};
        }

        if (fileTracker_ && targetPath == componentsDirectory_) {
            fileTracker_->scan();
            fileTracker_->compare();
            auto differences = fileTracker_->getDifferences();

            std::vector<std::string> newFiles;
            for (auto& [filePath, info] : differences.items()) {
                if (info["status"] == "new") {
                    newFiles.push_back(filePath);
                }
            }
            LOG_INFO("FileTracker detected {} new components", newFiles.size());
            return newFiles;
        }

        auto discovered = discoverComponents(targetPath);
        LOG_INFO("Discovered {} components via direct scan ({})",
                 discovered.size(), targetPath);
        return discovered;
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to scan components: {}", e.what());
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
        LOG_ERROR("Failed to get component info: {}", e.what());
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
        LOG_ERROR("Failed to get component list: {}", e.what());
        return {};
    }
}

auto ComponentManagerImpl::getComponentDoc(const std::string& component_name)
    -> std::string {
    try {
        std::lock_guard lock(mutex_);
        auto it = components_.find(component_name);
        if (it != components_.end()) {
            // Return documentation if available
            return "Component documentation for " + component_name;
        }
        return "";
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to get component documentation: {}", e.what());
        return "";
    }
}

auto ComponentManagerImpl::hasComponent(const std::string& component_name)
    -> bool {
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
            dependencyGraph_.addDependency(component_name, dependencies[i],
                                           depVer);
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to update dependency graph: {}", e.what());
    }
}

void ComponentManagerImpl::printDependencyTree() {
    try {
        // Print dependency information using available methods
        auto components = getComponentList();
        LOG_INFO("Dependency Tree:");
        for (const auto& component : components) {
            auto dependencies = dependencyGraph_.getDependencies(component);
            LOG_INFO(
                "  {} -> [{}]", component,
                std::accumulate(dependencies.begin(), dependencies.end(),
                                std::string{},
                                [](const std::string& a, const std::string& b) {
                                    return a.empty() ? b : a + ", " + b;
                                }));
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to print dependency tree: {}", e.what());
    }
}

auto ComponentManagerImpl::initializeComponent(const std::string& name)
    -> bool {
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

void ComponentManagerImpl::updateConfig(const std::string& name,
                                        const json& config) {
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

auto ComponentManagerImpl::batchLoad(const std::vector<std::string>& components)
    -> bool {
    bool success = true;
    std::vector<std::future<bool>> futures;

    // 按优先级排序
    auto sortedComponents = components;
    std::sort(sortedComponents.begin(), sortedComponents.end(),
              [this](const std::string& a, const std::string& b) {
                  int priorityA = componentOptions_.contains(a)
                                      ? componentOptions_[a].priority
                                      : 0;
                  int priorityB = componentOptions_.contains(b)
                                      ? componentOptions_[b].priority
                                      : 0;
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

void ComponentManagerImpl::handleError(const std::string& name,
                                       const std::string& operation,
                                       const std::exception& e) {
    lastError_ = std::format("{}: {}", operation, e.what());
    updateComponentState(name, ComponentState::Error);
    notifyListeners(name, ComponentEvent::Error,
                    {{"operation", operation}, {"error", e.what()}});
    LOG_ERROR("{} for {}: {}", operation, name, e.what());
}

void ComponentManagerImpl::notifyListeners(const std::string& component,
                                           ComponentEvent event,
                                           const json& data) {
    auto it = eventListeners_.find(event);
    if (it != eventListeners_.end()) {
        for (const auto& callback : it->second) {
            try {
                callback(component, event, data);
            } catch (const std::exception& e) {
                LOG_ERROR("Event listener error: {}", e.what());
            }
        }
    }
}

void ComponentManagerImpl::handleFileChange(const fs::path& path,
                                            const std::string& change) {
    LOG_INFO("Component file {} was {}", path.string(), change);

    if (change == "modified") {
        // Handle file modification
        std::string name = path.stem().string();
        if (hasComponent(name)) {
            LOG_INFO("Reloading component {} due to file change", name);
            json params;
            params["name"] = name;
            unloadComponent(params);
            loadComponentByName(name);
        }
    } else if (change == "added") {
        // Handle new file
        std::string name = path.stem().string();
        loadComponentByName(name);
    } else if (change == "removed" || change == "deleted") {
        std::string name = path.stem().string();
        if (hasComponent(name)) {
            LOG_INFO("Unloading component {} due to file removal", name);
            json params;
            params["name"] = name;
            unloadComponent(params);
        }
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
        LOG_ERROR("Component {} not found", name);
        return false;
    }
    // 可添加更多验证逻辑
    return true;
}

bool ComponentManagerImpl::loadComponentByName(const std::string& name) {
    if (componentsDirectory_.empty()) {
        LOG_ERROR("Components directory is not configured; cannot load {}",
                  name);
        return false;
    }

    // Construct component path based on platform
#ifdef _WIN32
    const std::string componentExt = ".dll";
    const std::string pathSep = "\\";
#elif defined(__APPLE__)
    const std::string componentExt = ".dylib";
    const std::string pathSep = "/";
#else
    const std::string componentExt = ".so";
    const std::string pathSep = "/";
#endif

    fs::path basePath = fs::path(componentsDirectory_);

    // Try to find component in the components directory
    fs::path componentPath = basePath / (name + componentExt);

    // Check if file exists, if not try with lib prefix (common on Unix)
    if (!fs::exists(componentPath)) {
        componentPath = basePath / ("lib" + name + componentExt);
    }

    if (!fs::exists(componentPath)) {
        LOG_ERROR("Component file not found: {}", name);
        return false;
    }

    json params;
    params["name"] = name;
    params["path"] = componentPath.string();
    return loadComponent(params);
}

std::vector<std::string> ComponentManagerImpl::discoverComponents(
    const std::string& directory) {
    std::vector<std::string> discovered;
    try {
        if (directory.empty() || !fs::exists(directory)) {
            return discovered;
        }

#if defined(_WIN32)
        const std::vector<std::string> extensions = {".dll"};
#elif defined(__APPLE__)
        const std::vector<std::string> extensions = {".dylib", ".so"};
#else
        const std::vector<std::string> extensions = {".so"};
#endif

        for (const auto& entry : fs::directory_iterator(directory)) {
            if (!entry.is_regular_file()) {
                continue;
            }

            const auto& path = entry.path();
            const auto extension = path.extension().string();
            if (std::find(extensions.begin(), extensions.end(), extension) ==
                extensions.end()) {
                continue;
            }

            discovered.push_back(path.stem().string());
        }
    } catch (const std::exception& e) {
        LOG_ERROR("Failed to discover components in {}: {}", directory,
                  e.what());
    }

    return discovered;
}

}  // namespace lithium
