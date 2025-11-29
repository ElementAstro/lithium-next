#ifndef LITHIUM_SERVER_CONTROLLER_CONTROLLER_HPP
#define LITHIUM_SERVER_CONTROLLER_CONTROLLER_HPP

#include "../app.hpp"

namespace lithium::server::controller {

/**
 * @brief Base class for all HTTP controllers
 *
 * All controllers must inherit from this class and implement
 * the registerRoutes() method to define their HTTP endpoints.
 */
class Controller {
public:
    virtual ~Controller() = default;

    /**
     * @brief Register HTTP routes with the Crow application
     * @param app The Crow application instance
     */
    virtual void registerRoutes(ServerApp& app) = 0;
};

}  // namespace lithium::server::controller

// Backward compatibility alias
using Controller = lithium::server::controller::Controller;

#endif  // LITHIUM_SERVER_CONTROLLER_CONTROLLER_HPP
