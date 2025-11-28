#ifndef LITHIUM_SERVER_DOME_COMMANDS_HPP
#define LITHIUM_SERVER_DOME_COMMANDS_HPP

#include <memory>
#include "command.hpp"

namespace lithium::app {

void registerDomeCommands(std::shared_ptr<CommandDispatcher> dispatcher);

}  // namespace lithium::app

#endif  // LITHIUM_SERVER_DOME_COMMANDS_HPP
