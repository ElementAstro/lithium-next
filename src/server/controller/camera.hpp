#ifndef LITHIUM_SERVER_CONTROLLER_CAMERA_HPP
#define LITHIUM_SERVER_CONTROLLER_CAMERA_HPP

#include <crow/json.h>
#include "controller.hpp"

#include <string>

#include "atom/log/loguru.hpp"
#include "atom/type/json.hpp"
#include "server/command/camera.hpp"

class CameraController : public Controller {
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
              {"message", "Camera not found"},
              {"details", {{"deviceId", deviceId}}}}}
        };
    }

    static auto isValidDeviceId(const std::string &deviceId) -> bool {
        // Currently only a single primary camera is supported.
        return deviceId == "cam-001";
    }

public:
    void registerRoutes(crow::SimpleApp &app) override {
        CROW_ROUTE(app, "/api/v1/cameras")
            .methods("GET"_method)(&CameraController::listCameras, this);

        CROW_ROUTE(app, "/api/v1/cameras/<string>")
            .methods("GET"_method)(&CameraController::getCameraStatusRoute,
                                     this);

        CROW_ROUTE(app, "/api/v1/cameras/<string>/connect")
            .methods("POST"_method)(&CameraController::connectCameraRoute,
                                      this);

        CROW_ROUTE(app, "/api/v1/cameras/<string>/settings")
            .methods("PUT"_method)(&CameraController::updateSettingsRoute,
                                     this);

        CROW_ROUTE(app, "/api/v1/cameras/<string>/exposure")
            .methods("POST"_method)(&CameraController::startExposureRoute,
                                      this);

        CROW_ROUTE(app, "/api/v1/cameras/<string>/exposure/abort")
            .methods("POST"_method)(&CameraController::abortExposureRoute,
                                      this);

        CROW_ROUTE(app, "/api/v1/cameras/<string>/capabilities")
            .methods("GET"_method)(&CameraController::capabilitiesRoute,
                                     this);

        CROW_ROUTE(app, "/api/v1/cameras/<string>/gains")
            .methods("GET"_method)(&CameraController::gainsRoute, this);

        CROW_ROUTE(app, "/api/v1/cameras/<string>/offsets")
            .methods("GET"_method)(&CameraController::offsetsRoute, this);

        CROW_ROUTE(app, "/api/v1/cameras/<string>/cooler-power")
            .methods("PUT"_method)(&CameraController::coolerPowerRoute, this);

        CROW_ROUTE(app, "/api/v1/cameras/<string>/warmup")
            .methods("POST"_method)(&CameraController::warmupRoute, this);
    }

    void listCameras(const crow::request &req, crow::response &res) {
        (void)req;
        auto body = lithium::middleware::listCameras();
        res = makeJsonResponse(body, 200);
    }

    void getCameraStatusRoute(const crow::request &req, crow::response &res,
                              const std::string &deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }
        auto body = lithium::middleware::getCameraStatus(deviceId);
        res = makeJsonResponse(body, 200);
    }

    void connectCameraRoute(const crow::request &req, crow::response &res,
                            const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            if (body.contains("connected") &&
                !body["connected"].is_boolean()) {
                nlohmann::json err = {
                    {"status", "error"},
                    {"error",
                     {{"code", "invalid_field_value"},
                      {"message",
                       "'connected' must be a boolean if provided"}}}};
                res = makeJsonResponse(err, 400);
                return;
            }

            bool connected = body.value("connected", true);
            auto result = lithium::middleware::connectCamera(deviceId,
                                                             connected);
            res = makeJsonResponse(result, 200);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void updateSettingsRoute(const crow::request &req, crow::response &res,
                             const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto settings = nlohmann::json::parse(req.body);

            // Validate CameraSettings basic types
            if (settings.contains("coolerOn") &&
                !settings["coolerOn"].is_boolean()) {
                nlohmann::json err = {
                    {"status", "error"},
                    {"error",
                     {{"code", "invalid_field_value"},
                      {"message",
                       "'coolerOn' must be a boolean if provided"}}}};
                res = makeJsonResponse(err, 400);
                return;
            }

            if (settings.contains("setpoint") &&
                !settings["setpoint"].is_number()) {
                nlohmann::json err = {
                    {"status", "error"},
                    {"error",
                     {{"code", "invalid_field_value"},
                      {"message",
                       "'setpoint' must be a number if provided"}}}};
                res = makeJsonResponse(err, 400);
                return;
            }

            if (settings.contains("gain") &&
                !settings["gain"].is_number_integer()) {
                nlohmann::json err = {
                    {"status", "error"},
                    {"error",
                     {{"code", "invalid_field_value"},
                      {"message",
                       "'gain' must be an integer if provided"}}}};
                res = makeJsonResponse(err, 400);
                return;
            }

            if (settings.contains("offset") &&
                !settings["offset"].is_number_integer()) {
                nlohmann::json err = {
                    {"status", "error"},
                    {"error",
                     {{"code", "invalid_field_value"},
                      {"message",
                       "'offset' must be an integer if provided"}}}};
                res = makeJsonResponse(err, 400);
                return;
            }

            if (settings.contains("binning") &&
                (!settings["binning"].is_object() ||
                 !settings["binning"].contains("x") ||
                 !settings["binning"].contains("y") ||
                 !settings["binning"]["x"].is_number_integer() ||
                 !settings["binning"]["y"].is_number_integer())) {
                nlohmann::json err = {
                    {"status", "error"},
                    {"error",
                     {{"code", "invalid_field_value"},
                      {"message",
                       "'binning' must be an object with integer 'x' and 'y'"}}}};
                res = makeJsonResponse(err, 400);
                return;
            }

            if (settings.contains("roi") &&
                (!settings["roi"].is_object() ||
                 !settings["roi"].contains("x") ||
                 !settings["roi"].contains("y") ||
                 !settings["roi"].contains("width") ||
                 !settings["roi"].contains("height") ||
                 !settings["roi"]["x"].is_number_integer() ||
                 !settings["roi"]["y"].is_number_integer() ||
                 !settings["roi"]["width"].is_number_integer() ||
                 !settings["roi"]["height"].is_number_integer())) {
                nlohmann::json err = {
                    {"status", "error"},
                    {"error",
                     {{"code", "invalid_field_value"},
                      {"message",
                       "'roi' must be an object with integer 'x','y','width','height'"}}}};
                res = makeJsonResponse(err, 400);
                return;
            }

            auto result =
                lithium::middleware::updateCameraSettings(deviceId, settings);
            res = makeJsonResponse(result, 202);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void startExposureRoute(const crow::request &req, crow::response &res,
                            const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            // Validate ExposureRequest schema
            if (!body.contains("duration") ||
                !body["duration"].is_number()) {
                nlohmann::json err = {
                    {"status", "error"},
                    {"error",
                     {{"code", "invalid_field_value"},
                      {"message",
                       "'duration' is required and must be a number"}}}};
                res = makeJsonResponse(err, 400);
                return;
            }

            double duration = body["duration"].get<double>();
            if (duration < 0.0) {
                nlohmann::json err = {
                    {"status", "error"},
                    {"error",
                     {{"code", "invalid_field_value"},
                      {"message",
                       "'duration' must be greater than or equal to 0"}}}};
                res = makeJsonResponse(err, 400);
                return;
            }

            if (!body.contains("frameType") ||
                !body["frameType"].is_string()) {
                nlohmann::json err = {
                    {"status", "error"},
                    {"error",
                     {{"code", "invalid_field_value"},
                      {"message",
                       "'frameType' is required and must be a string"}}}};
                res = makeJsonResponse(err, 400);
                return;
            }

            std::string frameType = body["frameType"].get<std::string>();
            if (frameType != "Light" && frameType != "Dark" &&
                frameType != "Flat" && frameType != "Bias") {
                nlohmann::json err = {
                    {"status", "error"},
                    {"error",
                     {{"code", "invalid_field_value"},
                      {"message",
                       "'frameType' must be one of: Light, Dark, Flat, Bias"}}}};
                res = makeJsonResponse(err, 400);
                return;
            }

            std::string filename = body.value("filename", std::string{});

            auto result = lithium::middleware::startExposure(
                deviceId, duration, frameType, filename);
            res = makeJsonResponse(result, 202);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void abortExposureRoute(const crow::request &req, crow::response &res,
                            const std::string &deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }
        auto result = lithium::middleware::abortExposure(deviceId);
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
        auto result = lithium::middleware::getCameraCapabilities(deviceId);
        res = makeJsonResponse(result, 200);
    }

    void gainsRoute(const crow::request &req, crow::response &res,
                    const std::string &deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }
        auto result = lithium::middleware::getCameraGains(deviceId);
        res = makeJsonResponse(result, 200);
    }

    void offsetsRoute(const crow::request &req, crow::response &res,
                      const std::string &deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }
        auto result = lithium::middleware::getCameraOffsets(deviceId);
        res = makeJsonResponse(result, 200);
    }

    void coolerPowerRoute(const crow::request &req, crow::response &res,
                          const std::string &deviceId) {
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            if (body.contains("power") && !body["power"].is_number()) {
                nlohmann::json err = {
                    {"status", "error"},
                    {"error",
                     {{"code", "invalid_field_value"},
                      {"message",
                       "'power' must be a number if provided"}}}};
                res = makeJsonResponse(err, 400);
                return;
            }

            double power = body.value("power", 0.0);
            if (power < 0.0 || power > 100.0) {
                nlohmann::json err = {
                    {"status", "error"},
                    {"error",
                     {{"code", "invalid_field_value"},
                      {"message",
                       "'power' must be between 0 and 100"}}}};
                res = makeJsonResponse(err, 400);
                return;
            }

            if (body.contains("mode") && !body["mode"].is_string()) {
                nlohmann::json err = {
                    {"status", "error"},
                    {"error",
                     {{"code", "invalid_field_value"},
                      {"message",
                       "'mode' must be a string if provided"}}}};
                res = makeJsonResponse(err, 400);
                return;
            }

            std::string mode = body.value("mode", std::string{"manual"});
            auto result =
                lithium::middleware::setCoolerPower(deviceId, power, mode);
            res = makeJsonResponse(result, 200);
        } catch (const std::exception &e) {
            nlohmann::json err = {{"status", "error"},
                                  {"error",
                                   {{"code", "invalid_json"},
                                    {"message", e.what()}}}};
            res = makeJsonResponse(err, 400);
        }
    }

    void warmupRoute(const crow::request &req, crow::response &res,
                     const std::string &deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            auto err = makeDeviceNotFound(deviceId);
            res = makeJsonResponse(err, 404);
            return;
        }
        auto result = lithium::middleware::warmUpCamera(deviceId);
        res = makeJsonResponse(result, 202);
    }
};

#endif  // LITHIUM_SERVER_CONTROLLER_CAMERA_HPP
