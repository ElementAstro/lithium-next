#ifndef LITHIUM_SERVER_CONTROLLER_FILTERWHEEL_HPP
#define LITHIUM_SERVER_CONTROLLER_FILTERWHEEL_HPP

#include <crow/json.h>
#include "../controller.hpp"

#include <string>

#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"
#include "server/command/device/filterwheel.hpp"
#include "server/utils/response.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

class FilterWheelController : public Controller {
private:
    static auto isValidDeviceId(const std::string& deviceId) -> bool {
        return deviceId == "fw-001";
    }

public:
    void registerRoutes(lithium::server::ServerApp& app) override {
        CROW_ROUTE(app, "/api/v1/filterwheels")
            .methods("GET"_method)(
                &FilterWheelController::listFilterWheelsRoute, this);

        CROW_ROUTE(app, "/api/v1/filterwheels/<string>")
            .methods("GET"_method)(
                &FilterWheelController::getFilterWheelStatusRoute, this);

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
            .methods("GET"_method)(&FilterWheelController::capabilitiesRoute,
                                   this);

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
            .methods("POST"_method)(&FilterWheelController::calibrateRoute,
                                    this);
    }

    void listFilterWheelsRoute(const crow::request& req, crow::response& res) {
        (void)req;
        auto body = lithium::middleware::listFilterWheels();
        res = ResponseBuilder::success(body);
    }

    void getFilterWheelStatusRoute(const crow::request& req,
                                   crow::response& res,
                                   const std::string& deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "FilterWheel");
            return;
        }
        auto body = lithium::middleware::getFilterWheelStatus(deviceId);
        res = ResponseBuilder::success(body);
    }

    void connectFilterWheelRoute(const crow::request& req, crow::response& res,
                                 const std::string& deviceId) {
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "FilterWheel");
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            bool connected = body.value("connected", true);
            auto result =
                lithium::middleware::connectFilterWheel(deviceId, connected);
            res = ResponseBuilder::success(result);
        } catch (const std::exception& e) {
            res = ResponseBuilder::badRequest(std::string("Invalid JSON: ") +
                                              e.what());
        }
    }

    void setFilterPositionRoute(const crow::request& req, crow::response& res,
                                const std::string& deviceId) {
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "FilterWheel");
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            auto result =
                lithium::middleware::setFilterPosition(deviceId, body);
            res = ResponseBuilder::accepted(result);
        } catch (const std::exception& e) {
            res = ResponseBuilder::badRequest(std::string("Invalid JSON: ") +
                                              e.what());
        }
    }

    void setFilterByNameRoute(const crow::request& req, crow::response& res,
                              const std::string& deviceId) {
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "FilterWheel");
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            auto result = lithium::middleware::setFilterByName(deviceId, body);
            res = ResponseBuilder::accepted(result);
        } catch (const std::exception& e) {
            res = ResponseBuilder::badRequest(std::string("Invalid JSON: ") +
                                              e.what());
        }
    }

    void capabilitiesRoute(const crow::request& req, crow::response& res,
                           const std::string& deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "FilterWheel");
            return;
        }
        auto result = lithium::middleware::getFilterWheelCapabilities(deviceId);
        res = ResponseBuilder::success(result);
    }

    void configureFilterNamesRoute(const crow::request& req,
                                   crow::response& res,
                                   const std::string& deviceId) {
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "FilterWheel");
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            auto result =
                lithium::middleware::configureFilterNames(deviceId, body);
            res = ResponseBuilder::success(result);
        } catch (const std::exception& e) {
            res = ResponseBuilder::badRequest(std::string("Invalid JSON: ") +
                                              e.what());
        }
    }

    void getFilterOffsetsRoute(const crow::request& req, crow::response& res,
                               const std::string& deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "FilterWheel");
            return;
        }
        auto result = lithium::middleware::getFilterOffsets(deviceId);
        res = ResponseBuilder::success(result);
    }

    void setFilterOffsetsRoute(const crow::request& req, crow::response& res,
                               const std::string& deviceId) {
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "FilterWheel");
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            auto result = lithium::middleware::setFilterOffsets(deviceId, body);
            res = ResponseBuilder::success(result);
        } catch (const std::exception& e) {
            res = ResponseBuilder::badRequest(std::string("Invalid JSON: ") +
                                              e.what());
        }
    }

    void haltRoute(const crow::request& req, crow::response& res,
                   const std::string& deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "FilterWheel");
            return;
        }
        auto result = lithium::middleware::haltFilterWheel(deviceId);
        res = ResponseBuilder::success(result);
    }

    void calibrateRoute(const crow::request& req, crow::response& res,
                        const std::string& deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "FilterWheel");
            return;
        }
        auto result = lithium::middleware::calibrateFilterWheel(deviceId);
        res = ResponseBuilder::accepted(result);
    }
};

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_FILTERWHEEL_HPP

