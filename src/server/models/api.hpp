#ifndef LITHIUM_SERVER_MODELS_API_HPP
#define LITHIUM_SERVER_MODELS_API_HPP

#include <string>

#include "atom/type/json.hpp"

namespace lithium::models::api {

using json = nlohmann::json;

inline auto makeSuccess(const json &data, const std::string &message = "")
    -> json {
    json body;
    body["status"] = "success";
    body["data"] = data;
    if (!message.empty()) {
        body["message"] = message;
    }
    return body;
}

inline auto makeAccepted(const json &data, const std::string &message = "")
    -> json {
    // Same shape as success; HTTP 层用 202 状态码区分
    return makeSuccess(data, message);
}

inline auto makeError(const std::string &code, const std::string &message,
                      const json &details = json::object()) -> json {
    json body;
    body["status"] = "error";
    body["error"] = {
        {"code", code},
        {"message", message},
        {"details", details},
    };
    return body;
}

inline auto makeDeviceNotFound(const std::string &deviceId,
                               const std::string &deviceKind) -> json {
    json details;
    details["deviceId"] = deviceId;
    details["deviceType"] = deviceKind;
    return makeError("device_not_found", deviceKind + " not found", details);
}

}  // namespace lithium::models::api

#endif  // LITHIUM_SERVER_MODELS_API_HPP
