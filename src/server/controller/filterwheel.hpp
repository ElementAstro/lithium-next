#ifndef LITHIUM_SERVER_CONTROLLER_FILTERWHEEL_HPP
#define LITHIUM_SERVER_CONTROLLER_FILTERWHEEL_HPP

#include <crow/json.h>
#include "controller.hpp"

#include <string>

#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"
#include "server/command/filterwheel.hpp"

class FilterWheelController : public Controller {
private:
    static auto makeJsonResponse(const nlohmann::json &body, int code)
        -> crow::response {
        crow::response res(code);
        res.set_header("Content-Type", "application/json");
        res.write(body.dump());
        return res;
    }

    static auto makeDeviceNotFound(const std::string &deviceId)
        -> nlohmann::json {
        return {
            {"status", "error"},
            {"error",
             {{"code", "device_not_found"},
              {"message", "Filter wheel not found"},
              {"details", {{"deviceId", deviceId}}}}}
        };
    }

    static auto isValidDeviceId(const std::string &deviceId) -> bool {
        return deviceId == "fw-001";
    }

public:
    void registerRoutes(lithium::server::ServerApp &app) override {
        CROW_ROUTE(app, "/api/v1/filterwheels")
            .methods("GET"_method)(&FilterWheelController::listFilterWheelsRoute,
                                     this);

        CROW_ROUTE(app, "/api/v1/filterwheels/<string>")
            .methods("GET"_method)(&FilterWheelController::getFilterWheelStatusRoute,
                                     this);

        CROW_ROUTE(app, "/api/v1/filterwheels/<string>/connect")
            .methods("POST"_method)(
                &FilterWheelController::connectFilterWheelRoute, this);

        CROW_ROUTE(app, "/api/v1/filterwheels/<string>/position")
            .methods("POST"_method)(
                &FilterWheelController::setFilterPositionRoute, this);

        CROW_ROUTE(app, "/api/v1/filterwheels/<string>/filter")
            .methods("POST"_method)(
                &FilterWheelController::setFilterByNameRoute, this);

        CROW_ROUTE(app, "/api/v1/filterwheels/<string>/capabilities")
            .methods("GET"_method)(
                &FilterWheelController::capabilitiesRoute, this);

        CROW_ROUTE(app, "/api/v1/filterwheels/<string>/filters")
            .methods("PUT"_method)(
                &FilterWheelController::configureFilterNamesRoute, this);

        CROW_ROUTE(app, "/api/v1/filterwheels/<string>/offsets")
            .methods("GET"_method)(
                &FilterWheelController::getFilterOffsetsRoute, this);

        CROW_ROUTE(app, "/api/v1/filterwheels/<string>/offsets")
            .methods("PUT"_method)(
                &FilterWheelController::setFilterOffsetsRoute, this);

        CROW_ROUTE(app, "/api/v1/filterwheels/<string>/halt")
            .methods("POST"_method)(&FilterWheelController::haltRoute, this);

        CROW_ROUTE(app, "/api/v1/filterwheels/<string>/calibrate")
            .methods("POST"_method)(
                &FilterWheelController::calibrateRoute, this);
    }

    void listFilterWheelsRoute(const crow::request &req, crow::response &res) {
        (void)req;
        auto body = lithium::middleware::listFilterWheels();
        res = makeJsonResponse(body, 200);
    }

    void getFilterWheelStatusRoute(const crow::request &req,
                                   crow::response &res,
                                   const std::string &deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }
        auto body = lithium::middleware::getFilterWheelStatus(deviceId);
        res = makeJsonResponse(body, 200);
    }

    void connectFilterWheelRoute(const crow::request &req,
                                 crow::response &res,
                                 const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            bool connected = body.value("connected", true);
            auto result =
                lithium::middleware::connectFilterWheel(deviceId, connected);
            res = makeJsonResponse(result, 200);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void setFilterPositionRoute(const crow::request &req, crow::response &res,
                                const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            auto result =
                lithium::middleware::setFilterPosition(deviceId, body);
            res = makeJsonResponse(result, 202);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void setFilterByNameRoute(const crow::request &req, crow::response &res,
                              const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            auto result =
                lithium::middleware::setFilterByName(deviceId, body);
            res = makeJsonResponse(result, 202);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void capabilitiesRoute(const crow::request &req, crow::response &res,
                           const std::string &deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }
        auto result =
            lithium::middleware::getFilterWheelCapabilities(deviceId);
        res = makeJsonResponse(result, 200);
    }

    void configureFilterNamesRoute(const crow::request &req,
                                   crow::response &res,
                                   const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            auto result =
                lithium::middleware::configureFilterNames(deviceId, body);
            res = makeJsonResponse(result, 200);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void getFilterOffsetsRoute(const crow::request &req, crow::response &res,
                               const std::string &deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }
        auto result = lithium::middleware::getFilterOffsets(deviceId);
        res = makeJsonResponse(result, 200);
    }

    void setFilterOffsetsRoute(const crow::request &req, crow::response &res,
                               const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            auto result =
                lithium::middleware::setFilterOffsets(deviceId, body);
            res = makeJsonResponse(result, 200);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void haltRoute(const crow::request &req, crow::response &res,
                   const std::string &deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }
        auto result = lithium::middleware::haltFilterWheel(deviceId);
        res = makeJsonResponse(result, 200);
    }

    void calibrateRoute(const crow::request &req, crow::response &res,
                        const std::string &deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }
        auto result = lithium::middleware::calibrateFilterWheel(deviceId);
        res = makeJsonResponse(result, 202);
    }
};

#endif  // LITHIUM_SERVER_CONTROLLER_FILTERWHEEL_HPP
