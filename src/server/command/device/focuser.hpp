#ifndef LITHIUM_SERVER_COMMAND_FOCUSER_HPP
#define LITHIUM_SERVER_COMMAND_FOCUSER_HPP

#include <memory>
#include <string>

#include "atom/type/json.hpp"

namespace lithium::app {

class CommandDispatcher;

void registerFocuser(std::shared_ptr<CommandDispatcher> dispatcher);

}  // namespace lithium::app

// Middleware functions for HTTP controller
namespace lithium::middleware {

auto listFocusers() -> nlohmann::json;
auto getFocuserStatus(const std::string& deviceId) -> nlohmann::json;
auto connectFocuser(const std::string& deviceId, bool connected)
    -> nlohmann::json;
auto moveFocuser(const std::string& deviceId, const nlohmann::json& body)
    -> nlohmann::json;
auto updateFocuserSettings(const std::string& deviceId,
                           const nlohmann::json& body) -> nlohmann::json;
auto haltFocuser(const std::string& deviceId) -> nlohmann::json;
auto getFocuserCapabilities(const std::string& deviceId) -> nlohmann::json;
auto startAutofocus(const std::string& deviceId, const nlohmann::json& body)
    -> nlohmann::json;
auto getAutofocusStatus(const std::string& deviceId, const std::string& taskId)
    -> nlohmann::json;

}  // namespace lithium::middleware

#endif  // LITHIUM_SERVER_COMMAND_FOCUSER_HPP
