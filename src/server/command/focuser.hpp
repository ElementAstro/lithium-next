#ifndef LITHIUM_SERVER_COMMAND_FOCUSER_HPP
#define LITHIUM_SERVER_COMMAND_FOCUSER_HPP

#include <memory>

namespace lithium::app {

class CommandDispatcher;

void registerFocuser(std::shared_ptr<CommandDispatcher> dispatcher);

}  // namespace lithium::app

#endif  // LITHIUM_SERVER_COMMAND_FOCUSER_HPP
