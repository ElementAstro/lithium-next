#ifndef LITHIUM_SERVER_MIDDLEWARE_FILTERWHEEL_HPP
#define LITHIUM_SERVER_MIDDLEWARE_FILTERWHEEL_HPP

#include <string>

#include "atom/type/json.hpp"

namespace lithium::middleware {

using json = nlohmann::json;

auto listFilterWheels() -> json;

auto getFilterWheelStatus(const std::string &deviceId) -> json;

auto connectFilterWheel(const std::string &deviceId, bool connected) -> json;

auto setFilterPosition(const std::string &deviceId,
                       const json &requestBody) -> json;

auto setFilterByName(const std::string &deviceId,
                     const json &requestBody) -> json;

auto getFilterWheelCapabilities(const std::string &deviceId) -> json;

auto configureFilterNames(const std::string &deviceId,
                          const json &requestBody) -> json;

auto getFilterOffsets(const std::string &deviceId) -> json;

auto setFilterOffsets(const std::string &deviceId,
                      const json &requestBody) -> json;

auto haltFilterWheel(const std::string &deviceId) -> json;

auto calibrateFilterWheel(const std::string &deviceId) -> json;

}  // namespace lithium::middleware

#endif  // LITHIUM_SERVER_MIDDLEWARE_FILTERWHEEL_HPP
