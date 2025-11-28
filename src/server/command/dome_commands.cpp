#include "dome_commands.hpp"

#include <spdlog/spdlog.h>

#include "atom/type/json.hpp"
#include "command.hpp"
#include "command/dome.hpp"

namespace lithium::app {

using nlohmann::json;

void registerDomeCommands(std::shared_ptr<CommandDispatcher> dispatcher) {
    // Dome: list
    dispatcher->registerCommand<json>(
        "dome.list",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            (void)p;
            auto result = lithium::middleware::listDomes();
            p = result;
        });
    spdlog::info("Registered command handler for 'dome.list'");

    // Dome: status
    dispatcher->registerCommand<json>(
        "dome.status",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            std::string deviceId = p.value("deviceId", std::string("dom-001"));
            auto result = lithium::middleware::getDomeStatus(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'dome.status'");

    // Dome: connect
    dispatcher->registerCommand<json>(
        "dome.connect",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            std::string deviceId = p.value("deviceId", std::string("dom-001"));
            if (!p.contains("connected")) {
                 throw std::runtime_error("dome.connect: missing 'connected'");
            }
            bool connected = p["connected"].get<bool>();
            auto result = lithium::middleware::connectDome(deviceId, connected);
            p = result;
        });
    spdlog::info("Registered command handler for 'dome.connect'");

    // Dome: slew
    dispatcher->registerCommand<json>(
        "dome.slew",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            std::string deviceId = p.value("deviceId", std::string("dom-001"));
            if (!p.contains("azimuth")) {
                 throw std::runtime_error("dome.slew: missing 'azimuth'");
            }
            double az = p["azimuth"].get<double>();
            auto result = lithium::middleware::slewDome(deviceId, az);
            p = result;
        });
    spdlog::info("Registered command handler for 'dome.slew'");

    // Dome: shutter
    dispatcher->registerCommand<json>(
        "dome.shutter",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            std::string deviceId = p.value("deviceId", std::string("dom-001"));
            if (!p.contains("open")) {
                 throw std::runtime_error("dome.shutter: missing 'open'");
            }
            bool open = p["open"].get<bool>();
            auto result = lithium::middleware::shutterControl(deviceId, open);
            p = result;
        });
    spdlog::info("Registered command handler for 'dome.shutter'");

    // Dome: park
    dispatcher->registerCommand<json>(
        "dome.park",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            std::string deviceId = p.value("deviceId", std::string("dom-001"));
            auto result = lithium::middleware::parkDome(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'dome.park'");

    // Dome: unpark
    dispatcher->registerCommand<json>(
        "dome.unpark",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            std::string deviceId = p.value("deviceId", std::string("dom-001"));
            auto result = lithium::middleware::unparkDome(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'dome.unpark'");

    // Dome: home
    dispatcher->registerCommand<json>(
        "dome.home",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            std::string deviceId = p.value("deviceId", std::string("dom-001"));
            auto result = lithium::middleware::homeDome(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'dome.home'");
    
    // Dome: stop
    dispatcher->registerCommand<json>(
        "dome.stop",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            std::string deviceId = p.value("deviceId", std::string("dom-001"));
            auto result = lithium::middleware::stopDome(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'dome.stop'");

    // Dome: capabilities
    dispatcher->registerCommand<json>(
        "dome.capabilities",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            std::string deviceId = p.value("deviceId", std::string("dom-001"));
            auto result = lithium::middleware::getDomeCapabilities(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'dome.capabilities'");
}

}  // namespace lithium::app
