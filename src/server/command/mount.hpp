#ifndef LITHIUM_SERVER_COMMAND_MOUNT_HPP
#define LITHIUM_SERVER_COMMAND_MOUNT_HPP

#include <memory>

namespace lithium::app {

class CommandDispatcher;

void registerMount(std::shared_ptr<CommandDispatcher> dispatcher);

}  // namespace lithium::app

#endif  // LITHIUM_SERVER_COMMAND_MOUNT_HPP
