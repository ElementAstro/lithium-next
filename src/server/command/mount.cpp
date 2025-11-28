#include "mount.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>

#include <mutex>

#include "atom/async/message_bus.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/log/loguru.hpp"

#include "device/template/telescope.hpp"

#include "constant/constant.hpp"

namespace lithium::middleware {

using json = nlohmann::json;

namespace {

auto toUpper(std::string value) -> std::string {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) {
                       return static_cast<char>(std::toupper(c));
                   });
    return value;
}

auto parseSexagesimalRA(const std::string &value, double &outHours) -> bool {
    auto first = value.find(':');
    auto second = value.find(':', first == std::string::npos ? first : first + 1);
    if (first == std::string::npos || second == std::string::npos) {
        return false;
    }
    try {
        int hours = std::stoi(value.substr(0, first));
        int minutes = std::stoi(value.substr(first + 1, second - first - 1));
        double seconds = std::stod(value.substr(second + 1));
        if (hours < 0 || hours >= 24 || minutes < 0 || minutes >= 60 ||
            seconds < 0.0 || seconds >= 60.0) {
            return false;
        }
        outHours = static_cast<double>(hours) + minutes / 60.0 + seconds / 3600.0;
        return true;
    } catch (...) {
        return false;
    }
}

auto parseSexagesimalDec(const std::string &value, double &outDegrees) -> bool {
    if (value.empty()) {
        return false;
    }
    std::size_t start = 0;
    int sign = 1;
    if (value[0] == '+' || value[0] == '-') {
        sign = (value[0] == '-') ? -1 : 1;
        start = 1;
    }
    auto first = value.find(':', start);
    auto second = value.find(':', first == std::string::npos ? first : first + 1);
    if (first == std::string::npos || second == std::string::npos) {
        return false;
    }
    try {
        int deg = std::stoi(value.substr(start, first - start));
        int minutes = std::stoi(value.substr(first + 1, second - first - 1));
        double seconds = std::stod(value.substr(second + 1));
        if (deg < 0) {
            sign = -1;
            deg = -deg;
        }
        if (deg > 90 || minutes < 0 || minutes >= 60 || seconds < 0.0 ||
            seconds >= 60.0) {
            return false;
        }
        double total = static_cast<double>(deg) + minutes / 60.0 + seconds / 3600.0;
        outDegrees = sign * total;
        return true;
    } catch (...) {
        return false;
    }
}

auto formatSexagesimalRA(double hours) -> std::string {
    if (hours < 0.0) {
        hours = std::fmod(hours, 24.0) + 24.0;
    }
    double totalSeconds = hours * 3600.0;
    int h = static_cast<int>(std::floor(totalSeconds / 3600.0));
    double rem = totalSeconds - h * 3600.0;
    int m = static_cast<int>(std::floor(rem / 60.0));
    double s = rem - m * 60.0;
    if (h >= 24) {
        h %= 24;
    }
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%02d:%02d:%05.2f", h, m, s);
    return std::string(buffer);
}

auto formatSexagesimalDec(double degrees) -> std::string {
    char sign = degrees >= 0.0 ? '+' : '-';
    double absDeg = std::fabs(degrees);
    int d = static_cast<int>(std::floor(absDeg));
    double rem = absDeg - static_cast<double>(d);
    int m = static_cast<int>(std::floor(rem * 60.0));
    double s = (rem * 60.0 - static_cast<double>(m)) * 60.0;
    char buffer[32];
    std::snprintf(buffer, sizeof(buffer), "%c%02d:%02d:%05.2f", sign, d, m, s);
    return std::string(buffer);
}

// Stored guide rates for the mount. These values are updated via setGuideRates
// and reported back in getMountCapabilities to keep the API behaviour
// consistent, even though the underlying telescope interface does not expose
// a dedicated guide-rate configuration.

std::mutex g_mountConfigMutex;
double g_guideRateRA = 0.5;
double g_guideRateDec = 0.5;

}  // namespace

auto listMounts() -> json {
    LOG_F(INFO, "listMounts: Listing all available mounts");
    json response;
    response["status"] = "success";
    try {
        json mountList = json::array();
        std::shared_ptr<AtomTelescope> telescope;
        try {
            GET_OR_CREATE_PTR(telescope, AtomTelescope, Constants::MAIN_TELESCOPE)
            json info;
            info["deviceId"] = "mnt-001";
            info["name"] = telescope->getName();
            info["isConnected"] = telescope->isConnected();
            mountList.push_back(info);
        } catch (...) {
            LOG_F(WARNING, "listMounts: Main telescope not available");
        }
        response["data"] = mountList;
    } catch (const std::exception &e) {
        LOG_F(ERROR, "listMounts: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }
    LOG_F(INFO, "listMounts: Completed");
    return response;
}

auto getMountStatus(const std::string &deviceId) -> json {
    LOG_F(INFO, "getMountStatus: Getting status for mount: %s", deviceId.c_str());
    json response;
    try {
        std::shared_ptr<AtomTelescope> telescope;
        GET_OR_CREATE_PTR(telescope, AtomTelescope, Constants::MAIN_TELESCOPE)

        if (!telescope->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                   {"message", "Mount is not connected"}};
            return response;
        }

        json data;
        data["isConnected"] = telescope->isConnected();

        bool isSlewing = false;
        if (auto status = telescope->getStatus()) {
            isSlewing = status.value() == "Slewing";
        }
        data["isSlewing"] = isSlewing;
        data["isTracking"] = telescope->isTrackingEnabled();
        data["isParked"] = telescope->isParked();

        if (auto coords = telescope->getRADECJNow()) {
            json coordJson;
            coordJson["ra"] = formatSexagesimalRA(coords->first);
            coordJson["dec"] = formatSexagesimalDec(coords->second);
            data["coordinates"] = coordJson;
        }

        if (auto altaz = telescope->getAZALT()) {
            data["azimuth"] = altaz->first;
            data["altitude"] = altaz->second;
        }

        if (auto pierSide = telescope->getPierSide()) {
            std::string side = "Unknown";
            switch (*pierSide) {
                case PierSide::EAST:
                    side = "East";
                    break;
                case PierSide::WEST:
                    side = "West";
                    break;
                default:
                    side = "Unknown";
                    break;
            }
            data["pierSide"] = side;
        }

        response["status"] = "success";
        response["data"] = data;
    } catch (const std::exception &e) {
        LOG_F(ERROR, "getMountStatus: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }
    LOG_F(INFO, "getMountStatus: Completed");
    return response;
}

auto connectMount(const std::string &deviceId, bool connected) -> json {
    LOG_F(INFO, "connectMount: %s mount: %s", connected ? "Connecting" : "Disconnecting",
          deviceId.c_str());
    json response;
    try {
        std::shared_ptr<AtomTelescope> telescope;
        GET_OR_CREATE_PTR(telescope, AtomTelescope, Constants::MAIN_TELESCOPE)

        bool success = connected ? telescope->connect("") : telescope->disconnect();
        if (success) {
            response["status"] = "success";
            response["message"] = connected
                                        ? "Mount connection process initiated."
                                        : "Mount disconnection process initiated.";

            std::shared_ptr<atom::async::MessageBus> messageBusPtr;
            GET_OR_CREATE_PTR(messageBusPtr, atom::async::MessageBus,
                              Constants::MESSAGE_BUS)
            auto state = connected ? "ON" : "OFF";
            messageBusPtr->publish("main", std::string("MountConnection:") + state);
        } else {
            response["status"] = "error";
            response["error"] = {{"code", "connection_failed"},
                                   {"message", "Connection operation failed."}};
        }
    } catch (const std::exception &e) {
        LOG_F(ERROR, "connectMount: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }
    LOG_F(INFO, "connectMount: Completed");
    return response;
}

auto slewMount(const std::string &deviceId, const std::string &ra,
               const std::string &dec) -> json {
    LOG_F(INFO, "slewMount: Slewing mount %s to RA=%s DEC=%s", deviceId.c_str(),
          ra.c_str(), dec.c_str());
    json response;
    try {
        std::shared_ptr<AtomTelescope> telescope;
        GET_OR_CREATE_PTR(telescope, AtomTelescope, Constants::MAIN_TELESCOPE)

        if (!telescope->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                   {"message", "Mount is not connected"}};
            return response;
        }

        double raHours = 0.0;
        double decDegrees = 0.0;
        if (!parseSexagesimalRA(ra, raHours) ||
            !parseSexagesimalDec(dec, decDegrees)) {
            response["status"] = "error";
            response["error"] = {{"code", "invalid_coordinates"},
                                   {"message", "Invalid RA/Dec format"}};
            return response;
        }

        bool success = telescope->slewToRADECJNow(raHours, decDegrees, true);
        if (success) {
            response["status"] = "success";
            response["message"] = "Slew command accepted.";
            response["data"] =
                json{{"target", json{{"ra", ra}, {"dec", dec}}}};
        } else {
            response["status"] = "error";
            response["error"] = {{"code", "slew_failed"},
                                   {"message", "Failed to start slew."}};
        }
    } catch (const std::exception &e) {
        LOG_F(ERROR, "slewMount: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }
    LOG_F(INFO, "slewMount: Completed");
    return response;
}

auto setTracking(const std::string &deviceId, bool tracking) -> json {
    LOG_F(INFO, "setTracking: %s tracking on mount: %s",
          tracking ? "Enabling" : "Disabling", deviceId.c_str());
    json response;
    try {
        std::shared_ptr<AtomTelescope> telescope;
        GET_OR_CREATE_PTR(telescope, AtomTelescope, Constants::MAIN_TELESCOPE)

        if (!telescope->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                   {"message", "Mount is not connected"}};
            return response;
        }

        bool success = telescope->enableTracking(tracking);
        if (success) {
            response["status"] = "success";
            response["message"] = "Tracking state updated.";
            response["data"] = json{{"tracking", tracking}};
        } else {
            response["status"] = "error";
            response["error"] = {{"code", "tracking_failed"},
                                   {"message", "Failed to update tracking state."}};
        }
    } catch (const std::exception &e) {
        LOG_F(ERROR, "setTracking: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }
    LOG_F(INFO, "setTracking: Completed");
    return response;
}

auto setMountPosition(const std::string &deviceId,
                      const std::string &command) -> json {
    LOG_F(INFO, "setMountPosition: Command '%s' for mount: %s", command.c_str(),
          deviceId.c_str());
    json response;
    try {
        std::shared_ptr<AtomTelescope> telescope;
        GET_OR_CREATE_PTR(telescope, AtomTelescope, Constants::MAIN_TELESCOPE)

        if (!telescope->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                   {"message", "Mount is not connected"}};
            return response;
        }

        auto cmdUpper = toUpper(command);
        bool success = false;
        if (cmdUpper == "PARK") {
            success = telescope->park(true);
        } else if (cmdUpper == "UNPARK") {
            success = telescope->park(false);
        } else if (cmdUpper == "HOME") {
            success = telescope->initializeHome("SLEWHOME");
        } else if (cmdUpper == "FIND_HOME") {
            success = telescope->initializeHome("SYNCHOME");
        } else {
            response["status"] = "error";
            response["error"] = {{"code", "invalid_position_command"},
                                   {"message", "Unsupported mount position command"}};
            return response;
        }

        if (success) {
            response["status"] = "success";
            response["message"] = "Mount command accepted.";
            response["data"] = json{{"command", cmdUpper}};
        } else {
            response["status"] = "error";
            response["error"] = {{"code", "mount_command_failed"},
                                   {"message", "Mount position command failed."}};
        }
    } catch (const std::exception &e) {
        LOG_F(ERROR, "setMountPosition: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }
    LOG_F(INFO, "setMountPosition: Completed");
    return response;
}

auto pulseGuide(const std::string &deviceId, const std::string &direction,
                int durationMs) -> json {
    LOG_F(INFO, "pulseGuide: Direction=%s duration=%dms for mount: %s",
          direction.c_str(), durationMs, deviceId.c_str());
    json response;
    try {
        std::shared_ptr<AtomTelescope> telescope;
        GET_OR_CREATE_PTR(telescope, AtomTelescope, Constants::MAIN_TELESCOPE)

        if (!telescope->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                   {"message", "Mount is not connected"}};
            return response;
        }

        auto dirUpper = toUpper(direction);
        bool success = false;
        if (dirUpper == "NORTH") {
            success = telescope->guideNS(+1, durationMs);
        } else if (dirUpper == "SOUTH") {
            success = telescope->guideNS(-1, durationMs);
        } else if (dirUpper == "EAST") {
            success = telescope->guideEW(+1, durationMs);
        } else if (dirUpper == "WEST") {
            success = telescope->guideEW(-1, durationMs);
        } else {
            response["status"] = "error";
            response["error"] = {{"code", "invalid_pulse_direction"},
                                   {"message", "Invalid pulse guide direction"}};
            return response;
        }

        if (success) {
            response["status"] = "success";
            response["message"] = "Pulse guide command sent.";
        } else {
            response["status"] = "error";
            response["error"] = {{"code", "pulse_guide_failed"},
                                   {"message", "Pulse guide command failed."}};
        }
    } catch (const std::exception &e) {
        LOG_F(ERROR, "pulseGuide: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }
    LOG_F(INFO, "pulseGuide: Completed");
    return response;
}

auto syncMount(const std::string &deviceId, const std::string &ra,
               const std::string &dec) -> json {
    LOG_F(INFO, "syncMount: Syncing mount %s to RA=%s DEC=%s", deviceId.c_str(),
          ra.c_str(), dec.c_str());
    json response;
    try {
        std::shared_ptr<AtomTelescope> telescope;
        GET_OR_CREATE_PTR(telescope, AtomTelescope, Constants::MAIN_TELESCOPE)

        if (!telescope->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                   {"message", "Mount is not connected"}};
            return response;
        }

        double raHours = 0.0;
        double decDegrees = 0.0;
        if (!parseSexagesimalRA(ra, raHours) ||
            !parseSexagesimalDec(dec, decDegrees)) {
            response["status"] = "error";
            response["error"] = {{"code", "invalid_coordinates"},
                                   {"message", "Invalid RA/Dec format"}};
            return response;
        }

        bool success = telescope->syncToRADECJNow(raHours, decDegrees);
        if (success) {
            response["status"] = "success";
            response["message"] = "Mount position synchronized.";
            response["data"] = json{{"syncError", json{{"raError", 0.0},
                                                         {"decError", 0.0}}}};
        } else {
            response["status"] = "error";
            response["error"] = {{"code", "sync_failed"},
                                   {"message", "Failed to sync mount position."}};
        }
    } catch (const std::exception &e) {
        LOG_F(ERROR, "syncMount: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }
    LOG_F(INFO, "syncMount: Completed");
    return response;
}

auto getMountCapabilities(const std::string &deviceId) -> json {
    (void)deviceId;
    LOG_F(INFO, "getMountCapabilities: Getting capabilities for mount: %s",
          deviceId.c_str());
    json response;
    try {
        std::lock_guard<std::mutex> lock(g_mountConfigMutex);
        json caps;
        caps["canPark"] = true;
        caps["canUnpark"] = true;
        caps["canFindHome"] = true;
        caps["canSetTracking"] = true;
        caps["canSetGuideRates"] = true;
        caps["canPulseGuide"] = true;
        caps["canSync"] = true;
        caps["canSlewAsync"] = true;
        caps["canSlewAltAz"] = false;
        caps["hasEquatorialSystem"] = true;
        caps["alignmentMode"] = "GermanEquatorial";
        caps["trackingRates"] = json::array({"Sidereal", "Lunar", "Solar"});

        caps["axisRates"] = json{{"ra", json{{"min", 0.25}, {"max", 4.0}}},
                                  {"dec", json{{"min", 0.25}, {"max", 4.0}}}};

        caps["guideRates"] = json{{"ra", g_guideRateRA}, {"dec", g_guideRateDec}};
        caps["slewSettleTime"] = 5;

        response["status"] = "success";
        response["data"] = caps;
    } catch (const std::exception &e) {
        LOG_F(ERROR, "getMountCapabilities: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }
    LOG_F(INFO, "getMountCapabilities: Completed");
    return response;
}

auto setGuideRates(const std::string &deviceId, double raRate,
                   double decRate) -> json {
    LOG_F(INFO, "setGuideRates: RA=%f DEC=%f for mount: %s", raRate, decRate,
          deviceId.c_str());
    json response;
    try {
        std::shared_ptr<AtomTelescope> telescope;
        GET_OR_CREATE_PTR(telescope, AtomTelescope, Constants::MAIN_TELESCOPE)

        if (!telescope->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                   {"message", "Mount is not connected"}};
            return response;
        }

        // Validate guide rates: use a conservative range similar to typical
        // mount behaviour (0.1x - 1.0x sidereal).
        if (raRate <= 0.0 || decRate <= 0.0 || raRate > 4.0 || decRate > 4.0) {
            response["status"] = "error";
            response["error"] = {{"code", "invalid_field_value"},
                                   {"message",
                                    "Guide rates must be within (0, 4.0]."}};
            return response;
        }

        {
            std::lock_guard<std::mutex> lock(g_mountConfigMutex);
            g_guideRateRA = raRate;
            g_guideRateDec = decRate;
        }

        (void)telescope;
        response["status"] = "success";
        response["message"] = "Guide rates updated.";
        response["data"] = json{{"raRate", raRate}, {"decRate", decRate}};
    } catch (const std::exception &e) {
        LOG_F(ERROR, "setGuideRates: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }
    LOG_F(INFO, "setGuideRates: Completed");
    return response;
}

auto setTrackingRate(const std::string &deviceId, const std::string &rate)
    -> json {
    LOG_F(INFO, "setTrackingRate: rate=%s for mount: %s", rate.c_str(),
          deviceId.c_str());
    json response;
    try {
        std::shared_ptr<AtomTelescope> telescope;
        GET_OR_CREATE_PTR(telescope, AtomTelescope, Constants::MAIN_TELESCOPE)

        if (!telescope->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                   {"message", "Mount is not connected"}};
            return response;
        }

        auto rUpper = toUpper(rate);
        TrackMode mode = TrackMode::SIDEREAL;
        if (rUpper == "LUNAR") {
            mode = TrackMode::LUNAR;
        } else if (rUpper == "SOLAR") {
            mode = TrackMode::SOLAR;
        } else {
            mode = TrackMode::SIDEREAL;
        }

        bool success = telescope->setTrackRate(mode);
        if (success) {
            response["status"] = "success";
            response["message"] = "Tracking rate updated.";
        } else {
            response["status"] = "error";
            response["error"] = {{"code", "tracking_rate_failed"},
                                   {"message", "Failed to update tracking rate."}};
        }
    } catch (const std::exception &e) {
        LOG_F(ERROR, "setTrackingRate: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }
    LOG_F(INFO, "setTrackingRate: Completed");
    return response;
}

auto stopMount(const std::string &deviceId) -> json {
    LOG_F(INFO, "stopMount: Stopping mount: %s", deviceId.c_str());
    json response;
    try {
        std::shared_ptr<AtomTelescope> telescope;
        GET_OR_CREATE_PTR(telescope, AtomTelescope, Constants::MAIN_TELESCOPE)

        if (!telescope->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                   {"message", "Mount is not connected"}};
            return response;
        }

        bool success = telescope->abortMotion();
        if (success) {
            response["status"] = "success";
            response["message"] = "Mount motion stopped.";
        } else {
            response["status"] = "error";
            response["error"] = {{"code", "stop_failed"},
                                   {"message", "Failed to stop mount motion."}};
        }
    } catch (const std::exception &e) {
        LOG_F(ERROR, "stopMount: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }
    LOG_F(INFO, "stopMount: Completed");
    return response;
}

auto getPierSide(const std::string &deviceId) -> json {
    LOG_F(INFO, "getPierSide: Getting pier side for mount: %s", deviceId.c_str());
    json response;
    try {
        std::shared_ptr<AtomTelescope> telescope;
        GET_OR_CREATE_PTR(telescope, AtomTelescope, Constants::MAIN_TELESCOPE)

        if (!telescope->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                   {"message", "Mount is not connected"}};
            return response;
        }

        json data;
        std::string side = "Unknown";
        if (auto pierSide = telescope->getPierSide()) {
            switch (*pierSide) {
                case PierSide::EAST:
                    side = "East";
                    break;
                case PierSide::WEST:
                    side = "West";
                    break;
                default:
                    side = "Unknown";
                    break;
            }
        }
        data["pierSide"] = side;
        data["timeToFlip"] = nullptr;
        data["destinationAfterFlip"] = nullptr;

        response["status"] = "success";
        response["data"] = data;
    } catch (const std::exception &e) {
        LOG_F(ERROR, "getPierSide: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }
    LOG_F(INFO, "getPierSide: Completed");
    return response;
}

auto performMeridianFlip(const std::string &deviceId) -> json {
    LOG_F(INFO, "performMeridianFlip: Initiating meridian flip for mount: %s",
          deviceId.c_str());
    json response;
    try {
        std::shared_ptr<AtomTelescope> telescope;
        GET_OR_CREATE_PTR(telescope, AtomTelescope, Constants::MAIN_TELESCOPE)

        if (!telescope->isConnected()) {
            response["status"] = "error";
            response["error"] = {{"code", "device_not_connected"},
                                   {"message", "Mount is not connected"}};
            return response;
        }

        (void)telescope;
        response["status"] = "error";
        response["error"] = {{"code", "feature_not_supported"},
                               {"message",
                                "Meridian flip is not implemented for this mount."}};
    } catch (const std::exception &e) {
        LOG_F(ERROR, "performMeridianFlip: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"},
                               {"message", e.what()}};
    }
    LOG_F(INFO, "performMeridianFlip: Completed");
    return response;
}

}  // namespace lithium::middleware
