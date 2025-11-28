#include "mount_commands.hpp"

#include <spdlog/spdlog.h>

#include "atom/type/json.hpp"
#include "command.hpp"
#include "command/mount.hpp"

namespace lithium::app {

using nlohmann::json;

void registerMountCommands(std::shared_ptr<CommandDispatcher> dispatcher) {
    // Mount: get status
    dispatcher->registerCommand<json>(
        "mount.status",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("mnt-001"));
            auto result = lithium::middleware::getMountStatus(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'mount.status'");

    // Mount: slew to RA/Dec (sexagesimal strings)
    dispatcher->registerCommand<json>(
        "mount.slew",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("mnt-001"));
            if (!p.contains("ra") || !p.contains("dec")) {
                throw std::runtime_error("mount.slew: missing 'ra' or 'dec'");
            }
            std::string ra  = p["ra" ].get<std::string>();
            std::string dec = p["dec"].get<std::string>();

            auto result = lithium::middleware::slewMount(deviceId, ra, dec);
            p = result;
        });
    spdlog::info("Registered command handler for 'mount.slew'");

    // Mount: stop motion
    dispatcher->registerCommand<json>(
        "mount.stop",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("mnt-001"));
            auto result = lithium::middleware::stopMount(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'mount.stop'");

    // Mount: set tracking on/off
    dispatcher->registerCommand<json>(
        "mount.set_tracking",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("mnt-001"));
            if (!p.contains("tracking")) {
                throw std::runtime_error(
                    "mount.set_tracking: missing 'tracking'");
            }
            bool tracking = p["tracking"].get<bool>();
            auto result = lithium::middleware::setTracking(deviceId, tracking);
            p = result;
        });
    spdlog::info("Registered command handler for 'mount.set_tracking'");

    // Mount: list mounts
    dispatcher->registerCommand<json>(
        "mount.list",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            (void)p;  // currently unused
            auto result = lithium::middleware::listMounts();
            p = result;
        });
    spdlog::info("Registered command handler for 'mount.list'");

    // Mount: connect / disconnect
    dispatcher->registerCommand<json>(
        "mount.connect",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("mnt-001"));
            if (!p.contains("connected")) {
                throw std::runtime_error(
                    "mount.connect: missing 'connected'");
            }
            bool connected = p["connected"].get<bool>();
            auto result = lithium::middleware::connectMount(deviceId, connected);
            p = result;
        });
    spdlog::info("Registered command handler for 'mount.connect'");

    // Mount: position commands (PARK / UNPARK / HOME / FIND_HOME)
    dispatcher->registerCommand<json>(
        "mount.position",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("mnt-001"));
            if (!p.contains("command")) {
                throw std::runtime_error(
                    "mount.position: missing 'command'");
            }
            std::string command = p["command"].get<std::string>();
            auto result = lithium::middleware::setMountPosition(deviceId, command);
            p = result;
        });
    spdlog::info("Registered command handler for 'mount.position'");

    // Mount: pulse guide
    dispatcher->registerCommand<json>(
        "mount.pulse_guide",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("mnt-001"));
            if (!p.contains("direction") || !p.contains("durationMs")) {
                throw std::runtime_error(
                    "mount.pulse_guide: missing 'direction' or 'durationMs'");
            }
            std::string direction = p["direction"].get<std::string>();
            int durationMs = p["durationMs"].get<int>();
            auto result = lithium::middleware::pulseGuide(
                deviceId, direction, durationMs);
            p = result;
        });
    spdlog::info("Registered command handler for 'mount.pulse_guide'");

    // Mount: sync to RA/Dec
    dispatcher->registerCommand<json>(
        "mount.sync",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("mnt-001"));
            if (!p.contains("ra") || !p.contains("dec")) {
                throw std::runtime_error("mount.sync: missing 'ra' or 'dec'");
            }
            std::string ra  = p["ra" ].get<std::string>();
            std::string dec = p["dec"].get<std::string>();
            auto result = lithium::middleware::syncMount(deviceId, ra, dec);
            p = result;
        });
    spdlog::info("Registered command handler for 'mount.sync'");

    // Mount: capabilities
    dispatcher->registerCommand<json>(
        "mount.capabilities",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("mnt-001"));
            auto result = lithium::middleware::getMountCapabilities(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'mount.capabilities'");

    // Mount: set guide rates
    dispatcher->registerCommand<json>(
        "mount.set_guide_rates",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("mnt-001"));
            if (!p.contains("raRate") || !p.contains("decRate")) {
                throw std::runtime_error(
                    "mount.set_guide_rates: missing 'raRate' or 'decRate'");
            }
            double raRate  = p["raRate" ].get<double>();
            double decRate = p["decRate"].get<double>();
            auto result = lithium::middleware::setGuideRates(
                deviceId, raRate, decRate);
            p = result;
        });
    spdlog::info("Registered command handler for 'mount.set_guide_rates'");

    // Mount: set tracking rate (Sidereal / Lunar / Solar)
    dispatcher->registerCommand<json>(
        "mount.set_tracking_rate",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("mnt-001"));
            if (!p.contains("rate")) {
                throw std::runtime_error(
                    "mount.set_tracking_rate: missing 'rate'");
            }
            std::string rate = p["rate"].get<std::string>();
            auto result = lithium::middleware::setTrackingRate(deviceId, rate);
            p = result;
        });
    spdlog::info("Registered command handler for 'mount.set_tracking_rate'");

    // Mount: pier side
    dispatcher->registerCommand<json>(
        "mount.pier_side",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("mnt-001"));
            auto result = lithium::middleware::getPierSide(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'mount.pier_side'");

    // Mount: meridian flip
    dispatcher->registerCommand<json>(
        "mount.meridian_flip",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("mnt-001"));
            auto result = lithium::middleware::performMeridianFlip(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'mount.meridian_flip'");
}

}  // namespace lithium::app
