#include "manager.hpp"

#include <filesystem>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

#include "atom/error/exception.hpp"
#include "atom/log/loguru.hpp"

#include "dependency.hpp"
#include "loader.hpp"
#include "tracker.hpp"

namespace fs = std::filesystem;

namespace lithium {
class FailToLoadComponent : public atom::error::Exception {
public:
    using Exception::Exception;
};

#define THROW_FAIL_TO_LOAD_COMPONENT(...)                              \
    throw lithium::FailToLoadComponent(ATOM_FILE_NAME, ATOM_FILE_LINE, \
                                       ATOM_FUNC_NAME, __VA_ARGS__)

class ComponentManagerImpl {
public:
    ComponentManagerImpl()
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

    ~ComponentManagerImpl() {
        destroy();
        LOG_F(INFO, "ComponentManager destroyed");
    }

    auto initialize() -> bool {
        try {
            fileTracker_->scan();
            fileTracker_->startWatching();
            fileTracker_->setChangeCallback(
                [this](const fs::path& path, const std::string& change) {
                    handleFileChange(path, change);
                });
            LOG_F(INFO, "ComponentManager initialized successfully");
            return true;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to initialize ComponentManager: {}", e.what());
            return false;
        }
    }

    auto destroy() -> bool {
        try {
            fileTracker_->stopWatching();
            moduleLoader_->unloadAllModules();
            components_.clear();
            componentOptions_.clear();  // 清理组件选项
            LOG_F(INFO, "ComponentManager destroyed successfully");
            return true;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to destroy ComponentManager: {}", e.what());
            return false;
        }
    }

    auto loadComponent(const json& params) -> bool {
        try {
            std::string name = params.at("name").get<std::string>();

            // 使用对象池分配Component实例
            auto instance = component_pool_->acquire();
            if (!instance) {
                THROW_FAIL_TO_LOAD_COMPONENT(
                    "Failed to acquire component instance from pool for ",
                    name);
            }

            // 使用内存池分配其他小型数据结构
            ComponentOptions* options = reinterpret_cast<ComponentOptions*>(
                memory_pool_->allocate(sizeof(ComponentOptions)));
            new (options) ComponentOptions();  // placement new

            std::string path = params.at("path").get<std::string>();
            std::string version = params.value("version", "1.0.0");

            Version ver = Version::parse(version);

            // Check if component already loaded
            if (components_.contains(name)) {
                LOG_F(WARNING, "Component {} already loaded", name);
                return false;
            }

            // Add component to dependency graph
            dependencyGraph_.addNode(name, ver);

            // Add dependencies if specified
            if (params.contains("dependencies")) {
                for (const auto& dep : params["dependencies"]) {
                    std::string depName = dep.at("name").get<std::string>();
                    std::string depVersionStr =
                        dep.at("version").get<std::string>();
                    Version depVer = Version::parse(depVersionStr);
                    dependencyGraph_.addDependency(name, depName, depVer);
                }
            }

            // Load module
            if (!moduleLoader_->loadModule(path, name)) {
                THROW_FAIL_TO_LOAD_COMPONENT(
                    "Failed to load module for component ", name);
            }

            // Get component instance

            *instance = moduleLoader_->getInstance<Component>(
                name, params, "createComponent");
            if (!instance) {
                THROW_FAIL_TO_LOAD_COMPONENT(
                    "Failed to create instance for component ", name);
            }

            // Store component
            {
                std::lock_guard lock(mutex_);
                components_[name] = *instance;

                // 初始化组件选项
                if (params.contains("config")) {
                    options->config = params["config"];
                }
                options->priority = params.value("priority", 0);
                options->autoStart = params.value("autoStart", false);
                componentOptions_[name] = *options;
            }

            memory_pool_->deallocate(reinterpret_cast<char*>(options),
                                     sizeof(ComponentOptions));

            LOG_F(INFO, "Component {} loaded successfully", name);
            return true;

        } catch (const json::exception& e) {
            LOG_F(ERROR, "JSON parsing error while loading component: {}",
                  e.what());
            return false;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to load component: {}", e.what());
            return false;
        }
    }

    auto unloadComponent(const json& params) -> bool {
        try {
            std::string name = params.at("name").get<std::string>();
            if (!components_.contains(name)) {
                LOG_F(WARNING, "Component {} not found", name);
                return false;
            }

            // Check for dependent components
            auto dependents = dependencyGraph_.getDependents(name);
            if (!dependents.empty()) {
                LOG_F(ERROR,
                      "Cannot unload component {} as other components depend "
                      "on it",
                      name);
                return false;
            }

            // Remove from dependency graph
            dependencyGraph_.removeNode(name);

            // Unload module
            moduleLoader_->unloadModule(name);

            // Remove component and its options
            {
                std::lock_guard lock(mutex_);
                // Component实例会自动返回对象池
                components_.erase(name);
                componentOptions_.erase(name);
            }

            LOG_F(INFO, "Component {} unloaded successfully", name);
            return true;

        } catch (const json::exception& e) {
            LOG_F(ERROR, "JSON parsing error while unloading component: {}",
                  e.what());
            return false;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to unload component: {}", e.what());
            return false;
        }
    }

    auto scanComponents(const std::string& path) -> std::vector<std::string> {
        try {
            fileTracker_->scan();
            auto differences = fileTracker_->getDifferences();
            std::vector<std::string> newComponents;

            for (const auto& [filePath, info] : differences.items()) {
                if (info.at("status").get<std::string>() == "new" ||
                    info.at("status").get<std::string>() == "modified") {
                    newComponents.emplace_back(filePath);
                }
            }

            LOG_F(INFO,
                  "Scanned components. New or modified components found: {}",
                  newComponents.size());
            return newComponents;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to scan components: {}", e.what());
            return {};
        }
    }

    auto getComponent(const std::string& component_name)
        -> std::optional<std::weak_ptr<Component>> {
        std::lock_guard lock(mutex_);
        if (auto it = components_.find(component_name);
            it != components_.end()) {
            return it->second;
        }
        return std::nullopt;
    }

    auto getComponentInfo(const std::string& component_name)
        -> std::optional<json> {
        try {
            if (!components_.contains(component_name)) {
                return std::nullopt;
            }

            json info;
            info["name"] = component_name;

            // Get component version
            if (auto mod = moduleLoader_->getModule(component_name)) {
                info["version"] = mod->version;
                info["status"] = mod->status;
                info["author"] = mod->author;
                info["description"] = mod->description;
            }

            // Get dependencies
            info["dependencies"] =
                dependencyGraph_.getDependencies(component_name);

            LOG_F(INFO, "Retrieved info for component {}", component_name);
            return info;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to get component info for {}: {}",
                  component_name, e.what());
            return std::nullopt;
        }
    }

    auto getComponentList() -> std::vector<std::string> {
        try {
            auto list = moduleLoader_->getAllExistedModules();
            LOG_F(INFO, "Retrieved component list with {} components",
                  list.size());
            return list;
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to get component list: {}", e.what());
            return {};
        }
    }

    auto getComponentDoc(const std::string& component_name) -> std::string {
        try {
            if (!components_.contains(component_name)) {
                return "";
            }

            if (auto mod = moduleLoader_->getModule(component_name)) {
                return mod->description;
            }

            return "";
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to get component doc for {}: {}",
                  component_name, e.what());
            return "";
        }
    }

    auto hasComponent(const std::string& component_name) -> bool {
        std::lock_guard lock(mutex_);
        return components_.contains(component_name);
    }

    void updateDependencyGraph(
        const std::string& component_name, const std::string& version,
        const std::vector<std::string>& dependencies,
        const std::vector<std::string>& dependencies_version) {
        try {
            Version ver = Version::parse(version);
            dependencyGraph_.addNode(component_name, ver);

            for (size_t i = 0; i < dependencies.size(); i++) {
                Version depVer = Version::parse(dependencies_version[i]);
                dependencyGraph_.addDependency(component_name, dependencies[i],
                                               depVer);
            }

            LOG_F(INFO, "Updated dependency graph for component {}",
                  component_name);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to update dependency graph for {}: {}",
                  component_name, e.what());
        }
    }

    void printDependencyTree() {
        try {
            auto orderedDeps = dependencyGraph_.topologicalSort();
            if (!orderedDeps) {
                LOG_F(ERROR, "Circular dependency detected!");
                return;
            }

            LOG_F(INFO, "Dependency Tree:");
            for (const auto& component : *orderedDeps) {
                auto deps = dependencyGraph_.getDependencies(component);
                LOG_F(INFO, "Component: {} depends on:", component);
                for (const auto& dep : deps) {
                    LOG_F(INFO, "  - {}", dep);
                }
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to print dependency tree: {}", e.what());
        }
    }

    auto initializeComponent(const std::string& name) -> bool {
        try {
            auto component = components_[name];
            if (!component)
                return false;

            notifyListeners(name, ComponentEvent::PreLoad);

            const auto& options = componentOptions_[name];
            // if (!options.config.empty()) {
            ///   component->configure(options.config);
            //}

            if (options.autoStart) {
                startComponent(name);
            }

            updateComponentState(name, ComponentState::Initialized);
            notifyListeners(name, ComponentEvent::PostLoad);
            return true;
        } catch (const std::exception& e) {
            lastError_ = e.what();
            updateComponentState(name, ComponentState::Error);
            notifyListeners(name, ComponentEvent::Error, {{"error", e.what()}});
            return false;
        }
    }

    auto startComponent(const std::string& name) -> bool {
        if (!validateComponentOperation(name))
            return false;

        try {
            auto component = components_[name];
            component->initialize();
            updateComponentState(name, ComponentState::Running);
            notifyListeners(name, ComponentEvent::StateChanged);
            return true;
        } catch (const std::exception& e) {
            handleError(name, "Failed to start component", e);
            return false;
        }
    }

    void updateConfig(const std::string& name, const json& config) {
        if (!validateComponentOperation(name))
            return;

        try {
            auto& options = componentOptions_[name];
            options.config.merge_patch(config);

            auto component = components_[name];
            // component->configure(options.config);

            notifyListeners(name, ComponentEvent::ConfigChanged, config);
        } catch (const std::exception& e) {
            handleError(name, "Failed to update config", e);
        }
    }

    auto batchLoad(const std::vector<std::string>& components) -> bool {
        bool success = true;
        std::vector<std::future<bool>> futures;

        // 按优先级排序
        auto sortedComponents = components;
        std::sort(sortedComponents.begin(), sortedComponents.end(),
                  [this](const std::string& a, const std::string& b) {
                      return componentOptions_[a].priority >
                             componentOptions_[b].priority;
                  });

        // 并行加载
        for (const auto& name : sortedComponents) {
            if (componentOptions_[name].autoStart)
                continue;

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

    auto getPerformanceMetrics() -> json {
        if (!performanceMonitoringEnabled_)
            return {};

        json metrics;
        for (const auto& [name, component] : components_) {
            /*
            json componentMetrics = {
                {"state",
            std::string(magic_enum::enum_name(componentStates_[name]))},
                {"load_time", component->getLoadTime()},
                {"memory_usage", component->getMemoryUsage()},
                {"error_count", component->getErrorCount()}
            };
            metrics[name] = componentMetrics;
            */
        }
        return metrics;
    }

    void handleError(const std::string& name, const std::string& operation,
                     const std::exception& e) {
        lastError_ = std::format("{}: {}", operation, e.what());
        updateComponentState(name, ComponentState::Error);
        notifyListeners(name, ComponentEvent::Error,
                        {{"operation", operation}, {"error", e.what()}});
        LOG_F(ERROR, "{} for {}: {}", operation, name, e.what());
    }

    void notifyListeners(const std::string& component, ComponentEvent event,
                         const json& data = {}) {
        auto it = eventListeners_.find(event);
        if (it != eventListeners_.end()) {
            for (const auto& listener : it->second) {
                try {
                    listener(component, event, data);
                } catch (const std::exception& e) {
                    LOG_F(ERROR, "Event listener error: {}", e.what());
                }
            }
        }
    }

    void handleFileChange(const fs::path& path, const std::string& change) {
        LOG_F(INFO, "Component file {} was {}", path.string(), change);

        if (change == "modified") {
            // Reload component if it's loaded
            std::string name = path.stem().string();
            if (components_.contains(name)) {
                json params;
                params["name"] = name;
                params["path"] = path.string();
                if (unloadComponent(params)) {
                    loadComponent(params);
                }
            }
        } else if (change == "deleted") {
            // Unload component if it's loaded
            std::string name = path.stem().string();
            if (components_.contains(name)) {
                json params;
                params["name"] = name;
                unloadComponent(params);
            }
        }
    }

    // 新增成员变量
    std::shared_ptr<ModuleLoader> moduleLoader_;
    std::unique_ptr<FileTracker> fileTracker_;
    DependencyGraph dependencyGraph_;
    std::unordered_map<std::string, std::shared_ptr<Component>> components_;
    std::unordered_map<std::string, ComponentOptions>
        componentOptions_;  // 组件选项
    std::unordered_map<std::string, ComponentState> componentStates_;
    std::mutex mutex_;       // Protects access to components_
    std::string lastError_;  // 最后错误信息
    bool performanceMonitoringEnabled_ = true;
    std::shared_ptr<atom::memory::ObjectPool<std::shared_ptr<Component>>>
        component_pool_;
    std::unique_ptr<MemoryPool<char, 4096>> memory_pool_;

    // 新增方法
    void updateComponentState(const std::string& name,
                              ComponentState newState) {
        std::lock_guard lock(mutex_);
        componentStates_[name] = newState;
    }

    bool validateComponentOperation(const std::string& name) {
        std::lock_guard lock(mutex_);
        if (!components_.contains(name)) {
            LOG_F(ERROR, "Component {} does not exist", name);
            return false;
        }
        // 可添加更多验证逻辑
        return true;
    }

    bool loadComponentByName(const std::string& name) {
        json params;
        params["name"] = name;
        params["path"] = "/path/to/" + name;  // 替换为实际路径
        return loadComponent(params);
    }

    // 事件监听器
    std::unordered_map<ComponentEvent,
                       std::vector<std::function<void(
                           const std::string&, ComponentEvent, const json&)>>>
        eventListeners_;
};

ComponentManager::ComponentManager()
    : impl_(std::make_unique<ComponentManagerImpl>()) {}

ComponentManager::~ComponentManager() = default;

auto ComponentManager::initialize() -> bool { return impl_->initialize(); }

auto ComponentManager::destroy() -> bool { return impl_->destroy(); }

auto ComponentManager::createShared() -> std::shared_ptr<ComponentManager> {
    return std::make_shared<ComponentManager>();
}

auto ComponentManager::loadComponent(const json& params) -> bool {
    return impl_->loadComponent(params);
}

auto ComponentManager::unloadComponent(const json& params) -> bool {
    return impl_->unloadComponent(params);
}

auto ComponentManager::scanComponents(const std::string& path)
    -> std::vector<std::string> {
    return impl_->scanComponents(path);
}

auto ComponentManager::getComponent(const std::string& component_name)
    -> std::optional<std::weak_ptr<Component>> {
    return impl_->getComponent(component_name);
}

auto ComponentManager::getComponentInfo(const std::string& component_name)
    -> std::optional<json> {
    return impl_->getComponentInfo(component_name);
}

auto ComponentManager::getComponentList() -> std::vector<std::string> {
    return impl_->getComponentList();
}

auto ComponentManager::getComponentDoc(const std::string& component_name)
    -> std::string {
    return impl_->getComponentDoc(component_name);
}

auto ComponentManager::hasComponent(const std::string& component_name) -> bool {
    return impl_->hasComponent(component_name);
}

void ComponentManager::printDependencyTree() { impl_->printDependencyTree(); }

void ComponentManager::updateDependencyGraph(
    const std::string& component_name, const std::string& version,
    const std::vector<std::string>& dependencies,
    const std::vector<std::string>& dependencies_version) {
    impl_->updateDependencyGraph(component_name, version, dependencies,
                                 dependencies_version);
}

template <typename T>
auto ComponentManager::createComponent(const std::string& name,
                                       const ComponentOptions& options)
    -> std::shared_ptr<T> {
    json params;
    params["name"] = name;
    params["config"] = options.config;
    params["autoStart"] = options.autoStart;
    params["priority"] = options.priority;

    if (loadComponent(params)) {
        if (auto comp = getComponent(name)) {
            return std::dynamic_pointer_cast<T>(comp->lock());
        }
    }
    return nullptr;
}

auto ComponentManager::startComponent(const std::string& name) -> bool {
    return impl_->startComponent(name);
}

auto ComponentManager::stopComponent(const std::string& name) -> bool {
    if (auto comp = getComponent(name)) {
        auto component = comp->lock();
        if (component) {
            // TODO: What is the stop method?
            // component->stop();
            impl_->updateComponentState(name, ComponentState::Stopped);
            return true;
        }
    }
    return false;
}

auto ComponentManager::pauseComponent(const std::string& name) -> bool {
    if (auto comp = getComponent(name)) {
        auto component = comp->lock();
        if (component) {
            // TODO: What is the pause method?
            // component->pause();
            impl_->updateComponentState(name, ComponentState::Paused);
            return true;
        }
    }
    return false;
}

auto ComponentManager::resumeComponent(const std::string& name) -> bool {
    if (auto comp = getComponent(name)) {
        auto component = comp->lock();
        if (component) {
            // TODO: What is the resume method?
            // component->resume();
            impl_->updateComponentState(name, ComponentState::Running);
            return true;
        }
    }
    return false;
}

void ComponentManager::addEventListener(ComponentEvent event,
                                        EventCallback callback) {
    impl_->eventListeners_[event].push_back(callback);
}

void ComponentManager::removeEventListener(ComponentEvent event) {
    impl_->eventListeners_.erase(event);
}

auto ComponentManager::batchLoad(const std::vector<std::string>& components)
    -> bool {
    return impl_->batchLoad(components);
}

auto ComponentManager::batchUnload(const std::vector<std::string>& components)
    -> bool {
    bool success = true;
    for (const auto& name : components) {
        json params;
        params["name"] = name;
        success &= unloadComponent(params);
    }
    return success;
}

auto ComponentManager::getComponentState(const std::string& name)
    -> ComponentState {
    if (impl_->componentStates_.contains(name)) {
        return impl_->componentStates_[name];
    }
    return ComponentState::Error;
}

void ComponentManager::updateConfig(const std::string& name,
                                    const json& config) {
    impl_->updateConfig(name, config);
}

auto ComponentManager::getConfig(const std::string& name) -> json {
    if (impl_->componentOptions_.contains(name)) {
        return impl_->componentOptions_[name].config;
    }
    return json{};
}

void ComponentManager::addToGroup(const std::string& name,
                                  const std::string& group) {
    componentGroups_[group].push_back(name);
}

auto ComponentManager::getGroupComponents(const std::string& group)
    -> std::vector<std::string> {
    if (componentGroups_.contains(group)) {
        return componentGroups_[group];
    }
    return {};
}

auto ComponentManager::getPerformanceMetrics() -> json {
    return impl_->getPerformanceMetrics();
}

void ComponentManager::enablePerformanceMonitoring(bool enable) {
    impl_->performanceMonitoringEnabled_ = enable;
}

auto ComponentManager::getLastError() -> std::string {
    return impl_->lastError_;
}

void ComponentManager::clearErrors() { impl_->lastError_.clear(); }

}  // namespace lithium
