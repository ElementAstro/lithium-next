#ifndef LITHIUM_SERVER_MIDDLEWARE_GPIO_HPP
#define LITHIUM_SERVER_MIDDLEWARE_GPIO_HPP

#include "config/configor.hpp"

#include "atom/async/message_bus.hpp"
#include "atom/error/exception.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/log/loguru.hpp"
#include "atom/system/gpio.hpp"
#include "atom/type/json.hpp"
#include "atom/utils/print.hpp"

#include "constant/constant.hpp"
#include "utils/macro.hpp"

#define GPIO_PIN_1 "516"
#define GPIO_PIN_2 "527"

namespace lithium::middleware {
inline void getGPIOsStatus() {
    LOG_F(INFO, "getGPIOsStatus: Entering function");

    LITHIUM_GET_REQUIRED_PTR(configManager, lithium::ConfigManager,
                             Constants::CONFIG_MANAGER);
    LITHIUM_GET_REQUIRED_PTR(messageBus, atom::async::MessageBus,
                             Constants::MESSAGE_BUS);

    const std::vector<std::pair<int, std::string>> gpioPins = {{1, GPIO_PIN_1},
                                                               {2, GPIO_PIN_2}};

    for (const auto& [id, pin] : gpioPins) {
        LOG_F(DEBUG, "getGPIOsStatus: Processing GPIO pin: {} with ID: {}", pin,
              id);
        atom::system::GPIO gpio(pin);
        try {
            int value = static_cast<int>(gpio.getValue());
            LOG_F(INFO, "getGPIOsStatus: GPIO pin: {} has value: {}", pin,
                  value);
            configManager->set("/quarcs/gpio/{}"_fmt(id), value);
            messageBus->publish("quarcs",
                                "OutPutPowerStatus:{}:{}"_fmt(id, value));
        } catch (const atom::error::Exception& e) {
            LOG_F(ERROR, "getGPIOsStatus: Failed to get value for GPIO pin: {}",
                  pin);
        }
    }
    LOG_F(INFO, "getGPIOsStatus: Exiting function");
}

inline void switchOutPutPower(int id) {
    LOG_F(INFO, "switchOutPutPower: Entering function with ID: {}", id);

    LITHIUM_GET_REQUIRED_PTR(configManagerPtr, lithium::ConfigManager,
                             Constants::CONFIG_MANAGER);
    LITHIUM_GET_REQUIRED_PTR(messageBusPtr, atom::async::MessageBus,
                             Constants::MESSAGE_BUS);

    const std::vector<std::pair<int, std::string>> gpioPins = {{1, GPIO_PIN_1},
                                                               {2, GPIO_PIN_2}};

    auto it = std::find_if(gpioPins.begin(), gpioPins.end(),
                           [id](const auto& pair) { return pair.first == id; });

    if (it != gpioPins.end()) {
        LOG_F(DEBUG, "switchOutPutPower: Found GPIO pin: {} for ID: {}",
              it->second, id);
        atom::system::GPIO gpio(it->second);
        bool newValue;
        try {
            newValue = !gpio.getValue();
            LOG_F(INFO,
                  "switchOutPutPower: Setting GPIO pin: {} to new value: {}",
                  it->second, newValue);
            gpio.setValue(newValue);

            configManagerPtr->set("/quarcs/gpio/{}"_fmt(id), newValue);
            messageBusPtr->publish("quarcs",
                                   "OutPutPowerStatus:{}:{}"_fmt(id, newValue));
            return;
        } catch (const atom::error::Exception& e) {
            LOG_F(ERROR,
                  "switchOutPutPower: Failed to set GPIO pin: {} to new value: "
                  "{}",
                  it->second, newValue);
        }
    } else {
        LOG_F(WARNING, "switchOutPutPower: No GPIO pin found for ID: {}", id);
    }
    messageBusPtr->publish("quarcs", "OutPutPowerStatus:{}:-1"_fmt(id));

    LOG_F(INFO, "switchOutPutPower: Exiting function");
}
}  // namespace lithium::middleware

#undef GPIO_PIN_1
#undef GPIO_PIN_2

#endif
