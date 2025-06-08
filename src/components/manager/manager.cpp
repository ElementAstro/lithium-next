/*
 * manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-1-4

Description: Component Manager Implementation

**************************************************/

#include "manager.hpp"
#include "manager_impl.hpp"

namespace lithium {

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
        auto comp = getComponent(name);
        if (comp) {
            auto component = comp->lock();
            return std::dynamic_pointer_cast<T>(component);
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
            updateComponentState(name, ComponentState::Stopped);
            notifyListeners(name, ComponentEvent::StateChanged);
            return true;
        }
    }
    return false;
}

auto ComponentManager::pauseComponent(const std::string& name) -> bool {
    if (auto comp = getComponent(name)) {
        auto component = comp->lock();
        if (component) {
            updateComponentState(name, ComponentState::Paused);
            notifyListeners(name, ComponentEvent::StateChanged);
            return true;
        }
    }
    return false;
}

auto ComponentManager::resumeComponent(const std::string& name) -> bool {
    if (auto comp = getComponent(name)) {
        auto component = comp->lock();
        if (component) {
            updateComponentState(name, ComponentState::Running);
            notifyListeners(name, ComponentEvent::StateChanged);
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

void ComponentManager::notifyListeners(const std::string& component,
                                       ComponentEvent event, const json& data) {
    impl_->notifyListeners(component, event, data);
}

auto ComponentManager::validateComponentOperation(const std::string& name)
    -> bool {
    return impl_->validateComponentOperation(name);
}

void ComponentManager::updateComponentState(const std::string& name,
                                            ComponentState state) {
    impl_->updateComponentState(name, state);
}

auto ComponentManager::initializeComponent(const std::string& name) -> bool {
    return impl_->initializeComponent(name);
}

}  // namespace lithium
