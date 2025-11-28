#ifndef LITHIUM_SERVER_MIDDLEWARE_DOME_HPP
#define LITHIUM_SERVER_MIDDLEWARE_DOME_HPP

#include <string>
#include "atom/type/json.hpp"

namespace lithium::middleware {

using json = nlohmann::json;

auto listDomes() -> json;
auto getDomeStatus(const std::string &deviceId) -> json;
auto connectDome(const std::string &deviceId, bool connected) -> json;
auto slewDome(const std::string &deviceId, double azimuth) -> json;
auto shutterControl(const std::string &deviceId, bool open) -> json;
auto parkDome(const std::string &deviceId) -> json;
auto unparkDome(const std::string &deviceId) -> json;
auto homeDome(const std::string &deviceId) -> json;
auto stopDome(const std::string &deviceId) -> json;
auto getDomeCapabilities(const std::string &deviceId) -> json;

}  // namespace lithium::middleware

#endif  // LITHIUM_SERVER_MIDDLEWARE_DOME_HPP
