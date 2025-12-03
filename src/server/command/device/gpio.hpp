#ifndef LITHIUM_SERVER_MIDDLEWARE_GPIO_HPP
#define LITHIUM_SERVER_MIDDLEWARE_GPIO_HPP

#include "atom/type/json.hpp"

namespace lithium::middleware {

using json = nlohmann::json;

/**
 * @brief Get status of all GPIO switches.
 */
auto listSwitches() -> json;

/**
 * @brief Set switch state.
 */
auto setSwitch(int id, bool state) -> json;

/**
 * @brief Toggle switch state.
 */
auto toggleSwitch(int id) -> json;

}  // namespace lithium::middleware

#endif  // LITHIUM_SERVER_MIDDLEWARE_GPIO_HPP
