#ifndef LITHIUM_SERVER_COMMAND_CAMERA_HPP
#define LITHIUM_SERVER_COMMAND_CAMERA_HPP

#include <memory>
#include <string>

#include "atom/type/json.hpp"

namespace lithium::app {

class CommandDispatcher;

void registerCamera(std::shared_ptr<CommandDispatcher> dispatcher);

}  // namespace lithium::app

// Middleware functions for HTTP controller
namespace lithium::middleware {

auto listCameras() -> nlohmann::json;
auto getCameraStatus(const std::string& deviceId) -> nlohmann::json;
auto connectCamera(const std::string& deviceId, bool connected)
    -> nlohmann::json;
auto updateCameraSettings(const std::string& deviceId,
                          const nlohmann::json& settings) -> nlohmann::json;
auto startExposure(const std::string& deviceId, const nlohmann::json& params)
    -> nlohmann::json;
auto abortExposure(const std::string& deviceId) -> nlohmann::json;
auto getCameraCapabilities(const std::string& deviceId) -> nlohmann::json;
auto getCameraGains(const std::string& deviceId) -> nlohmann::json;
auto getCameraOffsets(const std::string& deviceId) -> nlohmann::json;
auto setCoolerPower(const std::string& deviceId, const nlohmann::json& body)
    -> nlohmann::json;
auto warmupCamera(const std::string& deviceId) -> nlohmann::json;

}  // namespace lithium::middleware

#endif  // LITHIUM_SERVER_COMMAND_CAMERA_HPP
