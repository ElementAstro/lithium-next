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
#include "controller.hpp"
#include "server/command/guider.hpp"
#include "server/models/api.hpp"
#include "atom/log/spdlog_logger.hpp"

class GuiderController : public Controller {
private:
    static auto makeJsonResponse(const nlohmann::json &body, int code)
        -> crow::response {
        crow::response res(code);
        res.set_header("Content-Type", "application/json");
        res.write(body.dump());
        return res;
    }

    static auto parseJsonBody(const crow::request &req) -> std::pair<nlohmann::json, bool> {
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
            .methods("POST"_method)([](const crow::request &req,
                                       crow::response &res) {
                auto [body, valid] = parseJsonBody(req);
                if (!valid) {
                    res = makeJsonResponse(
                        lithium::models::api::makeError("invalid_json", "Invalid JSON"),
                        400);
                    return;
                }

                std::string host = body.value("host", "localhost");
                int port = body.value("port", 4400);
                int timeout = body.value("timeout", 5000);

                auto result = lithium::middleware::connectGuider(host, port, timeout);
                res = makeJsonResponse(result, 200);
            });

        // Disconnect from PHD2
        CROW_ROUTE(app, "/api/v1/guider/disconnect")
            .methods("POST"_method)([](const crow::request &,
                                       crow::response &res) {
                auto result = lithium::middleware::disconnectGuider();
                res = makeJsonResponse(result, 200);
            });

        // Get connection status
        CROW_ROUTE(app, "/api/v1/guider/connection")
            .methods("GET"_method)([](const crow::request &,
                                      crow::response &res) {
                auto result = lithium::middleware::getConnectionStatus();
                res = makeJsonResponse(result, 200);
            });

        // ==================== Guiding Control ====================

        // Start guiding
        CROW_ROUTE(app, "/api/v1/guider/start")
            .methods("POST"_method)([](const crow::request &req,
                                       crow::response &res) {
                auto [body, valid] = parseJsonBody(req);
                if (!valid) {
                    res = makeJsonResponse(
                        lithium::models::api::makeError("invalid_json", "Invalid JSON"),
                        400);
                    return;
                }

                double settlePixels = body.value("settlePixels", 1.5);
                double settleTime = body.value("settleTime", 10.0);
                double settleTimeout = body.value("settleTimeout", 60.0);
                bool recalibrate = body.value("recalibrate", false);

                auto result = lithium::middleware::startGuiding(
                    settlePixels, settleTime, settleTimeout, recalibrate);
                res = makeJsonResponse(result, 200);
            });

        // Stop guiding
        CROW_ROUTE(app, "/api/v1/guider/stop")
            .methods("POST"_method)([](const crow::request &,
                                       crow::response &res) {
                auto result = lithium::middleware::stopGuiding();
                res = makeJsonResponse(result, 200);
            });

        // Pause guiding
        CROW_ROUTE(app, "/api/v1/guider/pause")
            .methods("POST"_method)([](const crow::request &req,
                                       crow::response &res) {
                auto [body, valid] = parseJsonBody(req);
                bool full = valid ? body.value("full", false) : false;
                auto result = lithium::middleware::pauseGuiding(full);
                res = makeJsonResponse(result, 200);
            });

        // Resume guiding
        CROW_ROUTE(app, "/api/v1/guider/resume")
            .methods("POST"_method)([](const crow::request &,
                                       crow::response &res) {
                auto result = lithium::middleware::resumeGuiding();
                res = makeJsonResponse(result, 200);
            });

        // Dither
        CROW_ROUTE(app, "/api/v1/guider/dither")
            .methods("POST"_method)([](const crow::request &req,
                                       crow::response &res) {
                auto [body, valid] = parseJsonBody(req);
                if (!valid) {
                    res = makeJsonResponse(
                        lithium::models::api::makeError("invalid_json", "Invalid JSON"),
                        400);
                    return;
                }

                double amount = body.value("amount", 5.0);
                bool raOnly = body.value("raOnly", false);
                double settlePixels = body.value("settlePixels", 1.5);
                double settleTime = body.value("settleTime", 10.0);
                double settleTimeout = body.value("settleTimeout", 60.0);

                auto result = lithium::middleware::ditherGuider(
                    amount, raOnly, settlePixels, settleTime, settleTimeout);
                res = makeJsonResponse(result, 200);
            });

        // Start looping
        CROW_ROUTE(app, "/api/v1/guider/loop")
            .methods("POST"_method)([](const crow::request &,
                                       crow::response &res) {
                auto result = lithium::middleware::loopGuider();
                res = makeJsonResponse(result, 200);
            });

        // Stop capture
        CROW_ROUTE(app, "/api/v1/guider/capture/stop")
            .methods("POST"_method)([](const crow::request &,
                                       crow::response &res) {
                auto result = lithium::middleware::stopCapture();
                res = makeJsonResponse(result, 200);
            });

        // ==================== Status ====================

        // Get status
        CROW_ROUTE(app, "/api/v1/guider/status")
            .methods("GET"_method)([](const crow::request &,
                                      crow::response &res) {
                auto result = lithium::middleware::getGuiderStatus();
                res = makeJsonResponse(result, 200);
            });

        // Get stats
        CROW_ROUTE(app, "/api/v1/guider/stats")
            .methods("GET"_method)([](const crow::request &,
                                      crow::response &res) {
                auto result = lithium::middleware::getGuiderStats();
                res = makeJsonResponse(result, 200);
            });

        // Get current star
        CROW_ROUTE(app, "/api/v1/guider/star")
            .methods("GET"_method)([](const crow::request &,
                                      crow::response &res) {
                auto result = lithium::middleware::getCurrentStar();
                res = makeJsonResponse(result, 200);
            });

        // ==================== Calibration ====================

        // Check calibration
        CROW_ROUTE(app, "/api/v1/guider/calibration")
            .methods("GET"_method)([](const crow::request &,
                                      crow::response &res) {
                auto result = lithium::middleware::getCalibrationData();
                res = makeJsonResponse(result, 200);
            });

        // Clear calibration
        CROW_ROUTE(app, "/api/v1/guider/calibration/clear")
            .methods("POST"_method)([](const crow::request &req,
                                       crow::response &res) {
                auto [body, valid] = parseJsonBody(req);
                std::string which = valid ? body.value("which", "both") : "both";
                auto result = lithium::middleware::clearCalibration(which);
                res = makeJsonResponse(result, 200);
            });

        // Flip calibration
        CROW_ROUTE(app, "/api/v1/guider/calibration/flip")
            .methods("POST"_method)([](const crow::request &,
                                       crow::response &res) {
                auto result = lithium::middleware::flipCalibration();
                res = makeJsonResponse(result, 200);
            });

        // ==================== Star Selection ====================

        // Find star
        CROW_ROUTE(app, "/api/v1/guider/star/find")
            .methods("POST"_method)([](const crow::request &req,
                                       crow::response &res) {
                auto [body, valid] = parseJsonBody(req);

                std::optional<int> roiX, roiY, roiWidth, roiHeight;
                if (valid && body.contains("roi")) {
                    auto roi = body["roi"];
                    if (roi.contains("x")) roiX = roi["x"].get<int>();
                    if (roi.contains("y")) roiY = roi["y"].get<int>();
                    if (roi.contains("width")) roiWidth = roi["width"].get<int>();
                    if (roi.contains("height")) roiHeight = roi["height"].get<int>();
                }

                auto result = lithium::middleware::findStar(roiX, roiY, roiWidth, roiHeight);
                res = makeJsonResponse(result, 200);
            });

        // Set lock position
        CROW_ROUTE(app, "/api/v1/guider/lock")
            .methods("PUT"_method)([](const crow::request &req,
                                      crow::response &res) {
                auto [body, valid] = parseJsonBody(req);
                if (!valid || !body.contains("x") || !body.contains("y")) {
                    res = makeJsonResponse(
                        lithium::models::api::makeError("invalid_params", "x and y required"),
                        400);
                    return;
                }

                double x = body["x"].get<double>();
                double y = body["y"].get<double>();
                bool exact = body.value("exact", true);

                auto result = lithium::middleware::setLockPosition(x, y, exact);
                res = makeJsonResponse(result, 200);
            });

        // Get lock position
        CROW_ROUTE(app, "/api/v1/guider/lock")
            .methods("GET"_method)([](const crow::request &,
                                      crow::response &res) {
                auto result = lithium::middleware::getLockPosition();
                res = makeJsonResponse(result, 200);
            });

        // ==================== Camera Control ====================

        // Get exposure
        CROW_ROUTE(app, "/api/v1/guider/exposure")
            .methods("GET"_method)([](const crow::request &,
                                      crow::response &res) {
                auto result = lithium::middleware::getExposure();
                res = makeJsonResponse(result, 200);
            });

        // Set exposure
        CROW_ROUTE(app, "/api/v1/guider/exposure")
            .methods("PUT"_method)([](const crow::request &req,
                                      crow::response &res) {
                auto [body, valid] = parseJsonBody(req);
                if (!valid || !body.contains("exposureMs")) {
                    res = makeJsonResponse(
                        lithium::models::api::makeError("invalid_params", "exposureMs required"),
                        400);
                    return;
                }

                int exposureMs = body["exposureMs"].get<int>();
                auto result = lithium::middleware::setExposure(exposureMs);
                res = makeJsonResponse(result, 200);
            });

        // Get exposure durations
        CROW_ROUTE(app, "/api/v1/guider/exposure/durations")
            .methods("GET"_method)([](const crow::request &,
                                      crow::response &res) {
                auto result = lithium::middleware::getExposureDurations();
                res = makeJsonResponse(result, 200);
            });

        // Get camera frame size
        CROW_ROUTE(app, "/api/v1/guider/camera/framesize")
            .methods("GET"_method)([](const crow::request &,
                                      crow::response &res) {
                auto result = lithium::middleware::getCameraFrameSize();
                res = makeJsonResponse(result, 200);
            });

        // Get CCD temperature
        CROW_ROUTE(app, "/api/v1/guider/camera/temperature")
            .methods("GET"_method)([](const crow::request &,
                                      crow::response &res) {
                auto result = lithium::middleware::getCcdTemperature();
                res = makeJsonResponse(result, 200);
            });

        // Get cooler status
        CROW_ROUTE(app, "/api/v1/guider/camera/cooler")
            .methods("GET"_method)([](const crow::request &,
                                      crow::response &res) {
                auto result = lithium::middleware::getCoolerStatus();
                res = makeJsonResponse(result, 200);
            });

        // Save current image
        CROW_ROUTE(app, "/api/v1/guider/camera/save")
            .methods("POST"_method)([](const crow::request &,
                                       crow::response &res) {
                auto result = lithium::middleware::saveImage();
                res = makeJsonResponse(result, 200);
            });

        // Get star image
        CROW_ROUTE(app, "/api/v1/guider/camera/starimage")
            .methods("GET"_method)([](const crow::request &req,
                                      crow::response &res) {
                int size = 15;
                if (req.url_params.get("size")) {
                    size = std::stoi(req.url_params.get("size"));
                }
                auto result = lithium::middleware::getStarImage(size);
                res = makeJsonResponse(result, 200);
            });

        // Capture single frame
        CROW_ROUTE(app, "/api/v1/guider/camera/capture")
            .methods("POST"_method)([](const crow::request &req,
                                       crow::response &res) {
                auto [body, valid] = parseJsonBody(req);
                std::optional<int> exposureMs;
                if (valid && body.contains("exposureMs")) {
                    exposureMs = body["exposureMs"].get<int>();
                }
                auto result = lithium::middleware::captureSingleFrame(exposureMs);
                res = makeJsonResponse(result, 200);
            });

        // ==================== Guide Pulse ====================

        // Send guide pulse
        CROW_ROUTE(app, "/api/v1/guider/pulse")
            .methods("POST"_method)([](const crow::request &req,
                                       crow::response &res) {
                auto [body, valid] = parseJsonBody(req);
                if (!valid || !body.contains("direction") || !body.contains("durationMs")) {
                    res = makeJsonResponse(
                        lithium::models::api::makeError("invalid_params",
                            "direction and durationMs required"),
                        400);
                    return;
                }

                std::string direction = body["direction"].get<std::string>();
                int durationMs = body["durationMs"].get<int>();
                bool useAO = body.value("useAO", false);

                auto result = lithium::middleware::guidePulse(direction, durationMs, useAO);
                res = makeJsonResponse(result, 200);
            });

        // ==================== Algorithm Settings ====================

        // Get Dec guide mode
        CROW_ROUTE(app, "/api/v1/guider/decmode")
            .methods("GET"_method)([](const crow::request &,
                                      crow::response &res) {
                auto result = lithium::middleware::getDecGuideMode();
                res = makeJsonResponse(result, 200);
            });

        // Set Dec guide mode
        CROW_ROUTE(app, "/api/v1/guider/decmode")
            .methods("PUT"_method)([](const crow::request &req,
                                      crow::response &res) {
                auto [body, valid] = parseJsonBody(req);
                if (!valid || !body.contains("mode")) {
                    res = makeJsonResponse(
                        lithium::models::api::makeError("invalid_params", "mode required"),
                        400);
                    return;
                }

                std::string mode = body["mode"].get<std::string>();
                auto result = lithium::middleware::setDecGuideMode(mode);
                res = makeJsonResponse(result, 200);
            });

        // Get algorithm parameter
        CROW_ROUTE(app, "/api/v1/guider/algo/<string>/<string>")
            .methods("GET"_method)([](const crow::request &,
                                      crow::response &res,
                                      const std::string &axis,
                                      const std::string &name) {
                auto result = lithium::middleware::getAlgoParam(axis, name);
                res = makeJsonResponse(result, 200);
            });

        // Set algorithm parameter
        CROW_ROUTE(app, "/api/v1/guider/algo/<string>/<string>")
            .methods("PUT"_method)([](const crow::request &req,
                                      crow::response &res,
                                      const std::string &axis,
                                      const std::string &name) {
                auto [body, valid] = parseJsonBody(req);
                if (!valid || !body.contains("value")) {
                    res = makeJsonResponse(
                        lithium::models::api::makeError("invalid_params", "value required"),
                        400);
                    return;
                }

                double value = body["value"].get<double>();
                auto result = lithium::middleware::setAlgoParam(axis, name, value);
                res = makeJsonResponse(result, 200);
            });

        // ==================== Equipment ====================

        // Get equipment status
        CROW_ROUTE(app, "/api/v1/guider/equipment")
            .methods("GET"_method)([](const crow::request &,
                                      crow::response &res) {
                auto result = lithium::middleware::getEquipmentInfo();
                res = makeJsonResponse(result, 200);
            });

        // Connect equipment
        CROW_ROUTE(app, "/api/v1/guider/equipment/connect")
            .methods("POST"_method)([](const crow::request &,
                                       crow::response &res) {
                auto result = lithium::middleware::connectEquipment();
                res = makeJsonResponse(result, 200);
            });

        // Disconnect equipment
        CROW_ROUTE(app, "/api/v1/guider/equipment/disconnect")
            .methods("POST"_method)([](const crow::request &,
                                       crow::response &res) {
                auto result = lithium::middleware::disconnectEquipment();
                res = makeJsonResponse(result, 200);
            });

        // ==================== Profile Management ====================

        // Get profiles
        CROW_ROUTE(app, "/api/v1/guider/profiles")
            .methods("GET"_method)([](const crow::request &,
                                      crow::response &res) {
                auto result = lithium::middleware::getProfiles();
                res = makeJsonResponse(result, 200);
            });

        // Get current profile
        CROW_ROUTE(app, "/api/v1/guider/profile")
            .methods("GET"_method)([](const crow::request &,
                                      crow::response &res) {
                auto result = lithium::middleware::getCurrentProfile();
                res = makeJsonResponse(result, 200);
            });

        // Set profile
        CROW_ROUTE(app, "/api/v1/guider/profile")
            .methods("PUT"_method)([](const crow::request &req,
                                      crow::response &res) {
                auto [body, valid] = parseJsonBody(req);
                if (!valid || !body.contains("profileId")) {
                    res = makeJsonResponse(
                        lithium::models::api::makeError("invalid_params", "profileId required"),
                        400);
                    return;
                }

                int profileId = body["profileId"].get<int>();
                auto result = lithium::middleware::setProfile(profileId);
                res = makeJsonResponse(result, 200);
            });

        // ==================== Settings ====================

        // Update settings
        CROW_ROUTE(app, "/api/v1/guider/settings")
            .methods("PUT"_method)([](const crow::request &req,
                                      crow::response &res) {
                auto [body, valid] = parseJsonBody(req);
                if (!valid) {
                    res = makeJsonResponse(
                        lithium::models::api::makeError("invalid_json", "Invalid JSON"),
                        400);
                    return;
                }

                auto result = lithium::middleware::setGuiderSettings(body);
                res = makeJsonResponse(result, 200);
            });

        // ==================== Lock Shift ====================

        // Get lock shift status
        CROW_ROUTE(app, "/api/v1/guider/lockshift")
            .methods("GET"_method)([](const crow::request &,
                                      crow::response &res) {
                auto result = lithium::middleware::isLockShiftEnabled();
                res = makeJsonResponse(result, 200);
            });

        // Set lock shift
        CROW_ROUTE(app, "/api/v1/guider/lockshift")
            .methods("PUT"_method)([](const crow::request &req,
                                      crow::response &res) {
                auto [body, valid] = parseJsonBody(req);
                if (!valid || !body.contains("enabled")) {
                    res = makeJsonResponse(
                        lithium::models::api::makeError("invalid_params", "enabled required"),
                        400);
                    return;
                }

                bool enabled = body["enabled"].get<bool>();
                auto result = lithium::middleware::setLockShiftEnabled(enabled);
                res = makeJsonResponse(result, 200);
            });

        // ==================== Shutdown ====================

        // Shutdown guider
        CROW_ROUTE(app, "/api/v1/guider/shutdown")
            .methods("POST"_method)([](const crow::request &,
                                       crow::response &res) {
                auto result = lithium::middleware::shutdownGuider();
                res = makeJsonResponse(result, 200);
            });
    }
};

#endif  // LITHIUM_SERVER_CONTROLLER_GUIDER_HPP
