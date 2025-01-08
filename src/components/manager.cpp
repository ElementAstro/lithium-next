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
              std::vector<std::string>{".so", ".dll"})) {
        LOG_F(INFO, "ComponentManager initialized");
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
            auto instance = moduleLoader_->getInstance<Component>(
                name, params, "createComponent");
            if (!instance) {
                THROW_FAIL_TO_LOAD_COMPONENT(
                    "Failed to create instance for component ", name);
            }

            // Store component
            {
                std::lock_guard<std::mutex> lock(mutex_);
                components_[name] = instance;
            }

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

            // Remove component
            {
                std::lock_guard<std::mutex> lock(mutex_);
                components_.erase(name);
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
        std::lock_guard<std::mutex> lock(mutex_);
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

    auto hasComponent(const std::string& component_name) -> bool {
        std::lock_guard<std::mutex> lock(mutex_);
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

private:
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

    std::shared_ptr<ModuleLoader> moduleLoader_;
    std::unique_ptr<FileTracker> fileTracker_;
    DependencyGraph dependencyGraph_;
    std::unordered_map<std::string, std::shared_ptr<Component>> components_;
    std::mutex mutex_;  // Protects access to components_
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

}  // namespace lithium