#ifndef LITHIUM_SERVER_CONTROLLER_CONTROLLER_H
#define LITHIUM_SERVER_CONTROLLER_CONTROLLER_H

#include "../app.hpp"

class Controller {
public:
    virtual ~Controller() = default;
    virtual void registerRoutes(lithium::server::ServerApp &app) = 0;
};

#endif
