#ifndef LITHIUM_SERVER_COMMAND_CAMERA_HPP
#define LITHIUM_SERVER_COMMAND_CAMERA_HPP

#include <memory>

namespace lithium::app {

class CommandDispatcher;

void registerCamera(std::shared_ptr<CommandDispatcher> dispatcher);

}  // namespace lithium::app

#endif  // LITHIUM_SERVER_COMMAND_CAMERA_HPP
