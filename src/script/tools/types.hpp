/*
 * types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file types.hpp
 * @brief Type definitions for Python Tool Registry
 * @date 2024
 * @version 1.0.0
 *
 * This module provides type definitions extracted from the tool registry,
 * including error codes, result types, parameter types, and configuration
 * structures for the Python tool integration system.
 */

#ifndef LITHIUM_SCRIPT_TOOLS_TYPES_HPP
#define LITHIUM_SCRIPT_TOOLS_TYPES_HPP

#include <chrono>
#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

namespace lithium::tools {

/**
 * @brief Error codes for tool registry operations
 */
enum class ToolRegistryError {
    Success = 0,
    NotInitialized,
    ToolNotFound,
    FunctionNotFound,
    InvocationFailed,
    DiscoveryFailed,
    PythonError,
    InvalidArguments,
    Timeout,
    Cancelled,
    SerializationError,
    UnknownError
};

/**
 * @brief Get string representation of ToolRegistryError
 */
[[nodiscard]] constexpr std::string_view toolRegistryErrorToString(
    ToolRegistryError error) noexcept {
    switch (error) {
        case ToolRegistryError::Success:
            return "Success";
        case ToolRegistryError::NotInitialized:
            return "Registry not initialized";
        case ToolRegistryError::ToolNotFound:
            return "Tool not found";
        case ToolRegistryError::FunctionNotFound:
            return "Function not found";
        case ToolRegistryError::InvocationFailed:
            return "Function invocation failed";
        case ToolRegistryError::DiscoveryFailed:
            return "Tool discovery failed";
        case ToolRegistryError::PythonError:
            return "Python error";
        case ToolRegistryError::InvalidArguments:
            return "Invalid arguments";
        case ToolRegistryError::Timeout:
            return "Timeout";
        case ToolRegistryError::Cancelled:
            return "Cancelled";
        case ToolRegistryError::SerializationError:
            return "Serialization error";
        case ToolRegistryError::UnknownError:
            return "Unknown error";
    }
    return "Unknown error";
}

/**
 * @brief Result type for tool registry operations
 */
template<typename T>
using ToolResult = std::expected<T, ToolRegistryError>;

/**
 * @brief Parameter type enumeration
 */
enum class ToolParameterType {
    String,
    Integer,
    Float,
    Boolean,
    List,
    Dict,
    Bytes,
    Path,
    Optional,
    Any
};

/**
 * @brief Configuration for tool registry
 */
struct ToolRegistryConfig {
    std::filesystem::path toolsDirectory;  ///< Path to Python tools directory
    bool autoDiscover{true};               ///< Discover tools on initialization
    bool enableCaching{true};              ///< Cache tool info
    std::chrono::seconds cacheTimeout{300}; ///< Cache expiry time
    bool enableHotReload{false};           ///< Enable hot-reload of tools
    size_t maxConcurrentInvocations{100};  ///< Max concurrent function calls
};

/**
 * @brief Callback for tool events
 */
using ToolEventCallback = std::function<void(
    std::string_view eventType,
    std::string_view toolName,
    const nlohmann::json& data
)>;

}  // namespace lithium::tools

#endif  // LITHIUM_SCRIPT_TOOLS_TYPES_HPP
