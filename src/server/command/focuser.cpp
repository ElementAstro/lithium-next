#include "focuser.hpp"

#include <spdlog/spdlog.h>

#include "atom/type/json.hpp"
#include "command.hpp"
#include "device/service/focuser_service.hpp"

namespace lithium::app {

using nlohmann::json;

// Global service instance
static lithium::device::FocuserService& getFocuserService() {
    static lithium::device::FocuserService instance;
    return instance;
}

void registerFocuser(std::shared_ptr<CommandDispatcher> dispatcher) {
    // Focuser: list
    dispatcher->registerCommand<json>(
        "focuser.list",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);
            (void)p;
            auto result = getFocuserService().list();
            p = result;
        });
    spdlog::info("Registered command handler for 'focuser.list'");

    // Focuser: status
    dispatcher->registerCommand<json>(
        "focuser.status",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("foc-001"));
            auto result = getFocuserService().getStatus(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'focuser.status'");

    // Focuser: connect / disconnect
    dispatcher->registerCommand<json>(
        "focuser.connect",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("foc-001"));
            if (!p.contains("connected")) {
                throw std::runtime_error(
                    "focuser.connect: missing 'connected'");
            }
            bool connected = p["connected"].get<bool>();
            auto result = getFocuserService().connect(deviceId, connected);
            p = result;
        });
    spdlog::info("Registered command handler for 'focuser.connect'");

    // Focuser: move (absolute or relative)
    dispatcher->registerCommand<json>(
        "focuser.move",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("foc-001"));
            auto result = getFocuserService().move(deviceId, p);
            p = result;
        });
    spdlog::info("Registered command handler for 'focuser.move'");

    // Focuser: update settings
    dispatcher->registerCommand<json>(
        "focuser.update_settings",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("foc-001"));
            auto result = getFocuserService().updateSettings(deviceId, p);
            p = result;
        });
    spdlog::info("Registered command handler for 'focuser.update_settings'");

    // Focuser: halt
    dispatcher->registerCommand<json>(
        "focuser.halt",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("foc-001"));
            auto result = getFocuserService().halt(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'focuser.halt'");

    // Focuser: capabilities
    dispatcher->registerCommand<json>(
        "focuser.capabilities",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("foc-001"));
            auto result = getFocuserService().getCapabilities(deviceId);
            p = result;
        });
    spdlog::info("Registered command handler for 'focuser.capabilities'");

    // Focuser: start autofocus
    dispatcher->registerCommand<json>(
        "focuser.autofocus_start",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("foc-001"));
            auto result = getFocuserService().startAutofocus(deviceId, p);
            p = result;
        });
    spdlog::info("Registered command handler for 'focuser.autofocus_start'");

    // Focuser: autofocus status
    dispatcher->registerCommand<json>(
        "focuser.autofocus_status",
        [](const json& payload) {
            auto& p = const_cast<json&>(payload);

            std::string deviceId = p.value("deviceId", std::string("foc-001"));
            std::string autofocusId = p.value("autofocusId", std::string(""));
            auto result = getFocuserService().getAutofocusStatus(deviceId, autofocusId);
            p = result;
        });
    spdlog::info("Registered command handler for 'focuser.autofocus_status'");
}

}  // namespace lithium::app
