/*
 * invocation.hpp
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file invocation.hpp
 * @brief Tool Invocation and Execution Management
 * @date 2024
 * @version 1.0.0
 *
 * This module provides types and utilities for invoking Python tools,
 * including result handling and GIL management for thread-safe execution.
 */

#ifndef LITHIUM_SCRIPT_TOOLS_INVOCATION_HPP
#define LITHIUM_SCRIPT_TOOLS_INVOCATION_HPP

#include "types.hpp"

#include <chrono>
#include <memory>
#include <optional>

#include <Python.h>
#include <nlohmann/json.hpp>

namespace lithium::tools {

// Forward declaration
class PythonToolRegistry;

/**
 * @brief Result of invoking a tool function
 *
 * Encapsulates all information returned from a tool function invocation,
 * including success status, returned data, error information, and
 * execution metrics.
 */
struct ToolInvocationResult {
    bool success{false};                   ///< Whether invocation succeeded
    nlohmann::json data;                   ///< Returned data from tool
    std::optional<std::string> error;      ///< Error message if failed
    std::optional<std::string> errorType;  ///< Type of error (e.g., ValueError)
    std::optional<std::string> traceback;  ///< Python traceback if available
    nlohmann::json metadata;               ///< Additional metadata
    std::chrono::milliseconds executionTime{0};  ///< Time taken to execute

    /**
     * @brief Convert invocation result to JSON
     * @return JSON representation of the result
     */
    [[nodiscard]] nlohmann::json toJson() const;

    /**
     * @brief Create invocation result from JSON
     * @param j JSON object to parse
     * @return Parsed invocation result
     */
    static ToolInvocationResult fromJson(const nlohmann::json& j);
};

/**
 * @brief RAII guard for GIL acquisition when invoking tools
 *
 * Automatically acquires and releases the Python Global Interpreter Lock (GIL)
 * for thread-safe Python C API calls. Follows RAII pattern for exception
 * safety.
 *
 * Example usage:
 * @code
 * PythonToolRegistry& registry = PythonToolRegistry::getInstance();
 * {
 *     ToolInvocationGuard guard(registry);
 *     // Safe to call Python C API here
 * }  // GIL automatically released on scope exit
 * @endcode
 */
class ToolInvocationGuard {
public:
    /**
     * @brief Construct guard and acquire GIL
     * @param registry The tool registry (used for context)
     */
    explicit ToolInvocationGuard(PythonToolRegistry& registry);

    /**
     * @brief Destructor that releases GIL
     */
    ~ToolInvocationGuard();

    // Prevent copying
    ToolInvocationGuard(const ToolInvocationGuard&) = delete;
    ToolInvocationGuard& operator=(const ToolInvocationGuard&) = delete;

    // Allow moving
    ToolInvocationGuard(ToolInvocationGuard&&) noexcept = default;
    ToolInvocationGuard& operator=(ToolInvocationGuard&&) noexcept = default;

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::tools

#endif  // LITHIUM_SCRIPT_TOOLS_INVOCATION_HPP
