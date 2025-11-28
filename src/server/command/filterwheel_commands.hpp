#ifndef LITHIUM_SERVER_FILTERWHEEL_COMMANDS_HPP
#define LITHIUM_SERVER_FILTERWHEEL_COMMANDS_HPP

#include <memory>

namespace lithium::app {

class CommandDispatcher;

void registerFilterWheelCommands(std::shared_ptr<CommandDispatcher> dispatcher);

}  // namespace lithium::app

#endif  // LITHIUM_SERVER_FILTERWHEEL_COMMANDS_HPP
