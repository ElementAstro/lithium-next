/*
 * tool_registry.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file tool_registry.hpp
 * @brief Python Tool Registry for C++ Integration
 * @date 2024
 * @version 1.0.0
 *
 * This module provides C++ interface for discovering, registering,
 * and invoking Python tools with pybind11 adapters.
 *
 * Features:
 * - Automatic discovery of Python tools
 * - Thread-safe registry access
 * - JSON-based function invocation
 * - Tool metadata introspection
 * - Hot-reload support
 */

#ifndef LITHIUM_SCRIPT_TOOLS_TOOL_REGISTRY_HPP
#define LITHIUM_SCRIPT_TOOLS_TOOL_REGISTRY_HPP

#include <chrono>
#include <expected>
#include <filesystem>
#include <functional>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

#include "tool_info.hpp"
#include "types.hpp"
#include "invocation.hpp"

namespace lithium::tools {

/**
 * @brief Callback for tool events
 */
using ToolEventCallback = std::function<void(
    std::string_view eventType,
    std::string_view toolName,
    const nlohmann::json& data
)>;

/**
 * @brief Python Tool Registry
 *
 * Provides C++ interface for discovering, registering, and invoking
 * Python tools with pybind11 adapters.
 *
 * Thread-safe for concurrent access from multiple threads.
 */
class PythonToolRegistry {
public:
    /**
     * @brief Constructs a PythonToolRegistry with default configuration
     */
    PythonToolRegistry();

    /**
     * @brief Constructs a PythonToolRegistry with specified configuration
     */
    explicit PythonToolRegistry(const ToolRegistryConfig& config);

    /**
     * @brief Destructor
     */
    ~PythonToolRegistry();

    // Disable copy
    PythonToolRegistry(const PythonToolRegistry&) = delete;
    PythonToolRegistry& operator=(const PythonToolRegistry&) = delete;

    // Enable move
    PythonToolRegistry(PythonToolRegistry&&) noexcept;
    PythonToolRegistry& operator=(PythonToolRegistry&&) noexcept;

    // =========================================================================
    // Initialization
    // =========================================================================

    /**
     * @brief Initialize the registry
     * @return Success or error
     */
    [[nodiscard]] ToolResult<void> initialize();

    /**
     * @brief Check if the registry is initialized
     */
    [[nodiscard]] bool isInitialized() const noexcept;

    /**
     * @brief Shutdown the registry
     */
    void shutdown();

    // =========================================================================
    // Discovery
    // =========================================================================

    /**
     * @brief Discover all Python tools in the tools directory
     * @return List of discovered tool names
     */
    [[nodiscard]] ToolResult<std::vector<std::string>> discoverTools();

    /**
     * @brief Discover a specific tool
     * @param toolName Name of the tool to discover
     * @return Success or error
     */
    [[nodiscard]] ToolResult<void> discoverTool(std::string_view toolName);

    /**
     * @brief Reload a tool (for hot-reload)
     * @param toolName Name of the tool to reload
     * @return Success or error
     */
    [[nodiscard]] ToolResult<void> reloadTool(std::string_view toolName);

    // =========================================================================
    // Registration
    // =========================================================================

    /**
     * @brief Register a tool manually
     * @param info Tool information
     * @return Success or error
     */
    [[nodiscard]] ToolResult<void> registerTool(const ToolInfo& info);

    /**
     * @brief Unregister a tool
     * @param toolName Name of the tool to unregister
     * @return True if the tool was found and unregistered
     */
    bool unregisterTool(std::string_view toolName);

    // =========================================================================
    // Query
    // =========================================================================

    /**
     * @brief Get list of all registered tool names
     */
    [[nodiscard]] std::vector<std::string> getToolNames() const;

    /**
     * @brief Check if a tool is registered
     */
    [[nodiscard]] bool hasTool(std::string_view toolName) const;

    /**
     * @brief Get information about a tool
     * @param toolName Name of the tool
     * @return Tool information or empty if not found
     */
    [[nodiscard]] std::optional<ToolInfo> getToolInfo(std::string_view toolName) const;

    /**
     * @brief Get information about a function
     * @param toolName Name of the tool
     * @param functionName Name of the function
     * @return Function information or empty if not found
     */
    [[nodiscard]] std::optional<ToolFunctionInfo> getFunctionInfo(
        std::string_view toolName,
        std::string_view functionName
    ) const;

    /**
     * @brief Get all tools in a category
     * @param category Category name
     * @return List of tool names in the category
     */
    [[nodiscard]] std::vector<std::string> getToolsByCategory(
        std::string_view category
    ) const;

    /**
     * @brief Get all available categories
     */
    [[nodiscard]] std::vector<std::string> getCategories() const;

    // =========================================================================
    // Invocation
    // =========================================================================

    /**
     * @brief Invoke a tool function synchronously
     * @param toolName Name of the tool
     * @param functionName Name of the function
     * @param args Arguments as JSON object
     * @return Invocation result
     */
    [[nodiscard]] ToolResult<ToolInvocationResult> invoke(
        std::string_view toolName,
        std::string_view functionName,
        const nlohmann::json& args = nlohmann::json::object()
    );

    /**
     * @brief Invoke a tool function with timeout
     * @param toolName Name of the tool
     * @param functionName Name of the function
     * @param args Arguments as JSON object
     * @param timeout Maximum execution time
     * @return Invocation result
     */
    [[nodiscard]] ToolResult<ToolInvocationResult> invokeWithTimeout(
        std::string_view toolName,
        std::string_view functionName,
        const nlohmann::json& args,
        std::chrono::milliseconds timeout
    );

    /**
     * @brief Invoke a tool function asynchronously
     * @param toolName Name of the tool
     * @param functionName Name of the function
     * @param args Arguments as JSON object
     * @return Future containing the result
     */
    [[nodiscard]] std::future<ToolResult<ToolInvocationResult>> invokeAsync(
        std::string_view toolName,
        std::string_view functionName,
        const nlohmann::json& args = nlohmann::json::object()
    );

    // =========================================================================
    // Events
    // =========================================================================

    /**
     * @brief Set callback for tool events
     * @param callback Event callback function
     */
    void setEventCallback(ToolEventCallback callback);

    // =========================================================================
    // Export
    // =========================================================================

    /**
     * @brief Export the registry as JSON
     * @return JSON representation of all registered tools
     */
    [[nodiscard]] nlohmann::json exportToJson() const;

    /**
     * @brief Export the registry as JSON string
     * @return JSON string representation
     */
    [[nodiscard]] std::string exportToJsonString() const;

    // =========================================================================
    // Statistics
    // =========================================================================

    /**
     * @brief Get registry statistics
     */
    struct Statistics {
        size_t totalTools{0};
        size_t loadedTools{0};
        size_t totalFunctions{0};
        size_t totalInvocations{0};
        size_t successfulInvocations{0};
        size_t failedInvocations{0};
        std::chrono::milliseconds totalExecutionTime{0};
    };

    [[nodiscard]] Statistics getStatistics() const;

    // =========================================================================
    // Singleton Access
    // =========================================================================

    /**
     * @brief Get the global registry instance
     */
    [[nodiscard]] static PythonToolRegistry& getInstance();

    /**
     * @brief Initialize the global registry
     */
    static ToolResult<void> initializeGlobal(const ToolRegistryConfig& config = {});

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;

    static std::unique_ptr<PythonToolRegistry> globalInstance_;
    static std::shared_mutex globalMutex_;
};

/**
 * @brief RAII guard for GIL acquisition when invoking tools
 */
class ToolInvocationGuard {
public:
    explicit ToolInvocationGuard(PythonToolRegistry& registry);
    ~ToolInvocationGuard();

    ToolInvocationGuard(const ToolInvocationGuard&) = delete;
    ToolInvocationGuard& operator=(const ToolInvocationGuard&) = delete;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::tools

#endif  // LITHIUM_SCRIPT_TOOLS_TOOL_REGISTRY_HPP
