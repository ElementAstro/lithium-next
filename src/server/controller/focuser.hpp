#ifndef LITHIUM_SERVER_CONTROLLER_FOCUSER_HPP
#define LITHIUM_SERVER_CONTROLLER_FOCUSER_HPP

#include <crow/json.h>
#include "controller.hpp"

#include <string>

#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"
#include "server/command/focuser.hpp"

class FocuserController : public Controller {
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
              {"message", "Focuser not found"},
              {"details", {{"deviceId", deviceId}}}}}
        };
    }

    static auto isValidDeviceId(const std::string &deviceId) -> bool {
        // Currently only a single primary focuser is supported.
        return deviceId == "foc-001";
    }

public:
    void registerRoutes(lithium::server::ServerApp &app) override {
        CROW_ROUTE(app, "/api/v1/focusers")
            .methods("GET"_method)(&FocuserController::listFocusersRoute, this);

        CROW_ROUTE(app, "/api/v1/focusers/<string>")
            .methods("GET"_method)(&FocuserController::getFocuserStatusRoute,
                                     this);

        CROW_ROUTE(app, "/api/v1/focusers/<string>/connect")
            .methods("POST"_method)(&FocuserController::connectFocuserRoute,
                                      this);

        CROW_ROUTE(app, "/api/v1/focusers/<string>/move")
            .methods("POST"_method)(&FocuserController::moveFocuserRoute,
                                      this);

        CROW_ROUTE(app, "/api/v1/focusers/<string>/settings")
            .methods("PUT"_method)(&FocuserController::settingsRoute, this);

        CROW_ROUTE(app, "/api/v1/focusers/<string>/halt")
            .methods("POST"_method)(&FocuserController::haltRoute, this);

        CROW_ROUTE(app, "/api/v1/focusers/<string>/capabilities")
            .methods("GET"_method)(&FocuserController::capabilitiesRoute,
                                     this);

        CROW_ROUTE(app, "/api/v1/focusers/<string>/autofocus")
            .methods("POST"_method)(&FocuserController::autofocusRoute, this);

        CROW_ROUTE(app,
                   "/api/v1/focusers/<string>/autofocus/<string>")
            .methods("GET"_method)(&FocuserController::autofocusStatusRoute,
                                     this);
    }

    void listFocusersRoute(const crow::request &req, crow::response &res) {
        (void)req;
        auto body = lithium::middleware::listFocusers();
        res = makeJsonResponse(body, 200);
    }

    void getFocuserStatusRoute(const crow::request &req, crow::response &res,
                               const std::string &deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }
        auto body = lithium::middleware::getFocuserStatus(deviceId);
        res = makeJsonResponse(body, 200);
    }

    void connectFocuserRoute(const crow::request &req, crow::response &res,
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
                lithium::middleware::connectFocuser(deviceId, connected);
            res = makeJsonResponse(result, 200);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void moveFocuserRoute(const crow::request &req, crow::response &res,
                          const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            auto result = lithium::middleware::moveFocuser(deviceId, body);
            res = makeJsonResponse(result, 202);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void settingsRoute(const crow::request &req, crow::response &res,
                       const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto settings = nlohmann::json::parse(req.body);
            auto result = lithium::middleware::updateFocuserSettings(
                deviceId, settings);
            res = makeJsonResponse(result, 202);
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
        auto result = lithium::middleware::haltFocuser(deviceId);
        res = makeJsonResponse(result, 200);
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
            lithium::middleware::getFocuserCapabilities(deviceId);
        res = makeJsonResponse(result, 200);
    }

    void autofocusRoute(const crow::request &req, crow::response &res,
                        const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            auto result =
                lithium::middleware::startAutofocus(deviceId, body);
            res = makeJsonResponse(result, 202);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void autofocusStatusRoute(const crow::request &req, crow::response &res,
                              const std::string &deviceId,
                              const std::string &autofocusId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }
        auto result = lithium::middleware::getAutofocusStatus(deviceId,
                                                               autofocusId);
        res = makeJsonResponse(result, 200);
    }
};

#endif  // LITHIUM_SERVER_CONTROLLER_FOCUSER_HPP
