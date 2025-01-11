#ifndef LITHIUM_SERVER_CONTROLLER_CONTROLLER_H
#define LITHIUM_SERVER_CONTROLLER_CONTROLLER_H

#include <crow.h>

class Controller {
public:
    virtual ~Controller() = default;
    virtual void registerRoutes(crow::SimpleApp &app) = 0;
};

#endif