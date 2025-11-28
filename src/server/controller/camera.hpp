#ifndef LITHIUM_SERVER_CONTROLLER_CAMERA_HPP
#define LITHIUM_SERVER_CONTROLLER_CAMERA_HPP

#include <crow/json.h>
#include "controller.hpp"

#include <string>

#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"
#include "server/command/camera.hpp"
#include "server/models/api.hpp"

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
        return lithium::models::api::makeDeviceNotFound(deviceId, "Camera");
    }

    static auto isValidDeviceId(const std::string &deviceId) -> bool {
        // Currently only a single primary camera is supported.
        return deviceId == "cam-001";
    }

public:
    void registerRoutes(lithium::server::ServerApp &app) override {
        CROW_ROUTE(app, "/api/v1/cameras")
            .methods("GET"_method)([this](const crow::request &req,
                                           crow::response &res) {
                listCameras(req, res);
            });

        CROW_ROUTE(app, "/api/v1/cameras/<string>")
            .methods("GET"_method)([this](const crow::request &req,
                                           crow::response &res,
                                           const std::string &deviceId) {
                getCameraStatusRoute(req, res, deviceId);
            });

        CROW_ROUTE(app, "/api/v1/cameras/<string>/connect")
            .methods("POST"_method)([this](const crow::request &req,
                                            crow::response &res,
                                            const std::string &deviceId) {
                connectCameraRoute(req, res, deviceId);
            });

        CROW_ROUTE(app, "/api/v1/cameras/<string>/settings")
            .methods("PUT"_method)([this](const crow::request &req,
                                           crow::response &res,
                                           const std::string &deviceId) {
                updateSettingsRoute(req, res, deviceId);
            });

        CROW_ROUTE(app, "/api/v1/cameras/<string>/exposure")
            .methods("POST"_method)([this](const crow::request &req,
                                            crow::response &res,
                                            const std::string &deviceId) {
                startExposureRoute(req, res, deviceId);
            });

        CROW_ROUTE(app, "/api/v1/cameras/<string>/exposure/abort")
            .methods("POST"_method)([this](const crow::request &req,
                                            crow::response &res,
                                            const std::string &deviceId) {
                abortExposureRoute(req, res, deviceId);
            });

        CROW_ROUTE(app, "/api/v1/cameras/<string>/capabilities")
            .methods("GET"_method)([this](const crow::request &req,
                                           crow::response &res,
                                           const std::string &deviceId) {
                capabilitiesRoute(req, res, deviceId);
            });

        CROW_ROUTE(app, "/api/v1/cameras/<string>/gains")
            .methods("GET"_method)([this](const crow::request &req,
                                           crow::response &res,
                                           const std::string &deviceId) {
                gainsRoute(req, res, deviceId);
            });

        CROW_ROUTE(app, "/api/v1/cameras/<string>/offsets")
            .methods("GET"_method)([this](const crow::request &req,
                                           crow::response &res,
                                           const std::string &deviceId) {
                offsetsRoute(req, res, deviceId);
            });

        CROW_ROUTE(app, "/api/v1/cameras/<string>/cooler-power")
            .methods("PUT"_method)([this](const crow::request &req,
                                           crow::response &res,
                                           const std::string &deviceId) {
                coolerPowerRoute(req, res, deviceId);
            });

        CROW_ROUTE(app, "/api/v1/cameras/<string>/warmup")
            .methods("POST"_method)([this](const crow::request &req,
                                            crow::response &res,
                                            const std::string &deviceId) {
                warmupRoute(req, res, deviceId);
            });
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
                nlohmann::json err =
                    lithium::models::api::makeError(
                        "invalid_field_value",
                        "'connected' must be a boolean if provided");
                res = makeJsonResponse(err, 400);
                return;
            }

            bool connected = body.value("connected", true);
            auto result = lithium::middleware::connectCamera(deviceId,
                                                             connected);
            res = makeJsonResponse(result, 200);
        } catch (const std::exception &e) {
            nlohmann::json err = lithium::models::api::makeError(
                "invalid_json", e.what());
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
                nlohmann::json err =
                    lithium::models::api::makeError(
                        "invalid_field_value",
                        "'coolerOn' must be a boolean if provided");
                res = makeJsonResponse(err, 400);
                return;
            }

            if (settings.contains("setpoint") &&
                !settings["setpoint"].is_number()) {
                nlohmann::json err =
                    lithium::models::api::makeError(
                        "invalid_field_value",
                        "'setpoint' must be a number if provided");
                res = makeJsonResponse(err, 400);
                return;
            }

            if (settings.contains("gain") &&
                !settings["gain"].is_number_integer()) {
                nlohmann::json err =
                    lithium::models::api::makeError(
                        "invalid_field_value",
                        "'gain' must be an integer if provided");
                res = makeJsonResponse(err, 400);
                return;
            }

            if (settings.contains("offset") &&
                !settings["offset"].is_number_integer()) {
                nlohmann::json err =
                    lithium::models::api::makeError(
                        "invalid_field_value",
                        "'offset' must be an integer if provided");
                res = makeJsonResponse(err, 400);
                return;
            }

            if (settings.contains("binning") &&
                (!settings["binning"].is_object() ||
                 !settings["binning"].contains("x") ||
                 !settings["binning"].contains("y") ||
                 !settings["binning"]["x"].is_number_integer() ||
                 !settings["binning"]["y"].is_number_integer())) {
                nlohmann::json err =
                    lithium::models::api::makeError(
                        "invalid_field_value",
                        "'binning' must be an object with integer 'x' and 'y'");
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
                nlohmann::json err = lithium::models::api::makeError(
                    "invalid_field_value",
                    "'roi' must be an object with integer 'x','y','width','height'");
                res = makeJsonResponse(err, 400);
                return;
            }

            auto result =
                lithium::middleware::updateCameraSettings(deviceId, settings);
            res = makeJsonResponse(result, 202);
        } catch (const std::exception &e) {
            nlohmann::json err = lithium::models::api::makeError(
                "invalid_json", e.what());
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
                nlohmann::json err =
                    lithium::models::api::makeError(
                        "invalid_field_value",
                        "'duration' is required and must be a number");
                res = makeJsonResponse(err, 400);
                return;
            }

            double duration = body["duration"].get<double>();
            if (duration < 0.0) {
                nlohmann::json err =
                    lithium::models::api::makeError(
                        "invalid_field_value",
                        "'duration' must be greater than or equal to 0");
                res = makeJsonResponse(err, 400);
                return;
            }

            if (!body.contains("frameType") ||
                !body["frameType"].is_string()) {
                nlohmann::json err =
                    lithium::models::api::makeError(
                        "invalid_field_value",
                        "'frameType' is required and must be a string");
                res = makeJsonResponse(err, 400);
                return;
            }

            std::string frameType = body["frameType"].get<std::string>();
            if (frameType != "Light" && frameType != "Dark" &&
                frameType != "Flat" && frameType != "Bias") {
                nlohmann::json err =
                    lithium::models::api::makeError(
                        "invalid_field_value",
                        "'frameType' must be one of: Light, Dark, Flat, Bias");
                res = makeJsonResponse(err, 400);
                return;
            }

            std::string filename = body.value("filename", std::string{});

            auto result = lithium::middleware::startExposure(
                deviceId, duration, frameType, filename);
            res = makeJsonResponse(result, 202);
        } catch (const std::exception &e) {
            nlohmann::json err = lithium::models::api::makeError(
                "invalid_json", e.what());
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
                nlohmann::json err =
                    lithium::models::api::makeError(
                        "invalid_field_value",
                        "'power' must be a number if provided");
                res = makeJsonResponse(err, 400);
                return;
            }

            double power = body.value("power", 0.0);
            if (power < 0.0 || power > 100.0) {
                nlohmann::json err =
                    lithium::models::api::makeError(
                        "invalid_field_value",
                        "'power' must be between 0 and 100");
                res = makeJsonResponse(err, 400);
                return;
            }

            if (body.contains("mode") && !body["mode"].is_string()) {
                nlohmann::json err =
                    lithium::models::api::makeError(
                        "invalid_field_value",
                        "'mode' must be a string if provided");
                res = makeJsonResponse(err, 400);
                return;
            }

            std::string mode = body.value("mode", std::string{"manual"});
            auto result =
                lithium::middleware::setCoolerPower(deviceId, power, mode);
            res = makeJsonResponse(result, 200);
        } catch (const std::exception &e) {
            nlohmann::json err = lithium::models::api::makeError(
                "invalid_json", e.what());
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
