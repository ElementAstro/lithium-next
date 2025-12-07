#ifndef LITHIUM_SERVER_CONTROLLER_SWITCH_HPP
#define LITHIUM_SERVER_CONTROLLER_SWITCH_HPP

#include <crow/json.h>
#include "../controller.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "server/command/gpio.hpp"
#include "server/utils/response.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

class SwitchController : public Controller {
public:
    void registerRoutes(lithium::server::ServerApp &app) override {
        // List Switches
        CROW_ROUTE(app, "/api/v1/switches")
            .methods("GET"_method)(
                [](const crow::request &req, crow::response &res) {
                    (void)req;
                    auto result = lithium::middleware::listSwitches();
                    res = ResponseBuilder::success(result);
                });

        // Set Switch State
        CROW_ROUTE(app, "/api/v1/switches/<int>")
            .methods("PUT"_method)(
                [](const crow::request &req, crow::response &res, int id) {
                    try {
                        auto body = nlohmann::json::parse(req.body);
                        if (!body.contains("on") || !body["on"].is_boolean()) {
                            res = ResponseBuilder::badRequest(
                                "'on' must be a boolean");
                            return;
                        }
                        bool state = body["on"];
                        auto result = lithium::middleware::setSwitch(id, state);
                        res = ResponseBuilder::success(result);
                    } catch (...) {
                        res = ResponseBuilder::badRequest("Invalid JSON");
                    }
                });

        // Toggle Switch
        CROW_ROUTE(app, "/api/v1/switches/<int>/toggle")
            .methods("POST"_method)(
                [](const crow::request &req, crow::response &res, int id) {
                    (void)req;
                    auto result = lithium::middleware::toggleSwitch(id);
                    res = ResponseBuilder::success(result);
                });
    }
};

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_SWITCH_HPP
