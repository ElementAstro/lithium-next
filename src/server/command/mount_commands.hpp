#ifndef LITHIUM_SERVER_MOUNT_COMMANDS_HPP
#define LITHIUM_SERVER_MOUNT_COMMANDS_HPP

#include <memory>

namespace lithium::app {

class CommandDispatcher;

void registerMountCommands(std::shared_ptr<CommandDispatcher> dispatcher);

}  // namespace lithium::app

#endif  // LITHIUM_SERVER_MOUNT_COMMANDS_HPP
