#ifndef LITHIUM_SERVER_COMMAND_FILTERWHEEL_HPP
#define LITHIUM_SERVER_COMMAND_FILTERWHEEL_HPP

#include <memory>

namespace lithium::app {

class CommandDispatcher;

void registerFilterWheel(std::shared_ptr<CommandDispatcher> dispatcher);

}  // namespace lithium::app

#endif  // LITHIUM_SERVER_COMMAND_FILTERWHEEL_HPP
