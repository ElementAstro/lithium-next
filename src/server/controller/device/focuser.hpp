#ifndef LITHIUM_SERVER_CONTROLLER_FOCUSER_HPP
#define LITHIUM_SERVER_CONTROLLER_FOCUSER_HPP

#include <crow/json.h>
#include "../controller.hpp"

#include <string>

#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"
#include "server/command/device/focuser.hpp"
#include "server/utils/response.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

class FocuserController : public Controller {
private:
    static auto isValidDeviceId(const std::string& deviceId) -> bool {
        // Currently only a single primary focuser is supported.
        return deviceId == "foc-001";
    }

public:
    void registerRoutes(lithium::server::ServerApp& app) override {
        CROW_ROUTE(app, "/api/v1/focusers")
            .methods("GET"_method)(&FocuserController::listFocusersRoute, this);

        CROW_ROUTE(app, "/api/v1/focusers/<string>")
            .methods("GET"_method)(&FocuserController::getFocuserStatusRoute,
                                   this);

        CROW_ROUTE(app, "/api/v1/focusers/<string>/connect")
            .methods("POST"_method)(&FocuserController::connectFocuserRoute,
                                    this);

        CROW_ROUTE(app, "/api/v1/focusers/<string>/move")
            .methods("POST"_method)(&FocuserController::moveFocuserRoute, this);

        CROW_ROUTE(app, "/api/v1/focusers/<string>/settings")
            .methods("PUT"_method)(&FocuserController::settingsRoute, this);

        CROW_ROUTE(app, "/api/v1/focusers/<string>/halt")
            .methods("POST"_method)(&FocuserController::haltRoute, this);

        CROW_ROUTE(app, "/api/v1/focusers/<string>/capabilities")
            .methods("GET"_method)(&FocuserController::capabilitiesRoute, this);

        CROW_ROUTE(app, "/api/v1/focusers/<string>/autofocus")
            .methods("POST"_method)(&FocuserController::autofocusRoute, this);

        CROW_ROUTE(app, "/api/v1/focusers/<string>/autofocus/<string>")
            .methods("GET"_method)(&FocuserController::autofocusStatusRoute,
                                   this);
    }

    void listFocusersRoute(const crow::request& req, crow::response& res) {
        (void)req;
        auto body = lithium::middleware::listFocusers();
        res = ResponseBuilder::success(body);
    }

    void getFocuserStatusRoute(const crow::request& req, crow::response& res,
                               const std::string& deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "Focuser");
            return;
        }
        auto body = lithium::middleware::getFocuserStatus(deviceId);
        res = ResponseBuilder::success(body);
    }

    void connectFocuserRoute(const crow::request& req, crow::response& res,
                             const std::string& deviceId) {
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "Focuser");
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            bool connected = body.value("connected", true);
            auto result =
                lithium::middleware::connectFocuser(deviceId, connected);
            res = ResponseBuilder::success(result);
        } catch (const std::exception& e) {
            res = ResponseBuilder::badRequest(std::string("Invalid JSON: ") +
                                              e.what());
        }
    }

    void moveFocuserRoute(const crow::request& req, crow::response& res,
                          const std::string& deviceId) {
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "Focuser");
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            auto result = lithium::middleware::moveFocuser(deviceId, body);
            res = ResponseBuilder::accepted(result);
        } catch (const std::exception& e) {
            res = ResponseBuilder::badRequest(std::string("Invalid JSON: ") +
                                              e.what());
        }
    }

    void settingsRoute(const crow::request& req, crow::response& res,
                       const std::string& deviceId) {
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "Focuser");
            return;
        }

        try {
            auto settings = nlohmann::json::parse(req.body);
            auto result =
                lithium::middleware::updateFocuserSettings(deviceId, settings);
            res = ResponseBuilder::accepted(result);
        } catch (const std::exception& e) {
            res = ResponseBuilder::badRequest(std::string("Invalid JSON: ") +
                                              e.what());
        }
    }

    void haltRoute(const crow::request& req, crow::response& res,
                   const std::string& deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "Focuser");
            return;
        }
        auto result = lithium::middleware::haltFocuser(deviceId);
        res = ResponseBuilder::success(result);
    }

    void capabilitiesRoute(const crow::request& req, crow::response& res,
                           const std::string& deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "Focuser");
            return;
        }
        auto result = lithium::middleware::getFocuserCapabilities(deviceId);
        res = ResponseBuilder::success(result);
    }

    void autofocusRoute(const crow::request& req, crow::response& res,
                        const std::string& deviceId) {
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "Focuser");
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            auto result = lithium::middleware::startAutofocus(deviceId, body);
            res = ResponseBuilder::accepted(result);
        } catch (const std::exception& e) {
            res = ResponseBuilder::badRequest(std::string("Invalid JSON: ") +
                                              e.what());
        }
    }

    void autofocusStatusRoute(const crow::request& req, crow::response& res,
                              const std::string& deviceId,
                              const std::string& autofocusId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "Focuser");
            return;
        }
        auto result =
            lithium::middleware::getAutofocusStatus(deviceId, autofocusId);
        res = ResponseBuilder::success(result);
    }
};

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_FOCUSER_HPP

