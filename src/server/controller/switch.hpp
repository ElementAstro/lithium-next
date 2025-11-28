#ifndef LITHIUM_SERVER_CONTROLLER_SWITCH_HPP
#define LITHIUM_SERVER_CONTROLLER_SWITCH_HPP

#include <crow/json.h>
#include "controller.hpp"
#include "server/command/gpio.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "server/models/api.hpp"

class SwitchController : public Controller {
private:
    static auto makeJsonResponse(const nlohmann::json &body, int code)
        -> crow::response {
        crow::response res(code);
        res.set_header("Content-Type", "application/json");
        res.write(body.dump());
        return res;
    }

public:
    void registerRoutes(lithium::server::ServerApp &app) override {
        // List Switches
        CROW_ROUTE(app, "/api/v1/switches")
            .methods("GET"_method)([](const crow::request &req,
                                           crow::response &res) {
                (void)req;
                auto result = lithium::middleware::listSwitches();
                res = makeJsonResponse(result, 200);
            });

        // Set Switch State
        CROW_ROUTE(app, "/api/v1/switches/<int>")
            .methods("PUT"_method)([](const crow::request &req,
                                           crow::response &res, int id) {
                try {
                    auto body = nlohmann::json::parse(req.body);
                    if (!body.contains("on") || !body["on"].is_boolean()) {
                         auto err = lithium::models::api::makeError("invalid_field_value", "'on' must be a boolean");
                         res = makeJsonResponse(err, 400);
                         return;
                    }
                    bool state = body["on"];
                    auto result = lithium::middleware::setSwitch(id, state);
                    res = makeJsonResponse(result, 200);
                } catch (...) {
                     auto err = lithium::models::api::makeError("invalid_json", "Invalid JSON");
                     res = makeJsonResponse(err, 400);
                }
            });

        // Toggle Switch
        CROW_ROUTE(app, "/api/v1/switches/<int>/toggle")
            .methods("POST"_method)([](const crow::request &req,
                                            crow::response &res, int id) {
                (void)req;
                auto result = lithium::middleware::toggleSwitch(id);
                res = makeJsonResponse(result, 200);
            });
    }
};

#endif // LITHIUM_SERVER_CONTROLLER_SWITCH_HPP
