#ifndef LITHIUM_SERVER_MIDDLEWARE_MOUNT_HPP
#define LITHIUM_SERVER_MIDDLEWARE_MOUNT_HPP

#include <string>

#include "atom/type/json.hpp"

namespace lithium::middleware {

using json = nlohmann::json;

auto listMounts() -> json;
auto getMountStatus(const std::string &deviceId) -> json;
auto connectMount(const std::string &deviceId, bool connected) -> json;
auto slewMount(const std::string &deviceId, const std::string &ra,
               const std::string &dec) -> json;
auto setTracking(const std::string &deviceId, bool tracking) -> json;
auto setMountPosition(const std::string &deviceId,
                      const std::string &command) -> json;
auto pulseGuide(const std::string &deviceId, const std::string &direction,
                int durationMs) -> json;
auto syncMount(const std::string &deviceId, const std::string &ra,
               const std::string &dec) -> json;
auto getMountCapabilities(const std::string &deviceId) -> json;
auto setGuideRates(const std::string &deviceId, double raRate,
                   double decRate) -> json;
auto setTrackingRate(const std::string &deviceId, const std::string &rate)
    -> json;
auto stopMount(const std::string &deviceId) -> json;
auto getPierSide(const std::string &deviceId) -> json;
auto performMeridianFlip(const std::string &deviceId) -> json;

}  // namespace lithium::middleware

#endif  // LITHIUM_SERVER_MIDDLEWARE_MOUNT_HPP
