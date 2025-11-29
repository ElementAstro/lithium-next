/**
 * @file api_adapter.hpp
 * @brief Task Engine API Data Types and Utilities
 *
 * Provides core data structures, converters, and event types for the task
 * engine API. This module is used by server controllers to handle API requests.
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#ifndef LITHIUM_TASK_API_ADAPTER_HPP
#define LITHIUM_TASK_API_ADAPTER_HPP

#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "atom/type/json.hpp"

namespace lithium::task {

using json = nlohmann::json;

// Forward declarations
class ExposureSequence;
class Target;
class Task;

/**
 * @namespace api
 * @brief API data types and utilities
 */
namespace api {

// ============================================================================
// Request/Response Structures
// ============================================================================

/**
 * @brief API request context
 */
struct RequestContext {
    std::string requestId;
    std::string apiKey;
    std::string userId;
    std::unordered_map<std::string, std::string> headers;
    json queryParams;
    std::string clientAddress;
    uint64_t timestamp;
};

/**
 * @brief API response structure
 */
struct ApiResponse {
    int statusCode{200};
    std::string status{"success"};
    json data;
    std::string message;

    struct Error {
        std::string code;
        std::string message;
        json details;
    } error;

    bool hasError{false};

    // Helper methods
    static ApiResponse success(const json& data,
                               const std::string& message = "");
    static ApiResponse accepted(const json& data,
                                const std::string& message = "");
    static ApiResponse makeError(int statusCode, const std::string& errorCode,
                                 const std::string& errorMessage,
                                 const json& details = json());

    /**
     * @brief Convert response to JSON
     */
    json toJson() const;
};

// ============================================================================
// WebSocket Event Types
// ============================================================================

/**
 * @brief WebSocket event types
 */
enum class WsEventType {
    // Sequence events
    SequenceStart,
    SequenceProgress,
    SequencePaused,
    SequenceResumed,
    SequenceComplete,
    SequenceAborted,

    // Target events
    TargetStart,
    TargetProgress,
    TargetComplete,
    TargetFailed,

    // Task events
    TaskStart,
    TaskProgress,
    TaskComplete,
    TaskFailed,

    // Exposure events
    ExposureStarted,
    ExposureProgress,
    ExposureFinished,
    ExposureAborted,

    // Device events
    DeviceConnected,
    DeviceDisconnected,
    DeviceStatusUpdate,

    // System events
    Notification,
    Error
};

/**
 * @brief Convert WsEventType to string
 */
std::string wsEventTypeToString(WsEventType type);

/**
 * @brief WebSocket event data
 */
struct WsEvent {
    WsEventType type;
    std::string eventType;  // String representation
    std::string timestamp;  // ISO 8601
    json data;
    std::string correlationId;  // Optional, for command responses

    json toJson() const;
    static WsEvent fromJson(const json& j);
    static WsEvent create(WsEventType type, const json& data,
                          const std::string& correlationId = "");
};

// ============================================================================
// Data Converters
// ============================================================================

/**
 * @brief Sequence data model converter
 *
 * Converts between API JSON format and internal task engine structures
 */
class SequenceConverter {
public:
    /**
     * @brief Convert API sequence JSON to internal Target
     */
    static std::shared_ptr<Target> fromApiJson(const json& sequenceJson);

    /**
     * @brief Convert internal Target to API JSON
     */
    static json toApiJson(const std::shared_ptr<Target>& target);

    /**
     * @brief Convert task parameters from API format
     */
    static json convertTaskParams(const std::string& taskType,
                                  const json& apiParams);

    /**
     * @brief Validate sequence JSON against schema
     * @return Validation errors (empty if valid)
     */
    static std::vector<std::string> validateSequence(const json& sequenceJson);
};

/**
 * @brief Error code mapper
 */
class ErrorMapper {
public:
    struct ErrorInfo {
        int httpStatus;
        std::string errorCode;
        std::string message;
        json details;
    };

    /**
     * @brief Map exception to error info
     */
    static ErrorInfo mapException(const std::exception& exception);

    /**
     * @brief Create error response from exception
     */
    static ApiResponse createErrorResponse(const std::exception& exception);

    /**
     * @brief Create validation error response
     */
    static ApiResponse createValidationError(const std::string& field,
                                             const std::string& message,
                                             const json& value = json());
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * @brief Get current ISO 8601 timestamp
 */
std::string getCurrentTimestamp();

/**
 * @brief Generate unique ID with prefix
 */
std::string generateUniqueId(const std::string& prefix);

}  // namespace api
}  // namespace lithium::task

#endif  // LITHIUM_TASK_API_ADAPTER_HPP
