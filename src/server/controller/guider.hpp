#ifndef LITHIUM_SERVER_CONTROLLER_GUIDER_HPP
#define LITHIUM_SERVER_CONTROLLER_GUIDER_HPP

#include <crow/json.h>
#include "controller.hpp"
#include "server/command/guider.hpp"
#include "atom/log/loguru.hpp"

class GuiderController : public Controller {
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
        // Connect
        CROW_ROUTE(app, "/api/v1/guider/connect")
            .methods("POST"_method)([this](const crow::request &req,
                                            crow::response &res) {
                auto result = lithium::middleware::connectGuider();
                res = makeJsonResponse(result, 200);
            });

        // Disconnect
        CROW_ROUTE(app, "/api/v1/guider/disconnect")
            .methods("POST"_method)([this](const crow::request &req,
                                            crow::response &res) {
                auto result = lithium::middleware::disconnectGuider();
                res = makeJsonResponse(result, 200);
            });

        // Start Guiding
        CROW_ROUTE(app, "/api/v1/guider/start")
            .methods("POST"_method)([this](const crow::request &req,
                                            crow::response &res) {
                auto result = lithium::middleware::startGuiding();
                res = makeJsonResponse(result, 200);
            });

        // Stop Guiding
        CROW_ROUTE(app, "/api/v1/guider/stop")
            .methods("POST"_method)([this](const crow::request &req,
                                            crow::response &res) {
                auto result = lithium::middleware::stopGuiding();
                res = makeJsonResponse(result, 200);
            });

        // Status
        CROW_ROUTE(app, "/api/v1/guider/status")
            .methods("GET"_method)([this](const crow::request &req,
                                           crow::response &res) {
                auto result = lithium::middleware::getGuiderStatus();
                res = makeJsonResponse(result, 200);
            });

        // Dither
        CROW_ROUTE(app, "/api/v1/guider/dither")
            .methods("POST"_method)([this](const crow::request &req,
                                            crow::response &res) {
                try {
                    auto body = nlohmann::json::parse(req.body);
                    double pixels = body.value("pixels", 1.0);
                    auto result = lithium::middleware::ditherGuider(pixels);
                    res = makeJsonResponse(result, 200);
                } catch (...) {
                     auto err = lithium::models::api::makeError("invalid_json", "Invalid JSON");
                     res = makeJsonResponse(err, 400);
                }
            });

        // Stats
        CROW_ROUTE(app, "/api/v1/guider/stats")
            .methods("GET"_method)([this](const crow::request &req,
                                           crow::response &res) {
                auto result = lithium::middleware::getGuiderStats();
                res = makeJsonResponse(result, 200);
            });

        // Settings
        CROW_ROUTE(app, "/api/v1/guider/settings")
            .methods("PUT"_method)([this](const crow::request &req,
                                           crow::response &res) {
                try {
                    auto body = nlohmann::json::parse(req.body);
                    auto result = lithium::middleware::setGuiderSettings(body);
                    res = makeJsonResponse(result, 200);
                } catch (...) {
                     auto err = lithium::models::api::makeError("invalid_json", "Invalid JSON");
                     res = makeJsonResponse(err, 400);
                }
            });
    }
};

#endif // LITHIUM_SERVER_CONTROLLER_GUIDER_HPP
