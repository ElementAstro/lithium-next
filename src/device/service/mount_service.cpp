/*
 * mount_service.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "mount_service.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <memory>
#include <mutex>
#include <string>

#include "atom/async/message_bus.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"

#include "constant/constant.hpp"
#include "device/template/telescope.hpp"

namespace lithium::device {

using json = nlohmann::json;

namespace {

auto toUpper(std::string value) -> std::string {
    std::transform(
        value.begin(), value.end(), value.begin(),
        [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return value;
}

auto parseSexagesimalRA(const std::string& value, double& outHours) -> bool {
    auto first = value.find(':');
    auto second =
        value.find(':', first == std::string::npos ? first : first + 1);
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
        outHours =
            static_cast<double>(hours) + minutes / 60.0 + seconds / 3600.0;
        return true;
    } catch (...) {
        return false;
    }
}

auto parseSexagesimalDec(const std::string& value, double& outDegrees) -> bool {
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
    auto second =
        value.find(':', first == std::string::npos ? first : first + 1);
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
        double total =
            static_cast<double>(deg) + minutes / 60.0 + seconds / 3600.0;
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

}  // namespace

class MountService::Impl {
public:
    std::mutex configMutex;
    double guideRateRA = 0.5;
    double guideRateDec = 0.5;

    // Site location (cached)
    double latitude = 0.0;
    double longitude = 0.0;
    double elevation = 0.0;
};

MountService::MountService()
    : TypedDeviceService("MountService", "Mount", Constants::MAIN_TELESCOPE),
      impl_(std::make_unique<Impl>()) {}

MountService::~MountService() = default;

auto MountService::list() -> json {
    LOG_INFO("MountService::list: Listing all available mounts");
    json response;
    response["status"] = "success";
    try {
        json mountList = json::array();
        std::shared_ptr<AtomTelescope> telescope;
        try {
            GET_OR_CREATE_PTR(telescope, AtomTelescope,
                              Constants::MAIN_TELESCOPE)
            json info;
            info["deviceId"] = "mnt-001";
            info["name"] = telescope->getName();
            info["isConnected"] = telescope->isConnected();
            mountList.push_back(info);
        } catch (...) {
            LOG_WARN("MountService::list: Main telescope not available");
        }
        response["data"] = mountList;
    } catch (const std::exception& e) {
        LOG_ERROR("MountService::list: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }
    LOG_INFO("MountService::list: Completed");
    return response;
}

auto MountService::getStatus(const std::string& deviceId) -> json {
    LOG_INFO("MountService::getStatus: Getting status for mount: %s",
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
    } catch (const std::exception& e) {
        LOG_ERROR("MountService::getStatus: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }
    LOG_INFO("MountService::getStatus: Completed");
    return response;
}

auto MountService::connect(const std::string& deviceId, bool connected)
    -> json {
    LOG_INFO("MountService::connect: %s mount: %s",
             connected ? "Connecting" : "Disconnecting", deviceId.c_str());
    json response;
    try {
        std::shared_ptr<AtomTelescope> telescope;
        GET_OR_CREATE_PTR(telescope, AtomTelescope, Constants::MAIN_TELESCOPE)

        bool success =
            connected ? telescope->connect("") : telescope->disconnect();
        if (success) {
            response["status"] = "success";
            response["message"] =
                connected ? "Mount connection process initiated."
                          : "Mount disconnection process initiated.";

            std::shared_ptr<atom::async::MessageBus> messageBusPtr;
            GET_OR_CREATE_PTR(messageBusPtr, atom::async::MessageBus,
                              Constants::MESSAGE_BUS)
            auto state = connected ? "ON" : "OFF";
            messageBusPtr->publish("main",
                                   std::string("MountConnection:") + state);
        } else {
            response["status"] = "error";
            response["error"] = {{"code", "connection_failed"},
                                 {"message", "Connection operation failed."}};
        }
    } catch (const std::exception& e) {
        LOG_ERROR("MountService::connect: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }
    LOG_INFO("MountService::connect: Completed");
    return response;
}

auto MountService::slew(const std::string& deviceId, const std::string& ra,
                        const std::string& dec) -> json {
    LOG_INFO("MountService::slew: Slewing mount %s to RA=%s DEC=%s",
             deviceId.c_str(), ra.c_str(), dec.c_str());
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
            response["data"] = json{{"target", json{{"ra", ra}, {"dec", dec}}}};
        } else {
            response["status"] = "error";
            response["error"] = {{"code", "slew_failed"},
                                 {"message", "Failed to start slew."}};
        }
    } catch (const std::exception& e) {
        LOG_ERROR("MountService::slew: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }
    LOG_INFO("MountService::slew: Completed");
    return response;
}

auto MountService::stop(const std::string& deviceId) -> json {
    LOG_INFO("MountService::stop: Stopping mount: %s", deviceId.c_str());
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
    } catch (const std::exception& e) {
        LOG_ERROR("MountService::stop: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }
    LOG_INFO("MountService::stop: Completed");
    return response;
}

auto MountService::setTracking(const std::string& deviceId, bool tracking)
    -> json {
    LOG_INFO("MountService::setTracking: %s tracking on mount: %s",
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
            response["error"] = {
                {"code", "tracking_failed"},
                {"message", "Failed to update tracking state."}};
        }
    } catch (const std::exception& e) {
        LOG_ERROR("MountService::setTracking: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }
    LOG_INFO("MountService::setTracking: Completed");
    return response;
}

auto MountService::setPosition(const std::string& deviceId,
                               const std::string& command) -> json {
    LOG_INFO("MountService::setPosition: Command '%s' for mount: %s",
             command.c_str(), deviceId.c_str());
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
            response["error"] = {
                {"code", "invalid_position_command"},
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
    } catch (const std::exception& e) {
        LOG_ERROR("MountService::setPosition: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }
    LOG_INFO("MountService::setPosition: Completed");
    return response;
}

auto MountService::pulseGuide(const std::string& deviceId,
                              const std::string& direction, int durationMs)
    -> json {
    LOG_INFO(
        "MountService::pulseGuide: Direction=%s duration=%dms for mount: %s",
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
    } catch (const std::exception& e) {
        LOG_ERROR("MountService::pulseGuide: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }
    LOG_INFO("MountService::pulseGuide: Completed");
    return response;
}

auto MountService::sync(const std::string& deviceId, const std::string& ra,
                        const std::string& dec) -> json {
    LOG_INFO("MountService::sync: Syncing mount %s to RA=%s DEC=%s",
             deviceId.c_str(), ra.c_str(), dec.c_str());
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
            response["data"] =
                json{{"syncError", json{{"raError", 0.0}, {"decError", 0.0}}}};
        } else {
            response["status"] = "error";
            response["error"] = {{"code", "sync_failed"},
                                 {"message", "Failed to sync mount position."}};
        }
    } catch (const std::exception& e) {
        LOG_ERROR("MountService::sync: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }
    LOG_INFO("MountService::sync: Completed");
    return response;
}

auto MountService::getCapabilities(const std::string& deviceId) -> json {
    (void)deviceId;
    LOG_INFO(
        "MountService::getCapabilities: Getting capabilities for mount: %s",
        deviceId.c_str());
    json response;
    try {
        std::lock_guard<std::mutex> lock(impl_->configMutex);
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

        caps["guideRates"] =
            json{{"ra", impl_->guideRateRA}, {"dec", impl_->guideRateDec}};
        caps["slewSettleTime"] = 5;

        response["status"] = "success";
        response["data"] = caps;
    } catch (const std::exception& e) {
        LOG_ERROR("MountService::getCapabilities: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }
    LOG_INFO("MountService::getCapabilities: Completed");
    return response;
}

auto MountService::setGuideRates(const std::string& deviceId, double raRate,
                                 double decRate) -> json {
    LOG_INFO("MountService::setGuideRates: RA=%f DEC=%f for mount: %s", raRate,
             decRate, deviceId.c_str());
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

        if (raRate <= 0.0 || decRate <= 0.0 || raRate > 4.0 || decRate > 4.0) {
            response["status"] = "error";
            response["error"] = {
                {"code", "invalid_field_value"},
                {"message", "Guide rates must be within (0, 4.0]."}};
            return response;
        }

        {
            std::lock_guard<std::mutex> lock(impl_->configMutex);
            impl_->guideRateRA = raRate;
            impl_->guideRateDec = decRate;
        }

        response["status"] = "success";
        response["message"] = "Guide rates updated.";
        response["data"] = json{{"raRate", raRate}, {"decRate", decRate}};
    } catch (const std::exception& e) {
        LOG_ERROR("MountService::setGuideRates: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }
    LOG_INFO("MountService::setGuideRates: Completed");
    return response;
}

auto MountService::setTrackingRate(const std::string& deviceId,
                                   const std::string& rate) -> json {
    LOG_INFO("MountService::setTrackingRate: rate=%s for mount: %s",
             rate.c_str(), deviceId.c_str());
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
            response["error"] = {
                {"code", "tracking_rate_failed"},
                {"message", "Failed to update tracking rate."}};
        }
    } catch (const std::exception& e) {
        LOG_ERROR("MountService::setTrackingRate: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }
    LOG_INFO("MountService::setTrackingRate: Completed");
    return response;
}

auto MountService::getPierSide(const std::string& deviceId) -> json {
    LOG_INFO("MountService::getPierSide: Getting pier side for mount: %s",
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
    } catch (const std::exception& e) {
        LOG_ERROR("MountService::getPierSide: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }
    LOG_INFO("MountService::getPierSide: Completed");
    return response;
}

auto MountService::performMeridianFlip(const std::string& deviceId) -> json {
    LOG_INFO(
        "MountService::performMeridianFlip: Initiating meridian flip for "
        "mount: %s",
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

        response["status"] = "error";
        response["error"] = {
            {"code", "feature_not_supported"},
            {"message", "Meridian flip is not implemented for this mount."}};
    } catch (const std::exception& e) {
        LOG_ERROR("MountService::performMeridianFlip: Exception: %s", e.what());
        response["status"] = "error";
        response["error"] = {{"code", "internal_error"}, {"message", e.what()}};
    }
    LOG_INFO("MountService::performMeridianFlip: Completed");
    return response;
}

// ========== INDI-specific operations ==========

auto MountService::getINDIProperties(const std::string& deviceId) -> json {
    return withConnectedDevice(
        deviceId, "getINDIProperties", [this](auto telescope) -> json {
            json data;
            data["driverName"] = "INDI Telescope";
            data["driverVersion"] = "1.0";

            json properties = json::object();

            // Tracking state
            properties["TELESCOPE_TRACK_STATE"] = {
                {"value", telescope->isTrackingEnabled()}, {"type", "switch"}};

            // Park state
            properties["TELESCOPE_PARK"] = {{"value", telescope->isParked()},
                                            {"type", "switch"}};

            // Pier side
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
                        break;
                }
                properties["TELESCOPE_PIER_SIDE"] = {{"value", side},
                                                     {"type", "text"}};
            }

            // Coordinates
            if (auto coords = telescope->getRADECJNow()) {
                properties["EQUATORIAL_EOD_COORD"] = {{"RA", coords->first},
                                                      {"DEC", coords->second},
                                                      {"type", "number"}};
            }

            data["properties"] = properties;
            return makeSuccessResponse(data);
        });
}

auto MountService::setINDIProperty(const std::string& deviceId,
                                   const std::string& propertyName,
                                   const json& value) -> json {
    return withConnectedDevice(
        deviceId, "setINDIProperty", [&](auto telescope) -> json {
            bool success = false;

            if (propertyName == "TELESCOPE_TRACK_STATE" && value.is_boolean()) {
                success = telescope->enableTracking(value.get<bool>());
            } else if (propertyName == "TELESCOPE_PARK" && value.is_boolean()) {
                success = telescope->park(value.get<bool>());
            } else if (propertyName == "TELESCOPE_TRACK_RATE" &&
                       value.is_string()) {
                std::string rate = value.get<std::string>();
                TrackMode mode = TrackMode::SIDEREAL;
                if (rate == "LUNAR") {
                    mode = TrackMode::LUNAR;
                } else if (rate == "SOLAR") {
                    mode = TrackMode::SOLAR;
                }
                success = telescope->setTrackRate(mode);
            } else {
                return makeErrorResponse(
                    ErrorCode::INVALID_FIELD_VALUE,
                    "Unknown or invalid property: " + propertyName);
            }

            if (success) {
                return makeSuccessResponse("Property " + propertyName +
                                           " updated");
            }
            return makeErrorResponse(ErrorCode::OPERATION_FAILED,
                                     "Failed to set property " + propertyName);
        });
}

auto MountService::getTelescopeInfo(const std::string& deviceId) -> json {
    return withConnectedDevice(
        deviceId, "getTelescopeInfo", [](auto telescope) -> json {
            json data;

            if (auto info = telescope->getTelescopeInfo()) {
                auto [aperture, focalLength, guiderAperture,
                      guiderFocalLength] = *info;
                data["aperture"] = aperture;
                data["focalLength"] = focalLength;
                data["guiderAperture"] = guiderAperture;
                data["guiderFocalLength"] = guiderFocalLength;
                data["focalRatio"] =
                    (aperture > 0) ? focalLength / aperture : 0.0;
            } else {
                data["aperture"] = 0.0;
                data["focalLength"] = 0.0;
                data["guiderAperture"] = 0.0;
                data["guiderFocalLength"] = 0.0;
                data["focalRatio"] = 0.0;
            }

            return makeSuccessResponse(data);
        });
}

auto MountService::setTelescopeInfo(const std::string& deviceId,
                                    double aperture, double focalLength,
                                    double guiderAperture,
                                    double guiderFocalLength) -> json {
    return withConnectedDevice(
        deviceId, "setTelescopeInfo", [&](auto telescope) -> json {
            if (aperture <= 0 || focalLength <= 0) {
                return makeErrorResponse(
                    ErrorCode::INVALID_FIELD_VALUE,
                    "Aperture and focal length must be positive");
            }

            if (telescope->setTelescopeInfo(
                    aperture, focalLength, guiderAperture, guiderFocalLength)) {
                json data;
                data["aperture"] = aperture;
                data["focalLength"] = focalLength;
                data["guiderAperture"] = guiderAperture;
                data["guiderFocalLength"] = guiderFocalLength;
                return makeSuccessResponse(data, "Telescope info updated");
            }
            return makeErrorResponse(ErrorCode::OPERATION_FAILED,
                                     "Failed to set telescope info");
        });
}

auto MountService::getSiteLocation(const std::string& deviceId) -> json {
    return withConnectedDevice(
        deviceId, "getSiteLocation", [this](auto telescope) -> json {
            (void)telescope;
            json data;
            {
                std::lock_guard<std::mutex> lock(impl_->configMutex);
                data["latitude"] = impl_->latitude;
                data["longitude"] = impl_->longitude;
                data["elevation"] = impl_->elevation;
            }
            return makeSuccessResponse(data);
        });
}

auto MountService::setSiteLocation(const std::string& deviceId, double latitude,
                                   double longitude, double elevation) -> json {
    return withConnectedDevice(
        deviceId, "setSiteLocation", [&](auto telescope) -> json {
            (void)telescope;
            if (latitude < -90.0 || latitude > 90.0) {
                return makeErrorResponse(ErrorCode::INVALID_FIELD_VALUE,
                                         "Latitude must be between -90 and 90");
            }
            if (longitude < -180.0 || longitude > 180.0) {
                return makeErrorResponse(
                    ErrorCode::INVALID_FIELD_VALUE,
                    "Longitude must be between -180 and 180");
            }

            {
                std::lock_guard<std::mutex> lock(impl_->configMutex);
                impl_->latitude = latitude;
                impl_->longitude = longitude;
                impl_->elevation = elevation;
            }

            json data;
            data["latitude"] = latitude;
            data["longitude"] = longitude;
            data["elevation"] = elevation;
            return makeSuccessResponse(data, "Site location updated");
        });
}

auto MountService::getUTCOffset(const std::string& deviceId) -> json {
    return withConnectedDevice(
        deviceId, "getUTCOffset", [](auto telescope) -> json {
            (void)telescope;
            // Get system UTC offset
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            std::tm local_tm{};
            std::tm utc_tm{};

#ifdef _WIN32
            localtime_s(&local_tm, &time_t_now);
            gmtime_s(&utc_tm, &time_t_now);
#else
        localtime_r(&time_t_now, &local_tm);
        gmtime_r(&time_t_now, &utc_tm);
#endif

            int offset_hours = local_tm.tm_hour - utc_tm.tm_hour;
            if (offset_hours > 12)
                offset_hours -= 24;
            if (offset_hours < -12)
                offset_hours += 24;

            json data;
            data["utcOffset"] = offset_hours;
            data["timezone"] = "Local";
            return makeSuccessResponse(data);
        });
}

}  // namespace lithium::device
