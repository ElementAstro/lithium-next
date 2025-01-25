/*
 * component_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-1-4

Description: Component Manager (the core of the plugin system)

**************************************************/

#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#include "atom/components/component.hpp"
#include "atom/memory/memory.hpp"
#include "atom/memory/object.hpp"
#include "atom/type/json_fwd.hpp"

using json = nlohmann::json;

namespace lithium {

// 组件事件类型
enum class ComponentEvent {
    PreLoad,
    PostLoad,
    PreUnload,
    PostUnload,
    ConfigChanged,
    StateChanged,
    Error
};

// 组件生命周期状态
enum class ComponentState {
    Created,
    Initialized,
    Running,
    Paused,
    Stopped,
    Error
};

// 组件配置选项
struct ComponentOptions {
    bool autoStart{true};  // 是否自动启动
    bool lazy{false};      // 是否延迟加载
    int priority{0};       // 加载优先级
    std::string group;     // 组件分组
    json config;           // 自定义配置
};

/**
 * @class ComponentManager
 * @brief Manages the lifecycle and dependencies of components in the system.
 *
 * The ComponentManager is responsible for loading, unloading, and managing
 * components. It also handles the dependency graph of components and ensures
 * that components are loaded and unloaded in the correct order.
 */
class ComponentManagerImpl;

class ComponentManager {
public:
    /**
     * @brief Constructs a new ComponentManager object.
     */
    explicit ComponentManager();

    /**
     * @brief Destroys the ComponentManager object.
     */
    ~ComponentManager();

    /**
     * @brief Initializes the ComponentManager.
     *
     * @return true if initialization is successful, false otherwise.
     */
    auto initialize() -> bool;

    /**
     * @brief Destroys the ComponentManager, unloading all components.
     *
     * @return true if destruction is successful, false otherwise.
     */
    auto destroy() -> bool;

    /**
     * @brief Creates a shared pointer to a new ComponentManager.
     *
     * @return A shared pointer to a new ComponentManager.
     */
    static auto createShared() -> std::shared_ptr<ComponentManager>;

    /**
     * @brief Loads a component based on the provided parameters.
     *
     * @param params JSON object containing the parameters for the component.
     * @return true if the component is loaded successfully, false otherwise.
     */
    auto loadComponent(const json& params) -> bool;

    /**
     * @brief Unloads a component based on the provided parameters.
     *
     * @param params JSON object containing the parameters for the component.
     * @return true if the component is unloaded successfully, false otherwise.
     */
    auto unloadComponent(const json& params) -> bool;

    /**
     * @brief Scans the specified path for components.
     *
     * @param path The path to scan for components.
     * @return A vector of strings representing the new or modified components
     * found.
     */
    auto scanComponents(const std::string& path) -> std::vector<std::string>;

    /**
     * @brief Retrieves a component by its name.
     *
     * @param component_name The name of the component.
     * @return An optional weak pointer to the component if found, std::nullopt
     * otherwise.
     */
    auto getComponent(const std::string& component_name)
        -> std::optional<std::weak_ptr<Component>>;

    /**
     * @brief Retrieves information about a component by its name.
     *
     * @param component_name The name of the component.
     * @return An optional JSON object containing the component information if
     * found, std::nullopt otherwise.
     */
    auto getComponentInfo(const std::string& component_name)
        -> std::optional<json>;

    /**
     * @brief Retrieves a list of all components.
     *
     * @return A vector of strings representing the names of all components.
     */
    auto getComponentList() -> std::vector<std::string>;

    /**
     * @brief Retrieves the documentation of a component by its name.
     *
     * @param component_name The name of the component.
     * @return A string containing the documentation of the component.
     */
    auto getComponentDoc(const std::string& component_name) -> std::string;

    /**
     * @brief Checks if a component exists by its name.
     *
     * @param component_name The name of the component.
     * @return true if the component exists, false otherwise.
     */
    auto hasComponent(const std::string& component_name) -> bool;

    /**
     * @brief Prints the dependency tree of all components.
     */
    void printDependencyTree();

    // 简化的组件创建接口
    template <typename T>
    auto createComponent(const std::string& name,
                         const ComponentOptions& options = {})
        -> std::shared_ptr<T>;

    // 组件生命周期管理
    auto startComponent(const std::string& name) -> bool;
    auto stopComponent(const std::string& name) -> bool;
    auto pauseComponent(const std::string& name) -> bool;
    auto resumeComponent(const std::string& name) -> bool;

    // 事件监听
    using EventCallback =
        std::function<void(const std::string&, ComponentEvent, const json&)>;
    void addEventListener(ComponentEvent event, EventCallback callback);
    void removeEventListener(ComponentEvent event);

    // 批量操作
    auto batchLoad(const std::vector<std::string>& components) -> bool;
    auto batchUnload(const std::vector<std::string>& components) -> bool;

    // 组件状态查询
    auto getComponentState(const std::string& name) -> ComponentState;

    // 配置管理
    void updateConfig(const std::string& name, const json& config);
    auto getConfig(const std::string& name) -> json;

    // 组件分组管理
    void addToGroup(const std::string& name, const std::string& group);
    auto getGroupComponents(const std::string& group)
        -> std::vector<std::string>;

    // 性能监控
    auto getPerformanceMetrics() -> json;
    void enablePerformanceMonitoring(bool enable);

    // 错误处理
    auto getLastError() -> std::string;
    void clearErrors();

private:
    /**
     * @brief Updates the dependency graph for a component.
     *
     * @param component_name The name of the component.
     * @param version The version of the component.
     * @param dependencies A vector of strings representing the dependencies of
     * the component.
     * @param dependencies_version A vector of strings representing the versions
     * of the dependencies.
     */
    void updateDependencyGraph(
        const std::string& component_name, const std::string& version,
        const std::vector<std::string>& dependencies,
        const std::vector<std::string>& dependencies_version);

    std::unique_ptr<ComponentManagerImpl> impl_;

    // 新增私有成员
    std::unordered_map<ComponentEvent, std::vector<EventCallback>>
        eventListeners_;
    std::unordered_map<std::string, ComponentState> componentStates_;
    std::unordered_map<std::string, ComponentOptions> componentOptions_;
    std::unordered_map<std::string, std::vector<std::string>> componentGroups_;

    std::string lastError_;
    bool performanceMonitoringEnabled_{false};

    // 新增内存池相关成员
    std::shared_ptr<atom::memory::ObjectPool<std::shared_ptr<Component>>>
        component_pool_;
    std::unique_ptr<MemoryPool<char, 4096>> memory_pool_;

    // 内部辅助方法
    void notifyListeners(const std::string& component, ComponentEvent event,
                         const json& data = {});
    auto validateComponentOperation(const std::string& name) -> bool;
    void updateComponentState(const std::string& name, ComponentState state);
    auto initializeComponent(const std::string& name) -> bool;
};

}  // namespace lithium
