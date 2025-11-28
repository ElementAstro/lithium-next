#include "filterwheel.hpp"

#include <spdlog/spdlog.h>

#include "atom/type/json.hpp"
#include "command.hpp"
#include "device/service/filterwheel_service.hpp"

namespace lithium::app {

using nlohmann::json;

// Global service instance
static lithium::device::FilterWheelService& getFilterWheelService() {
    static lithium::device::FilterWheelService instance;
    return instance;
}

void registerFilterWheel(std::shared_ptr<CommandDispatcher> dispatcher) {
    // FilterWheel: list
    dispatcher->registerCommand<json>(
        "filterwheel.list",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            (void)p;
            auto result = getFilterWheelService().list();
            p = result;
        });
    spdlog::info("Registered command handler for 'filterwheel.list'");

    // FilterWheel: status
    dispatcher->registerCommand<json>(
        "filterwheel.status",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("fw-001"));
            auto result = getFilterWheelService().getStatus(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'filterwheel.status'");

    // FilterWheel: connect / disconnect
    dispatcher->registerCommand<json>(
        "filterwheel.connect",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("fw-001"));
            if (!p.contains("connected")) {
                throw std::runtime_error(
                    "filterwheel.connect: missing 'connected'");
            }
            bool connected = p["connected"].get<bool>();
            auto result = getFilterWheelService().connect(deviceId, connected);
            p = result;
        });
    spdlog::info("Registered command handler for 'filterwheel.connect'");

    // FilterWheel: set position by slot
    dispatcher->registerCommand<json>(
        "filterwheel.set_position",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("fw-001"));
            auto result = getFilterWheelService().setPosition(deviceId, p);
            p = result;
        });
    spdlog::info("Registered command handler for 'filterwheel.set_position'");

    // FilterWheel: set position by filter name
    dispatcher->registerCommand<json>(
        "filterwheel.set_by_name",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("fw-001"));
            auto result = getFilterWheelService().setByName(deviceId, p);
            p = result;
        });
    spdlog::info("Registered command handler for 'filterwheel.set_by_name'");

    // FilterWheel: capabilities
    dispatcher->registerCommand<json>(
        "filterwheel.capabilities",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("fw-001"));
            auto result = getFilterWheelService().getCapabilities(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'filterwheel.capabilities'");

    // FilterWheel: configure filter names
    dispatcher->registerCommand<json>(
        "filterwheel.configure_names",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("fw-001"));
            auto result = getFilterWheelService().configureNames(deviceId, p);
            p = result;
        });
    spdlog::info("Registered command handler for 'filterwheel.configure_names'");

    // FilterWheel: get offsets
    dispatcher->registerCommand<json>(
        "filterwheel.get_offsets",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("fw-001"));
            auto result = getFilterWheelService().getOffsets(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'filterwheel.get_offsets'");

    // FilterWheel: set offsets
    dispatcher->registerCommand<json>(
        "filterwheel.set_offsets",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("fw-001"));
            auto result = getFilterWheelService().setOffsets(deviceId, p);
            p = result;
        });
    spdlog::info("Registered command handler for 'filterwheel.set_offsets'");

    // FilterWheel: halt (currently feature_not_supported)
    dispatcher->registerCommand<json>(
        "filterwheel.halt",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("fw-001"));
            auto result = getFilterWheelService().halt(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'filterwheel.halt'");

    // FilterWheel: calibrate (currently feature_not_supported)
    dispatcher->registerCommand<json>(
        "filterwheel.calibrate",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("fw-001"));
            auto result = getFilterWheelService().calibrate(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'filterwheel.calibrate'");
}

}  // namespace lithium::app
