#ifndef LITHIUM_SERVER_MODELS_API_HPP
#define LITHIUM_SERVER_MODELS_API_HPP

#include <atomic>
#include <chrono>
#include <string>

#include <fmt/format.h>
#include "atom/type/json.hpp"

namespace lithium::models::api {

using json = nlohmann::json;

/**
 * @brief Generate a unique request ID for tracking
 * Format: {timestamp_hex}-{counter_hex}
 */
inline auto generateRequestId() -> std::string {
    static std::atomic<uint64_t> counter{0};
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
                         now.time_since_epoch())
                         .count();
    return fmt::format("{:x}-{:04x}", timestamp, ++counter & 0xFFFF);
}

/**
 * @brief Create a success response with request_id tracking
 */
inline auto makeSuccess(const json& data, const std::string& request_id,
                        const std::string& message = "") -> json {
    json body;
    body["success"] = true;
    body["request_id"] = request_id;
    body["data"] = data;
    if (!message.empty()) {
        body["message"] = message;
    }
    return body;
}

/**
 * @brief Create an accepted response (202)
 */
inline auto makeAccepted(const json& data, const std::string& request_id,
                         const std::string& message = "") -> json {
    return makeSuccess(data, request_id, message);
}

/**
 * @brief Create an error response with request_id tracking
 */
inline auto makeError(const std::string& code, const std::string& message,
                      const std::string& request_id,
                      const json& details = json::object()) -> json {
    json body;
    body["success"] = false;
    body["request_id"] = request_id;
    body["error"] = {{"code", code}, {"message", message}};
    if (!details.empty()) {
        body["error"]["details"] = details;
    }
    return body;
}

/**
 * @brief Create a device not found error
 */
inline auto makeDeviceNotFound(const std::string& deviceId,
                               const std::string& deviceKind,
                               const std::string& request_id) -> json {
    json details;
    details["deviceId"] = deviceId;
    details["deviceType"] = deviceKind;
    return makeError("device_not_found", deviceKind + " not found", request_id,
                     details);
}

}  // namespace lithium::models::api

#endif  // LITHIUM_SERVER_MODELS_API_HPP
