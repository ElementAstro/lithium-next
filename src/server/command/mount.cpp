#include "mount.hpp"

#include "atom/log/spdlog_logger.hpp"

#include "atom/type/json.hpp"
#include "command.hpp"
#include "device/service/mount_service.hpp"
#include "response.hpp"

namespace lithium::app {

using command::CommandResponse;
using nlohmann::json;

// Global service instance
static lithium::device::MountService& getMountService() {
    static lithium::device::MountService instance;
    return instance;
}

void registerMount(std::shared_ptr<CommandDispatcher> dispatcher) {
    // Mount: get status
    dispatcher->registerCommand<json>("mount.status", [](json& payload) {
        LOG_INFO("Executing command: mount.status");

        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        try {
            auto result = getMountService().getStatus(deviceId);
            payload = result;
        } catch (const std::exception& e) {
            payload =
                CommandResponse::operationFailed("mount.status", e.what());
        }
    });

    // Mount: slew to RA/Dec (sexagesimal strings)
    dispatcher->registerCommand<json>("mount.slew", [](json& payload) {
        LOG_INFO("Executing command: mount.slew");

        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        if (!payload.contains("ra") || !payload["ra"].is_string() ||
            payload["ra"].get<std::string>().empty()) {
            payload = CommandResponse::missingParameter("ra");
            return;
        }
        if (!payload.contains("dec") || !payload["dec"].is_string() ||
            payload["dec"].get<std::string>().empty()) {
            payload = CommandResponse::missingParameter("dec");
            return;
        }

        std::string ra = payload["ra"].get<std::string>();
        std::string dec = payload["dec"].get<std::string>();

        try {
            auto result = getMountService().slew(deviceId, ra, dec);
            payload = result;
        } catch (const std::exception& e) {
            payload = CommandResponse::operationFailed("mount.slew", e.what());
        }
    });

    // Mount: stop motion
    dispatcher->registerCommand<json>("mount.stop", [](json& payload) {
        LOG_INFO("Executing command: mount.stop");

        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        try {
            auto result = getMountService().stop(deviceId);
            payload = result;
        } catch (const std::exception& e) {
            payload = CommandResponse::operationFailed("mount.stop", e.what());
        }
    });

    // Mount: set tracking on/off
    dispatcher->registerCommand<json>("mount.set_tracking", [](json& payload) {
        LOG_INFO("Executing command: mount.set_tracking");

        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        if (!payload.contains("tracking") ||
            !payload["tracking"].is_boolean()) {
            payload = CommandResponse::missingParameter("tracking");
            return;
        }
        bool tracking = payload["tracking"].get<bool>();

        try {
            auto result = getMountService().setTracking(deviceId, tracking);
            payload = result;
        } catch (const std::exception& e) {
            payload = CommandResponse::operationFailed("mount.set_tracking",
                                                       e.what());
        }
    });

    // Mount: list mounts
    dispatcher->registerCommand<json>("mount.list", [](json& payload) {
        LOG_INFO("Executing command: mount.list");

        try {
            auto result = getMountService().list();
            payload = result;
        } catch (const std::exception& e) {
            payload = CommandResponse::operationFailed("mount.list", e.what());
        }
    });

    // Mount: connect / disconnect
    dispatcher->registerCommand<json>("mount.connect", [](json& payload) {
        LOG_INFO("Executing command: mount.connect");

        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        if (!payload.contains("connected") ||
            !payload["connected"].is_boolean()) {
            payload = CommandResponse::missingParameter("connected");
            return;
        }
        bool connected = payload["connected"].get<bool>();

        try {
            auto result = getMountService().connect(deviceId, connected);
            payload = result;
        } catch (const std::exception& e) {
            payload =
                CommandResponse::operationFailed("mount.connect", e.what());
        }
    });

    // Mount: position commands (PARK / UNPARK / HOME / FIND_HOME)
    dispatcher->registerCommand<json>("mount.position", [](json& payload) {
        LOG_INFO("Executing command: mount.position");

        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        if (!payload.contains("command") || !payload["command"].is_string() ||
            payload["command"].get<std::string>().empty()) {
            payload = CommandResponse::missingParameter("command");
            return;
        }
        std::string command = payload["command"].get<std::string>();

        try {
            auto result = getMountService().setPosition(deviceId, command);
            payload = result;
        } catch (const std::exception& e) {
            payload =
                CommandResponse::operationFailed("mount.position", e.what());
        }
    });

    // Mount: pulse guide
    dispatcher->registerCommand<json>("mount.pulse_guide", [](json& payload) {
        LOG_INFO("Executing command: mount.pulse_guide");

        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        if (!payload.contains("direction") ||
            !payload["direction"].is_string() ||
            payload["direction"].get<std::string>().empty()) {
            payload = CommandResponse::missingParameter("direction");
            return;
        }
        if (!payload.contains("durationMs") ||
            !payload["durationMs"].is_number_integer()) {
            payload = CommandResponse::missingParameter("durationMs");
            return;
        }

        std::string direction = payload["direction"].get<std::string>();
        int durationMs = payload["durationMs"].get<int>();

        try {
            auto result =
                getMountService().pulseGuide(deviceId, direction, durationMs);
            payload = result;
        } catch (const std::exception& e) {
            payload =
                CommandResponse::operationFailed("mount.pulse_guide", e.what());
        }
    });

    // Mount: sync to RA/Dec
    dispatcher->registerCommand<json>("mount.sync", [](json& payload) {
        LOG_INFO("Executing command: mount.sync");

        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        if (!payload.contains("ra") || !payload["ra"].is_string() ||
            payload["ra"].get<std::string>().empty()) {
            payload = CommandResponse::missingParameter("ra");
            return;
        }
        if (!payload.contains("dec") || !payload["dec"].is_string() ||
            payload["dec"].get<std::string>().empty()) {
            payload = CommandResponse::missingParameter("dec");
            return;
        }

        std::string ra = payload["ra"].get<std::string>();
        std::string dec = payload["dec"].get<std::string>();

        try {
            auto result = getMountService().sync(deviceId, ra, dec);
            payload = result;
        } catch (const std::exception& e) {
            payload = CommandResponse::operationFailed("mount.sync", e.what());
        }
    });

    // Mount: capabilities
    dispatcher->registerCommand<json>("mount.capabilities", [](json& payload) {
        LOG_INFO("Executing command: mount.capabilities");

        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        try {
            auto result = getMountService().getCapabilities(deviceId);
            payload = result;
        } catch (const std::exception& e) {
            payload = CommandResponse::operationFailed("mount.capabilities",
                                                       e.what());
        }
    });

    // Mount: set guide rates
    dispatcher->registerCommand<json>(
        "mount.set_guide_rates", [](json& payload) {
            LOG_INFO("Executing command: mount.set_guide_rates");

            if (!payload.contains("deviceId") ||
                !payload["deviceId"].is_string() ||
                payload["deviceId"].get<std::string>().empty()) {
                payload = CommandResponse::missingParameter("deviceId");
                return;
            }
            std::string deviceId = payload["deviceId"].get<std::string>();

            if (!payload.contains("raRate") || !payload["raRate"].is_number()) {
                payload = CommandResponse::missingParameter("raRate");
                return;
            }
            if (!payload.contains("decRate") ||
                !payload["decRate"].is_number()) {
                payload = CommandResponse::missingParameter("decRate");
                return;
            }

            double raRate = payload["raRate"].get<double>();
            double decRate = payload["decRate"].get<double>();

            try {
                auto result =
                    getMountService().setGuideRates(deviceId, raRate, decRate);
                payload = result;
            } catch (const std::exception& e) {
                payload = CommandResponse::operationFailed(
                    "mount.set_guide_rates", e.what());
            }
        });

    // Mount: set tracking rate (Sidereal / Lunar / Solar)
    dispatcher->registerCommand<json>(
        "mount.set_tracking_rate", [](json& payload) {
            LOG_INFO("Executing command: mount.set_tracking_rate");

            if (!payload.contains("deviceId") ||
                !payload["deviceId"].is_string() ||
                payload["deviceId"].get<std::string>().empty()) {
                payload = CommandResponse::missingParameter("deviceId");
                return;
            }
            std::string deviceId = payload["deviceId"].get<std::string>();

            if (!payload.contains("rate") || !payload["rate"].is_string() ||
                payload["rate"].get<std::string>().empty()) {
                payload = CommandResponse::missingParameter("rate");
                return;
            }
            std::string rate = payload["rate"].get<std::string>();

            try {
                auto result = getMountService().setTrackingRate(deviceId, rate);
                payload = result;
            } catch (const std::exception& e) {
                payload = CommandResponse::operationFailed(
                    "mount.set_tracking_rate", e.what());
            }
        });

    // Mount: pier side
    dispatcher->registerCommand<json>("mount.pier_side", [](json& payload) {
        LOG_INFO("Executing command: mount.pier_side");

        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        try {
            auto result = getMountService().getPierSide(deviceId);
            payload = result;
        } catch (const std::exception& e) {
            payload =
                CommandResponse::operationFailed("mount.pier_side", e.what());
        }
    });

    // Mount: meridian flip
    dispatcher->registerCommand<json>("mount.meridian_flip", [](json& payload) {
        LOG_INFO("Executing command: mount.meridian_flip");

        if (!payload.contains("deviceId") || !payload["deviceId"].is_string() ||
            payload["deviceId"].get<std::string>().empty()) {
            payload = CommandResponse::missingParameter("deviceId");
            return;
        }
        std::string deviceId = payload["deviceId"].get<std::string>();

        try {
            auto result = getMountService().performMeridianFlip(deviceId);
            payload = result;
        } catch (const std::exception& e) {
            payload = CommandResponse::operationFailed("mount.meridian_flip",
                                                       e.what());
        }
    });
}

}  // namespace lithium::app
