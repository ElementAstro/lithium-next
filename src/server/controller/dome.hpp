#ifndef LITHIUM_SERVER_CONTROLLER_DOME_HPP
#define LITHIUM_SERVER_CONTROLLER_DOME_HPP

#include "controller.hpp"
#include "../utils/response.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"
#include "server/command/dome.hpp"

namespace lithium::server::controller {

/**
 * @brief Dome/Observatory Controller
 */
class DomeController : public Controller {
public:
    void registerRoutes(lithium::server::ServerApp& app) override {
        CROW_ROUTE(app, "/api/v1/domes")
            .methods("GET"_method)
            (&DomeController::listDomes, this);

        CROW_ROUTE(app, "/api/v1/domes/<string>")
            .methods("GET"_method)
            (&DomeController::getDomeStatus, this);

        CROW_ROUTE(app, "/api/v1/domes/<string>/connect")
            .methods("POST"_method)
            (&DomeController::connectDome, this);

        CROW_ROUTE(app, "/api/v1/domes/<string>/slew")
            .methods("POST"_method)
            (&DomeController::slewDome, this);

        CROW_ROUTE(app, "/api/v1/domes/<string>/shutter")
            .methods("POST"_method)
            (&DomeController::controlShutter, this);

        CROW_ROUTE(app, "/api/v1/domes/<string>/park")
            .methods("POST"_method)
            (&DomeController::parkDome, this);

        CROW_ROUTE(app, "/api/v1/domes/<string>/unpark")
            .methods("POST"_method)
            (&DomeController::unparkDome, this);

        CROW_ROUTE(app, "/api/v1/domes/<string>/home")
            .methods("POST"_method)
            (&DomeController::homeDome, this);

        CROW_ROUTE(app, "/api/v1/domes/<string>/capabilities")
            .methods("GET"_method)
            (&DomeController::getCapabilities, this);
    }

private:
    crow::response listDomes(const crow::request& /*req*/) {
        auto body = lithium::middleware::listDomes();
        return makeResponse(body);
    }

    crow::response getDomeStatus(const crow::request& /*req*/, const std::string& deviceId) {
        auto body = lithium::middleware::getDomeStatus(deviceId);
        return makeResponse(body);
    }

    crow::response connectDome(const crow::request& req, const std::string& deviceId) {
        try {
            auto body = nlohmann::json::parse(req.body);
            bool connected = body.value("connected", true);
            auto result = lithium::middleware::connectDome(deviceId, connected);
            return makeResponse(result);
        } catch (const std::exception& e) {
            return makeErrorResponse(e.what());
        }
    }

    crow::response slewDome(const crow::request& req, const std::string& deviceId) {
        try {
            auto body = nlohmann::json::parse(req.body);
            if (!body.contains("azimuth")) {
                return makeErrorResponse("Missing 'azimuth' field");
            }
            double az = body["azimuth"];
            auto result = lithium::middleware::slewDome(deviceId, az);
            return makeResponse(result);
        } catch (const std::exception& e) {
            return makeErrorResponse(e.what());
        }
    }

    crow::response controlShutter(const crow::request& req, const std::string& deviceId) {
        try {
            auto body = nlohmann::json::parse(req.body);
            if (!body.contains("open")) {
                return makeErrorResponse("Missing 'open' field");
            }
            bool open = body["open"];
            auto result = lithium::middleware::shutterControl(deviceId, open);
            return makeResponse(result);
        } catch (const std::exception& e) {
            return makeErrorResponse(e.what());
        }
    }

    crow::response parkDome(const crow::request& /*req*/, const std::string& deviceId) {
        auto result = lithium::middleware::parkDome(deviceId);
        return makeResponse(result);
    }

    crow::response unparkDome(const crow::request& /*req*/, const std::string& deviceId) {
        auto result = lithium::middleware::unparkDome(deviceId);
        return makeResponse(result);
    }

    crow::response homeDome(const crow::request& /*req*/, const std::string& deviceId) {
        auto result = lithium::middleware::homeDome(deviceId);
        return makeResponse(result);
    }

    crow::response getCapabilities(const crow::request& /*req*/, const std::string& deviceId) {
        auto result = lithium::middleware::getDomeCapabilities(deviceId);
        return makeResponse(result);
    }

    crow::response makeResponse(const nlohmann::json& body) {
        crow::response res;
        res.set_header("Content-Type", "application/json");
        res.write(body.dump());
        if (body.contains("status") && body["status"] == "error") {
             if (body.contains("error") && body["error"].contains("code") && body["error"]["code"] == "device_not_found") {
                 res.code = 404;
             } else {
                 res.code = 400;
             }
        } else {
             res.code = 200;
        }
        return res;
    }

    crow::response makeErrorResponse(const std::string& msg) {
        nlohmann::json err;
        err["status"] = "error";
        err["error"] = {{"code", "bad_request"}, {"message", msg}};
        return makeResponse(err);
    }
};

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_DOME_HPP
