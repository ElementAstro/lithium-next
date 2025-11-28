#include "gpio.hpp"

#include "config/config.hpp"
#include "atom/async/message_bus.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "constant/constant.hpp"
#include "server/models/api.hpp"

// TODO: Include actual GPIO header when available
// #include "atom/system/gpio.hpp"

namespace lithium::middleware {

using json = nlohmann::json;

// Hardcoded pins from original file
static const std::vector<std::pair<int, std::string>> kGpioPins = {
    {1, "516"},
    {2, "527"}
};

auto listSwitches() -> json {
    LOG_INFO( "listSwitches: Listing all switches");
    json response;
    
    try {
        LITHIUM_GET_REQUIRED_PTR(configManager, lithium::ConfigManager, Constants::CONFIG_MANAGER);
        
        json data = json::array();
        for (const auto& [id, pin] : kGpioPins) {
            // Read state from config (simulated state)
            std::string configKey = "/quarcs/gpio/" + std::to_string(id);
            bool isOn = false;
            if (configManager->has(configKey)) {
                 try {
                     isOn = configManager->get(configKey).value_or(0) != 0;
                 } catch (...) {}
            }
            
            data.push_back({
                {"id", id},
                {"name", "Switch " + std::to_string(id)},
                {"pin", pin},
                {"on", isOn},
                {"canSwitch", true}
            });
        }
        
        response["status"] = "success";
        response["data"] = data;
        
    } catch (const std::exception& e) {
        LOG_ERROR( "listSwitches: Exception: %s", e.what());
        return lithium::models::api::makeError("internal_error", e.what());
    }
    
    return response;
}

auto setSwitch(int id, bool state) -> json {
    LOG_INFO( "setSwitch: Setting switch %d to %s", id, state ? "ON" : "OFF");
    
    auto it = std::find_if(kGpioPins.begin(), kGpioPins.end(),
                           [id](const auto& pair) { return pair.first == id; });
                           
    if (it == kGpioPins.end()) {
        return lithium::models::api::makeError("device_not_found", "Switch ID not found");
    }
    
    try {
        LITHIUM_GET_REQUIRED_PTR(configManager, lithium::ConfigManager, Constants::CONFIG_MANAGER);
        LITHIUM_GET_REQUIRED_PTR(messageBus, atom::async::MessageBus, Constants::MESSAGE_BUS);
        
        // TODO: Hardware control
        /*
        atom::system::GPIO gpio(it->second);
        gpio.setValue(state);
        */
        
        // Update config (Simulation)
        configManager->set("/quarcs/gpio/" + std::to_string(id), state ? 1 : 0);
        
        // Publish event
        messageBus->publish("quarcs", "OutPutPowerStatus:{}:{}"_fmt(id, state));
        
        json response;
        response["status"] = "success";
        response["message"] = "Switch state updated";
        response["data"] = {
            {"id", id},
            {"on", state}
        };
        return response;
        
    } catch (const std::exception& e) {
        LOG_ERROR( "setSwitch: Exception: %s", e.what());
        return lithium::models::api::makeError("internal_error", e.what());
    }
}

auto toggleSwitch(int id) -> json {
    LOG_INFO( "toggleSwitch: Toggling switch %d", id);
    
    try {
        LITHIUM_GET_REQUIRED_PTR(configManager, lithium::ConfigManager, Constants::CONFIG_MANAGER);
        
        std::string configKey = "/quarcs/gpio/" + std::to_string(id);
        bool currentState = false;
        if (configManager->has(configKey)) {
             try {
                 currentState = configManager->get(configKey).value_or(0) != 0;
             } catch (...) {}
        }
        
        return setSwitch(id, !currentState);
        
    } catch (const std::exception& e) {
        LOG_ERROR( "toggleSwitch: Exception: %s", e.what());
        return lithium::models::api::makeError("internal_error", e.what());
    }
}

} // namespace lithium::middleware
