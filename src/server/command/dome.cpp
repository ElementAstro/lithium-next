#include "dome.hpp"

#include <spdlog/spdlog.h>

#include "atom/type/json.hpp"
#include "command.hpp"
#include "device/service/dome_service.hpp"

namespace lithium::app {

using nlohmann::json;

// Global service instance
static lithium::device::DomeService& getDomeService() {
    static lithium::device::DomeService instance;
    return instance;
}

void registerDome(std::shared_ptr<CommandDispatcher> dispatcher) {
    // Dome: list
    dispatcher->registerCommand<json>(
        "dome.list",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            (void)p;
            auto result = getDomeService().list();
            p = result;
        });
    spdlog::info("Registered command handler for 'dome.list'");

    // Dome: status
    dispatcher->registerCommand<json>(
        "dome.status",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            std::string deviceId = p.value("deviceId", std::string("dom-001"));
            auto result = getDomeService().getStatus(deviceId);
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
            auto result = getDomeService().connect(deviceId, connected);
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
            auto result = getDomeService().slew(deviceId, az);
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
            auto result = getDomeService().shutterControl(deviceId, open);
            p = result;
        });
    spdlog::info("Registered command handler for 'dome.shutter'");

    // Dome: park
    dispatcher->registerCommand<json>(
        "dome.park",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            std::string deviceId = p.value("deviceId", std::string("dom-001"));
            auto result = getDomeService().park(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'dome.park'");

    // Dome: unpark
    dispatcher->registerCommand<json>(
        "dome.unpark",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            std::string deviceId = p.value("deviceId", std::string("dom-001"));
            auto result = getDomeService().unpark(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'dome.unpark'");

    // Dome: home
    dispatcher->registerCommand<json>(
        "dome.home",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            std::string deviceId = p.value("deviceId", std::string("dom-001"));
            auto result = getDomeService().home(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'dome.home'");
    
    // Dome: stop
    dispatcher->registerCommand<json>(
        "dome.stop",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            std::string deviceId = p.value("deviceId", std::string("dom-001"));
            auto result = getDomeService().stop(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'dome.stop'");

    // Dome: capabilities
    dispatcher->registerCommand<json>(
        "dome.capabilities",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            std::string deviceId = p.value("deviceId", std::string("dom-001"));
            auto result = getDomeService().getCapabilities(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'dome.capabilities'");
}

}  // namespace lithium::app
