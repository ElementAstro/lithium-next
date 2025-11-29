/*
 * invocation.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file invocation.cpp
 * @brief Implementation of tool invocation utilities
 */

#include "invocation.hpp"

#include <Python.h>

namespace lithium::tools {

// =============================================================================
// ToolInvocationResult Implementation
// =============================================================================

nlohmann::json ToolInvocationResult::toJson() const {
    nlohmann::json j;
    j["success"] = success;
    if (!data.is_null()) {
        j["data"] = data;
    }
    if (error) {
        j["error"] = *error;
    }
    if (errorType) {
        j["error_type"] = *errorType;
    }
    if (traceback) {
        j["traceback"] = *traceback;
    }
    if (!metadata.is_null()) {
        j["metadata"] = metadata;
    }
    j["execution_time_ms"] = executionTime.count();
    return j;
}

ToolInvocationResult ToolInvocationResult::fromJson(const nlohmann::json& j) {
    ToolInvocationResult result;
    result.success = j.value("success", false);
    if (j.contains("data")) {
        result.data = j["data"];
    }
    if (j.contains("error")) {
        result.error = j["error"].get<std::string>();
    }
    if (j.contains("error_type")) {
        result.errorType = j["error_type"].get<std::string>();
    }
    if (j.contains("traceback")) {
        result.traceback = j["traceback"].get<std::string>();
    }
    if (j.contains("metadata")) {
        result.metadata = j["metadata"];
    }
    if (j.contains("execution_time_ms")) {
        result.executionTime = std::chrono::milliseconds(
            j["execution_time_ms"].get<int64_t>());
    }
    return result;
}

// =============================================================================
// ToolInvocationGuard Implementation
// =============================================================================

/**
 * @brief Implementation class for ToolInvocationGuard
 *
 * Manages the Python GIL state using PyGILState API.
 */
class ToolInvocationGuard::Impl {
public:
    /**
     * @brief Acquire the GIL on construction
     */
    Impl() {
        // Acquire GIL
        gilState_ = PyGILState_Ensure();
    }

    /**
     * @brief Release the GIL on destruction
     */
    ~Impl() {
        // Release GIL
        PyGILState_Release(gilState_);
    }

private:
    PyGILState_STATE gilState_;
};

ToolInvocationGuard::ToolInvocationGuard(PythonToolRegistry& /*registry*/)
    : pImpl_(std::make_unique<Impl>()) {}

ToolInvocationGuard::~ToolInvocationGuard() = default;

}  // namespace lithium::tools
