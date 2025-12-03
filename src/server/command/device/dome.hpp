#ifndef LITHIUM_SERVER_COMMAND_DOME_HPP
#define LITHIUM_SERVER_COMMAND_DOME_HPP

#include <memory>
#include <string>

#include "atom/type/json.hpp"

namespace lithium::app {

class CommandDispatcher;

void registerDome(std::shared_ptr<CommandDispatcher> dispatcher);

}  // namespace lithium::app

// Middleware functions for HTTP controller
namespace lithium::middleware {

auto listDomes() -> nlohmann::json;
auto getDomeStatus(const std::string& deviceId) -> nlohmann::json;
auto connectDome(const std::string& deviceId, bool connected) -> nlohmann::json;
auto slewDome(const std::string& deviceId, double azimuth) -> nlohmann::json;
auto shutterControl(const std::string& deviceId, bool open) -> nlohmann::json;
auto parkDome(const std::string& deviceId) -> nlohmann::json;
auto unparkDome(const std::string& deviceId) -> nlohmann::json;
auto homeDome(const std::string& deviceId) -> nlohmann::json;
auto getDomeCapabilities(const std::string& deviceId) -> nlohmann::json;

}  // namespace lithium::middleware

#endif  // LITHIUM_SERVER_COMMAND_DOME_HPP
