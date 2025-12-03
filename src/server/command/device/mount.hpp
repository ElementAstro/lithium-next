#ifndef LITHIUM_SERVER_COMMAND_MOUNT_HPP
#define LITHIUM_SERVER_COMMAND_MOUNT_HPP

#include <memory>
#include <string>

#include "atom/type/json.hpp"

namespace lithium::app {

class CommandDispatcher;

void registerMount(std::shared_ptr<CommandDispatcher> dispatcher);

}  // namespace lithium::app

// Middleware functions for HTTP controller
namespace lithium::middleware {

auto listMounts() -> nlohmann::json;
auto getMountStatus(const std::string& deviceId) -> nlohmann::json;
auto connectMount(const std::string& deviceId, bool connected) -> nlohmann::json;
auto slewMount(const std::string& deviceId, const nlohmann::json& body)
    -> nlohmann::json;
auto setTracking(const std::string& deviceId, const nlohmann::json& body)
    -> nlohmann::json;
auto setMountPosition(const std::string& deviceId, const nlohmann::json& body)
    -> nlohmann::json;
auto pulseGuide(const std::string& deviceId, const nlohmann::json& body)
    -> nlohmann::json;
auto syncMount(const std::string& deviceId, const nlohmann::json& body)
    -> nlohmann::json;
auto getMountCapabilities(const std::string& deviceId) -> nlohmann::json;
auto setGuideRates(const std::string& deviceId, const nlohmann::json& body)
    -> nlohmann::json;
auto setTrackingRate(const std::string& deviceId, const nlohmann::json& body)
    -> nlohmann::json;
auto stopMount(const std::string& deviceId) -> nlohmann::json;
auto getPierSide(const std::string& deviceId) -> nlohmann::json;
auto meridianFlip(const std::string& deviceId) -> nlohmann::json;

}  // namespace lithium::middleware

#endif  // LITHIUM_SERVER_COMMAND_MOUNT_HPP
