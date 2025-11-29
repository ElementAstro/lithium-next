/*
 * guider.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: Guider REST API Controller

*************************************************/

#ifndef LITHIUM_SERVER_CONTROLLER_GUIDER_HPP
#define LITHIUM_SERVER_CONTROLLER_GUIDER_HPP

#include <crow/json.h>
#include "atom/log/spdlog_logger.hpp"
#include "controller.hpp"
#include "server/command/guider.hpp"
#include "server/utils/response.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

class GuiderController : public Controller {
private:
    static auto parseJsonBody(const crow::request &req)
        -> std::pair<nlohmann::json, bool> {
        try {
            if (req.body.empty()) {
                return {nlohmann::json::object(), true};
            }
            return {nlohmann::json::parse(req.body), true};
        } catch (...) {
            return {nlohmann::json(), false};
        }
    }

public:
    void registerRoutes(lithium::server::ServerApp &app) override {
        // ==================== Connection ====================

        // Connect to PHD2
        CROW_ROUTE(app, "/api/v1/guider/connect")
            .methods("POST"_method)(
                [this](const crow::request &req, crow::response &res) {
                    auto [body, valid] = parseJsonBody(req);
                    if (!valid) {
                        res = ResponseBuilder::badRequest("Invalid JSON");
                        return;
                    }

                    std::string host = body.value("host", "localhost");
                    int port = body.value("port", 4400);
                    int timeout = body.value("timeout", 5000);

                    auto result =
                        lithium::middleware::connectGuider(host, port, timeout);
                    res = ResponseBuilder::success(result);
                });

        // Disconnect from PHD2
        CROW_ROUTE(app, "/api/v1/guider/disconnect")
            .methods("POST"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::disconnectGuider();
                    res = ResponseBuilder::success(result);
                });

        // Get connection status
        CROW_ROUTE(app, "/api/v1/guider/connection")
            .methods("GET"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::getConnectionStatus();
                    res = ResponseBuilder::success(result);
                });

        // ==================== Guiding Control ====================

        // Start guiding
        CROW_ROUTE(app, "/api/v1/guider/start")
            .methods("POST"_method)(
                [](const crow::request &req, crow::response &res) {
                    auto [body, valid] = parseJsonBody(req);
                    if (!valid) {
                        res = ResponseBuilder::badRequest("Invalid JSON");
                        return;
                    }

                    double settlePixels = body.value("settlePixels", 1.5);
                    double settleTime = body.value("settleTime", 10.0);
                    double settleTimeout = body.value("settleTimeout", 60.0);
                    bool recalibrate = body.value("recalibrate", false);

                    auto result = lithium::middleware::startGuiding(
                        settlePixels, settleTime, settleTimeout, recalibrate);
                    res = ResponseBuilder::success(result);
                });

        // Stop guiding
        CROW_ROUTE(app, "/api/v1/guider/stop")
            .methods("POST"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::stopGuiding();
                    res = ResponseBuilder::success(result);
                });

        // Pause guiding
        CROW_ROUTE(app, "/api/v1/guider/pause")
            .methods("POST"_method)(
                [](const crow::request &req, crow::response &res) {
                    auto [body, valid] = parseJsonBody(req);
                    bool full = valid ? body.value("full", false) : false;
                    auto result = lithium::middleware::pauseGuiding(full);
                    res = ResponseBuilder::success(result);
                });

        // Resume guiding
        CROW_ROUTE(app, "/api/v1/guider/resume")
            .methods("POST"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::resumeGuiding();
                    res = ResponseBuilder::success(result);
                });

        // Dither
        CROW_ROUTE(app, "/api/v1/guider/dither")
            .methods("POST"_method)([](const crow::request &req,
                                       crow::response &res) {
                auto [body, valid] = parseJsonBody(req);
                if (!valid) {
                    res = ResponseBuilder::badRequest("Invalid JSON");
                    return;
                }

                double amount = body.value("amount", 5.0);
                bool raOnly = body.value("raOnly", false);
                double settlePixels = body.value("settlePixels", 1.5);
                double settleTime = body.value("settleTime", 10.0);
                double settleTimeout = body.value("settleTimeout", 60.0);

                auto result = lithium::middleware::ditherGuider(
                    amount, raOnly, settlePixels, settleTime, settleTimeout);
                res = ResponseBuilder::success(result);
            });

        // Start looping
        CROW_ROUTE(app, "/api/v1/guider/loop")
            .methods("POST"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::loopGuider();
                    res = ResponseBuilder::success(result);
                });

        // Stop capture
        CROW_ROUTE(app, "/api/v1/guider/capture/stop")
            .methods("POST"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::stopCapture();
                    res = ResponseBuilder::success(result);
                });

        // ==================== Status ====================

        // Get status
        CROW_ROUTE(app, "/api/v1/guider/status")
            .methods("GET"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::getGuiderStatus();
                    res = ResponseBuilder::success(result);
                });

        // Get stats
        CROW_ROUTE(app, "/api/v1/guider/stats")
            .methods("GET"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::getGuiderStats();
                    res = ResponseBuilder::success(result);
                });

        // Get current star
        CROW_ROUTE(app, "/api/v1/guider/star")
            .methods("GET"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::getCurrentStar();
                    res = ResponseBuilder::success(result);
                });

        // ==================== Calibration ====================

        // Check calibration
        CROW_ROUTE(app, "/api/v1/guider/calibration")
            .methods("GET"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::getCalibrationData();
                    res = ResponseBuilder::success(result);
                });

        // Clear calibration
        CROW_ROUTE(app, "/api/v1/guider/calibration/clear")
            .methods("POST"_method)(
                [](const crow::request &req, crow::response &res) {
                    auto [body, valid] = parseJsonBody(req);
                    std::string which =
                        valid ? body.value("which", "both") : "both";
                    auto result = lithium::middleware::clearCalibration(which);
                    res = ResponseBuilder::success(result);
                });

        // Flip calibration
        CROW_ROUTE(app, "/api/v1/guider/calibration/flip")
            .methods("POST"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::flipCalibration();
                    res = ResponseBuilder::success(result);
                });

        // ==================== Star Selection ====================

        // Find star
        CROW_ROUTE(app, "/api/v1/guider/star/find")
            .methods("POST"_method)(
                [](const crow::request &req, crow::response &res) {
                    auto [body, valid] = parseJsonBody(req);

                    std::optional<int> roiX, roiY, roiWidth, roiHeight;
                    if (valid && body.contains("roi")) {
                        auto roi = body["roi"];
                        if (roi.contains("x"))
                            roiX = roi["x"].get<int>();
                        if (roi.contains("y"))
                            roiY = roi["y"].get<int>();
                        if (roi.contains("width"))
                            roiWidth = roi["width"].get<int>();
                        if (roi.contains("height"))
                            roiHeight = roi["height"].get<int>();
                    }

                    auto result = lithium::middleware::findStar(
                        roiX, roiY, roiWidth, roiHeight);
                    res = ResponseBuilder::success(result);
                });

        // Set lock position
        CROW_ROUTE(app, "/api/v1/guider/lock")
            .methods("PUT"_method)([](const crow::request &req,
                                      crow::response &res) {
                auto [body, valid] = parseJsonBody(req);
                if (!valid || !body.contains("x") || !body.contains("y")) {
                    res = ResponseBuilder::badRequest("x and y required");
                    return;
                }

                double x = body["x"].get<double>();
                double y = body["y"].get<double>();
                bool exact = body.value("exact", true);

                auto result = lithium::middleware::setLockPosition(x, y, exact);
                res = ResponseBuilder::success(result);
            });

        // Get lock position
        CROW_ROUTE(app, "/api/v1/guider/lock")
            .methods("GET"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::getLockPosition();
                    res = ResponseBuilder::success(result);
                });

        // ==================== Camera Control ====================

        // Get exposure
        CROW_ROUTE(app, "/api/v1/guider/exposure")
            .methods("GET"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::getExposure();
                    res = ResponseBuilder::success(result);
                });

        // Set exposure
        CROW_ROUTE(app, "/api/v1/guider/exposure")
            .methods("PUT"_method)([](const crow::request &req,
                                      crow::response &res) {
                auto [body, valid] = parseJsonBody(req);
                if (!valid || !body.contains("exposureMs")) {
                    res = ResponseBuilder::badRequest("exposureMs required");
                    return;
                }

                int exposureMs = body["exposureMs"].get<int>();
                auto result = lithium::middleware::setExposure(exposureMs);
                res = ResponseBuilder::success(result);
            });

        // Get exposure durations
        CROW_ROUTE(app, "/api/v1/guider/exposure/durations")
            .methods("GET"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::getExposureDurations();
                    res = ResponseBuilder::success(result);
                });

        // Get camera frame size
        CROW_ROUTE(app, "/api/v1/guider/camera/framesize")
            .methods("GET"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::getCameraFrameSize();
                    res = ResponseBuilder::success(result);
                });

        // Get CCD temperature
        CROW_ROUTE(app, "/api/v1/guider/camera/temperature")
            .methods("GET"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::getCcdTemperature();
                    res = ResponseBuilder::success(result);
                });

        // Get cooler status
        CROW_ROUTE(app, "/api/v1/guider/camera/cooler")
            .methods("GET"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::getCoolerStatus();
                    res = ResponseBuilder::success(result);
                });

        // Save current image
        CROW_ROUTE(app, "/api/v1/guider/camera/save")
            .methods("POST"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::saveImage();
                    res = ResponseBuilder::success(result);
                });

        // Get star image
        CROW_ROUTE(app, "/api/v1/guider/camera/starimage")
            .methods("GET"_method)(
                [](const crow::request &req, crow::response &res) {
                    int size = 15;
                    if (req.url_params.get("size")) {
                        size = std::stoi(req.url_params.get("size"));
                    }
                    auto result = lithium::middleware::getStarImage(size);
                    res = ResponseBuilder::success(result);
                });

        // Capture single frame
        CROW_ROUTE(app, "/api/v1/guider/camera/capture")
            .methods("POST"_method)(
                [](const crow::request &req, crow::response &res) {
                    auto [body, valid] = parseJsonBody(req);
                    std::optional<int> exposureMs;
                    if (valid && body.contains("exposureMs")) {
                        exposureMs = body["exposureMs"].get<int>();
                    }
                    auto result =
                        lithium::middleware::captureSingleFrame(exposureMs);
                    res = ResponseBuilder::success(result);
                });

        // ==================== Guide Pulse ====================

        // Send guide pulse
        CROW_ROUTE(app, "/api/v1/guider/pulse")
            .methods("POST"_method)([](const crow::request &req,
                                       crow::response &res) {
                auto [body, valid] = parseJsonBody(req);
                if (!valid || !body.contains("direction") ||
                    !body.contains("durationMs")) {
                    res = ResponseBuilder::badRequest(
                        "direction and durationMs required");
                    return;
                }

                std::string direction = body["direction"].get<std::string>();
                int durationMs = body["durationMs"].get<int>();
                bool useAO = body.value("useAO", false);

                auto result = lithium::middleware::guidePulse(
                    direction, durationMs, useAO);
                res = ResponseBuilder::success(result);
            });

        // ==================== Algorithm Settings ====================

        // Get Dec guide mode
        CROW_ROUTE(app, "/api/v1/guider/decmode")
            .methods("GET"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::getDecGuideMode();
                    res = ResponseBuilder::success(result);
                });

        // Set Dec guide mode
        CROW_ROUTE(app, "/api/v1/guider/decmode")
            .methods("PUT"_method)(
                [](const crow::request &req, crow::response &res) {
                    auto [body, valid] = parseJsonBody(req);
                    if (!valid || !body.contains("mode")) {
                        res = ResponseBuilder::badRequest("mode required");
                        return;
                    }

                    std::string mode = body["mode"].get<std::string>();
                    auto result = lithium::middleware::setDecGuideMode(mode);
                    res = ResponseBuilder::success(result);
                });

        // Get algorithm parameter
        CROW_ROUTE(app, "/api/v1/guider/algo/<string>/<string>")
            .methods("GET"_method)(
                [](const crow::request &, crow::response &res,
                   const std::string &axis, const std::string &name) {
                    auto result = lithium::middleware::getAlgoParam(axis, name);
                    res = ResponseBuilder::success(result);
                });

        // Set algorithm parameter
        CROW_ROUTE(app, "/api/v1/guider/algo/<string>/<string>")
            .methods("PUT"_method)(
                [](const crow::request &req, crow::response &res,
                   const std::string &axis, const std::string &name) {
                    auto [body, valid] = parseJsonBody(req);
                    if (!valid || !body.contains("value")) {
                        res = ResponseBuilder::badRequest("value required");
                        return;
                    }

                    double value = body["value"].get<double>();
                    auto result =
                        lithium::middleware::setAlgoParam(axis, name, value);
                    res = ResponseBuilder::success(result);
                });

        // ==================== Equipment ====================

        // Get equipment status
        CROW_ROUTE(app, "/api/v1/guider/equipment")
            .methods("GET"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::getEquipmentInfo();
                    res = ResponseBuilder::success(result);
                });

        // Connect equipment
        CROW_ROUTE(app, "/api/v1/guider/equipment/connect")
            .methods("POST"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::connectEquipment();
                    res = ResponseBuilder::success(result);
                });

        // Disconnect equipment
        CROW_ROUTE(app, "/api/v1/guider/equipment/disconnect")
            .methods("POST"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::disconnectEquipment();
                    res = ResponseBuilder::success(result);
                });

        // ==================== Profile Management ====================

        // Get profiles
        CROW_ROUTE(app, "/api/v1/guider/profiles")
            .methods("GET"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::getProfiles();
                    res = ResponseBuilder::success(result);
                });

        // Get current profile
        CROW_ROUTE(app, "/api/v1/guider/profile")
            .methods("GET"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::getCurrentProfile();
                    res = ResponseBuilder::success(result);
                });

        // Set profile
        CROW_ROUTE(app, "/api/v1/guider/profile")
            .methods("PUT"_method)(
                [](const crow::request &req, crow::response &res) {
                    auto [body, valid] = parseJsonBody(req);
                    if (!valid || !body.contains("profileId")) {
                        res = ResponseBuilder::badRequest("profileId required");
                        return;
                    }

                    int profileId = body["profileId"].get<int>();
                    auto result = lithium::middleware::setProfile(profileId);
                    res = ResponseBuilder::success(result);
                });

        // ==================== Settings ====================

        // Update settings
        CROW_ROUTE(app, "/api/v1/guider/settings")
            .methods("PUT"_method)(
                [](const crow::request &req, crow::response &res) {
                    auto [body, valid] = parseJsonBody(req);
                    if (!valid) {
                        res = ResponseBuilder::badRequest("Invalid JSON");
                        return;
                    }

                    auto result = lithium::middleware::setGuiderSettings(body);
                    res = ResponseBuilder::success(result);
                });

        // ==================== Lock Shift ====================

        // Get lock shift status
        CROW_ROUTE(app, "/api/v1/guider/lockshift")
            .methods("GET"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::isLockShiftEnabled();
                    res = ResponseBuilder::success(result);
                });

        // Set lock shift
        CROW_ROUTE(app, "/api/v1/guider/lockshift")
            .methods("PUT"_method)([](const crow::request &req,
                                      crow::response &res) {
                auto [body, valid] = parseJsonBody(req);
                if (!valid || !body.contains("enabled")) {
                    res = ResponseBuilder::badRequest("enabled required");
                    return;
                }

                bool enabled = body["enabled"].get<bool>();
                auto result = lithium::middleware::setLockShiftEnabled(enabled);
                res = ResponseBuilder::success(result);
            });

        // ==================== Shutdown ====================

        // Shutdown guider
        CROW_ROUTE(app, "/api/v1/guider/shutdown")
            .methods("POST"_method)(
                [](const crow::request &, crow::response &res) {
                    auto result = lithium::middleware::shutdownGuider();
                    res = ResponseBuilder::success(result);
                });
    }
};

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_GUIDER_HPP
