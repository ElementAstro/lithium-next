#ifndef LITHIUM_SERVER_CONTROLLER_CAMERA_HPP
#define LITHIUM_SERVER_CONTROLLER_CAMERA_HPP

#include <crow/json.h>
#include "controller.hpp"

#include <string>

#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"
#include "server/command/camera.hpp"
#include "server/utils/response.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

class CameraController : public Controller {
private:
    static auto isValidDeviceId(const std::string& deviceId) -> bool {
        // Currently only a single primary camera is supported.
        return deviceId == "cam-001";
    }

public:
    void registerRoutes(lithium::server::ServerApp& app) override {
        CROW_ROUTE(app, "/api/v1/cameras")
            .methods("GET"_method)(
                [this](const crow::request& req, crow::response& res) {
                    listCameras(req, res);
                });

        CROW_ROUTE(app, "/api/v1/cameras/<string>")
            .methods("GET"_method)([this](const crow::request& req,
                                          crow::response& res,
                                          const std::string& deviceId) {
                getCameraStatusRoute(req, res, deviceId);
            });

        CROW_ROUTE(app, "/api/v1/cameras/<string>/connect")
            .methods("POST"_method)([this](const crow::request& req,
                                           crow::response& res,
                                           const std::string& deviceId) {
                connectCameraRoute(req, res, deviceId);
            });

        CROW_ROUTE(app, "/api/v1/cameras/<string>/settings")
            .methods("PUT"_method)([this](const crow::request& req,
                                          crow::response& res,
                                          const std::string& deviceId) {
                updateSettingsRoute(req, res, deviceId);
            });

        CROW_ROUTE(app, "/api/v1/cameras/<string>/exposure")
            .methods("POST"_method)([this](const crow::request& req,
                                           crow::response& res,
                                           const std::string& deviceId) {
                startExposureRoute(req, res, deviceId);
            });

        CROW_ROUTE(app, "/api/v1/cameras/<string>/exposure/abort")
            .methods("POST"_method)([this](const crow::request& req,
                                           crow::response& res,
                                           const std::string& deviceId) {
                abortExposureRoute(req, res, deviceId);
            });

        CROW_ROUTE(app, "/api/v1/cameras/<string>/capabilities")
            .methods("GET"_method)([this](const crow::request& req,
                                          crow::response& res,
                                          const std::string& deviceId) {
                capabilitiesRoute(req, res, deviceId);
            });

        CROW_ROUTE(app, "/api/v1/cameras/<string>/gains")
            .methods("GET"_method)([this](const crow::request& req,
                                          crow::response& res,
                                          const std::string& deviceId) {
                gainsRoute(req, res, deviceId);
            });

        CROW_ROUTE(app, "/api/v1/cameras/<string>/offsets")
            .methods("GET"_method)([this](const crow::request& req,
                                          crow::response& res,
                                          const std::string& deviceId) {
                offsetsRoute(req, res, deviceId);
            });

        CROW_ROUTE(app, "/api/v1/cameras/<string>/cooler-power")
            .methods("PUT"_method)([this](const crow::request& req,
                                          crow::response& res,
                                          const std::string& deviceId) {
                coolerPowerRoute(req, res, deviceId);
            });

        CROW_ROUTE(app, "/api/v1/cameras/<string>/warmup")
            .methods("POST"_method)([this](const crow::request& req,
                                           crow::response& res,
                                           const std::string& deviceId) {
                warmupRoute(req, res, deviceId);
            });
    }

    void listCameras(const crow::request& req, crow::response& res) {
        (void)req;
        auto data = lithium::middleware::listCameras();
        res = ResponseBuilder::success(data);
    }

    void getCameraStatusRoute(const crow::request& req, crow::response& res,
                              const std::string& deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "Camera");
            return;
        }
        auto data = lithium::middleware::getCameraStatus(deviceId);
        res = ResponseBuilder::success(data);
    }

    void connectCameraRoute(const crow::request& req, crow::response& res,
                            const std::string& deviceId) {
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "Camera");
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            if (body.contains("connected") && !body["connected"].is_boolean()) {
                res = ResponseBuilder::badRequest(
                    "'connected' must be a boolean if provided");
                return;
            }

            bool connected = body.value("connected", true);
            auto result =
                lithium::middleware::connectCamera(deviceId, connected);
            res = ResponseBuilder::success(result);
        } catch (const std::exception& e) {
            res = ResponseBuilder::badRequest(std::string("Invalid JSON: ") +
                                              e.what());
        }
    }

    void updateSettingsRoute(const crow::request& req, crow::response& res,
                             const std::string& deviceId) {
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "Camera");
            return;
        }

        try {
            auto settings = nlohmann::json::parse(req.body);

            // Validate CameraSettings basic types
            if (settings.contains("coolerOn") &&
                !settings["coolerOn"].is_boolean()) {
                res = ResponseBuilder::badRequest(
                    "'coolerOn' must be a boolean if provided");
                return;
            }

            if (settings.contains("setpoint") &&
                !settings["setpoint"].is_number()) {
                res = ResponseBuilder::badRequest(
                    "'setpoint' must be a number if provided");
                return;
            }

            if (settings.contains("gain") &&
                !settings["gain"].is_number_integer()) {
                res = ResponseBuilder::badRequest(
                    "'gain' must be an integer if provided");
                return;
            }

            if (settings.contains("offset") &&
                !settings["offset"].is_number_integer()) {
                res = ResponseBuilder::badRequest(
                    "'offset' must be an integer if provided");
                return;
            }

            if (settings.contains("binning") &&
                (!settings["binning"].is_object() ||
                 !settings["binning"].contains("x") ||
                 !settings["binning"].contains("y") ||
                 !settings["binning"]["x"].is_number_integer() ||
                 !settings["binning"]["y"].is_number_integer())) {
                res = ResponseBuilder::badRequest(
                    "'binning' must be an object with integer 'x' and 'y'");
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
                res = ResponseBuilder::badRequest(
                    "'roi' must be an object with integer "
                    "'x','y','width','height'");
                return;
            }

            auto result =
                lithium::middleware::updateCameraSettings(deviceId, settings);
            res = ResponseBuilder::accepted(result);
        } catch (const std::exception& e) {
            res = ResponseBuilder::badRequest(std::string("Invalid JSON: ") +
                                              e.what());
        }
    }

    void startExposureRoute(const crow::request& req, crow::response& res,
                            const std::string& deviceId) {
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "Camera");
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            // Validate ExposureRequest schema
            if (!body.contains("duration") || !body["duration"].is_number()) {
                res = ResponseBuilder::badRequest(
                    "'duration' is required and must be a number");
                return;
            }

            double duration = body["duration"].get<double>();
            if (duration < 0.0) {
                res = ResponseBuilder::badRequest(
                    "'duration' must be greater than or equal to 0");
                return;
            }

            if (!body.contains("frameType") || !body["frameType"].is_string()) {
                res = ResponseBuilder::badRequest(
                    "'frameType' is required and must be a string");
                return;
            }

            std::string frameType = body["frameType"].get<std::string>();
            if (frameType != "Light" && frameType != "Dark" &&
                frameType != "Flat" && frameType != "Bias") {
                res = ResponseBuilder::badRequest(
                    "'frameType' must be one of: Light, Dark, Flat, Bias");
                return;
            }

            std::string filename = body.value("filename", std::string{});

            auto result = lithium::middleware::startExposure(
                deviceId, duration, frameType, filename);
            res = ResponseBuilder::accepted(result);
        } catch (const std::exception& e) {
            res = ResponseBuilder::badRequest(std::string("Invalid JSON: ") +
                                              e.what());
        }
    }

    void abortExposureRoute(const crow::request& req, crow::response& res,
                            const std::string& deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "Camera");
            return;
        }
        auto result = lithium::middleware::abortExposure(deviceId);
        res = ResponseBuilder::success(result);
    }

    void capabilitiesRoute(const crow::request& req, crow::response& res,
                           const std::string& deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "Camera");
            return;
        }
        auto result = lithium::middleware::getCameraCapabilities(deviceId);
        res = ResponseBuilder::success(result);
    }

    void gainsRoute(const crow::request& req, crow::response& res,
                    const std::string& deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "Camera");
            return;
        }
        auto result = lithium::middleware::getCameraGains(deviceId);
        res = ResponseBuilder::success(result);
    }

    void offsetsRoute(const crow::request& req, crow::response& res,
                      const std::string& deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "Camera");
            return;
        }
        auto result = lithium::middleware::getCameraOffsets(deviceId);
        res = ResponseBuilder::success(result);
    }

    void coolerPowerRoute(const crow::request& req, crow::response& res,
                          const std::string& deviceId) {
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "Camera");
            return;
        }

        try {
            auto body = nlohmann::json::parse(req.body);
            if (body.contains("power") && !body["power"].is_number()) {
                res = ResponseBuilder::badRequest(
                    "'power' must be a number if provided");
                return;
            }

            double power = body.value("power", 0.0);
            if (power < 0.0 || power > 100.0) {
                res = ResponseBuilder::badRequest(
                    "'power' must be between 0 and 100");
                return;
            }

            if (body.contains("mode") && !body["mode"].is_string()) {
                res = ResponseBuilder::badRequest(
                    "'mode' must be a string if provided");
                return;
            }

            std::string mode = body.value("mode", std::string{"manual"});
            auto result =
                lithium::middleware::setCoolerPower(deviceId, power, mode);
            res = ResponseBuilder::success(result);
        } catch (const std::exception& e) {
            res = ResponseBuilder::badRequest(std::string("Invalid JSON: ") +
                                              e.what());
        }
    }

    void warmupRoute(const crow::request& req, crow::response& res,
                     const std::string& deviceId) {
        (void)req;
        if (!isValidDeviceId(deviceId)) {
            res = ResponseBuilder::deviceNotFound(deviceId, "Camera");
            return;
        }
        auto result = lithium::middleware::warmUpCamera(deviceId);
        res = ResponseBuilder::accepted(result);
    }
};

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_CAMERA_HPP
