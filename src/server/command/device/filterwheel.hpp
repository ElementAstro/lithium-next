#ifndef LITHIUM_SERVER_COMMAND_FILTERWHEEL_HPP
#define LITHIUM_SERVER_COMMAND_FILTERWHEEL_HPP

#include <memory>
#include <string>

#include "atom/type/json.hpp"

namespace lithium::app {

class CommandDispatcher;

void registerFilterWheel(std::shared_ptr<CommandDispatcher> dispatcher);

}  // namespace lithium::app

// Middleware functions for HTTP controller
namespace lithium::middleware {

auto listFilterWheels() -> nlohmann::json;
auto getFilterWheelStatus(const std::string& deviceId) -> nlohmann::json;
auto connectFilterWheel(const std::string& deviceId, bool connected)
    -> nlohmann::json;
auto setFilterPosition(const std::string& deviceId, const nlohmann::json& body)
    -> nlohmann::json;
auto setFilterByName(const std::string& deviceId, const nlohmann::json& body)
    -> nlohmann::json;
auto getFilterWheelCapabilities(const std::string& deviceId) -> nlohmann::json;
auto configureFilterNames(const std::string& deviceId,
                          const nlohmann::json& body) -> nlohmann::json;
auto getFilterOffsets(const std::string& deviceId) -> nlohmann::json;
auto setFilterOffsets(const std::string& deviceId, const nlohmann::json& body)
    -> nlohmann::json;
auto haltFilterWheel(const std::string& deviceId) -> nlohmann::json;
auto calibrateFilterWheel(const std::string& deviceId) -> nlohmann::json;

}  // namespace lithium::middleware

#endif  // LITHIUM_SERVER_COMMAND_FILTERWHEEL_HPP
