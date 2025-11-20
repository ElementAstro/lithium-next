#ifndef LITHIUM_SERVER_MIDDLEWARE_FOCUSER_HPP
#define LITHIUM_SERVER_MIDDLEWARE_FOCUSER_HPP

#include <string>

#include "atom/type/json.hpp"

namespace lithium::middleware {

using json = nlohmann::json;

auto listFocusers() -> json;

auto getFocuserStatus(const std::string &deviceId) -> json;

auto connectFocuser(const std::string &deviceId, bool connected) -> json;

auto moveFocuser(const std::string &deviceId, const json &moveRequest) -> json;

auto updateFocuserSettings(const std::string &deviceId,
                           const json &settings) -> json;

auto haltFocuser(const std::string &deviceId) -> json;

auto getFocuserCapabilities(const std::string &deviceId) -> json;

auto startAutofocus(const std::string &deviceId,
                    const json &autofocusRequest) -> json;

auto getAutofocusStatus(const std::string &deviceId,
                        const std::string &autofocusId) -> json;

}  // namespace lithium::middleware

#endif  // LITHIUM_SERVER_MIDDLEWARE_FOCUSER_HPP
