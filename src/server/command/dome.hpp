#ifndef LITHIUM_SERVER_COMMAND_DOME_HPP
#define LITHIUM_SERVER_COMMAND_DOME_HPP

#include <memory>

namespace lithium::app {

class CommandDispatcher;

void registerDome(std::shared_ptr<CommandDispatcher> dispatcher);

}  // namespace lithium::app

#endif  // LITHIUM_SERVER_COMMAND_DOME_HPP
