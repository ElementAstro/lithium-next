/*
 * guider.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "guider.hpp"

#include "../command.hpp"
#include "../response.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "device/service/guider_service.hpp"

namespace lithium::middleware {

using json = nlohmann::json;

// Global guider service instance
static std::shared_ptr<lithium::device::GuiderService> g_guiderService;

auto getGuiderService() -> std::shared_ptr<lithium::device::GuiderService> {
    if (!g_guiderService) {
        g_guiderService = std::make_shared<lithium::device::GuiderService>();
    }
    return g_guiderService;
}

// ==================== Connection ====================

auto connectGuider(const std::string& host, int port, int timeout) -> json {
    LOG_INFO("connectGuider: host={} port={} timeout={}", host, port, timeout);
    return getGuiderService()->connect(host, port, timeout);
}

auto disconnectGuider() -> json {
    LOG_INFO("disconnectGuider");
    return getGuiderService()->disconnect();
}

auto getConnectionStatus() -> json {
    return getGuiderService()->getConnectionStatus();
}

// ==================== Guiding Control ====================

auto startGuiding(double settlePixels, double settleTime, double settleTimeout,
                  bool recalibrate) -> json {
    LOG_INFO(
        "startGuiding: settlePixels={} settleTime={} settleTimeout={} "
        "recalibrate={}",
        settlePixels, settleTime, settleTimeout, recalibrate);
    return getGuiderService()->startGuiding(settlePixels, settleTime,
                                            settleTimeout, recalibrate);
}

auto stopGuiding() -> json {
    LOG_INFO("stopGuiding");
    return getGuiderService()->stopGuiding();
}

auto pauseGuiding(bool full) -> json {
    LOG_INFO("pauseGuiding: full={}", full);
    return getGuiderService()->pause(full);
}

auto resumeGuiding() -> json {
    LOG_INFO("resumeGuiding");
    return getGuiderService()->resume();
}

auto ditherGuider(double amount, bool raOnly, double settlePixels,
                  double settleTime, double settleTimeout) -> json {
    LOG_INFO("ditherGuider: amount={} raOnly={}", amount, raOnly);
    return getGuiderService()->dither(amount, raOnly, settlePixels, settleTime,
                                      settleTimeout);
}

auto loopGuider() -> json {
    LOG_INFO("loopGuider");
    return getGuiderService()->loop();
}

auto stopCapture() -> json {
    LOG_INFO("stopCapture");
    return getGuiderService()->stopCapture();
}

// ==================== Status ====================

auto getGuiderStatus() -> json { return getGuiderService()->getStatus(); }

auto getGuiderStats() -> json { return getGuiderService()->getStats(); }

auto getCurrentStar() -> json { return getGuiderService()->getCurrentStar(); }

// ==================== Calibration ====================

auto isCalibrated() -> json { return getGuiderService()->isCalibrated(); }

auto clearCalibration(const std::string& which) -> json {
    LOG_INFO("clearCalibration: which={}", which);
    return getGuiderService()->clearCalibration(which);
}

auto flipCalibration() -> json {
    LOG_INFO("flipCalibration");
    return getGuiderService()->flipCalibration();
}

auto getCalibrationData() -> json {
    return getGuiderService()->getCalibrationData();
}

// ==================== Star Selection ====================

auto findStar(std::optional<int> roiX, std::optional<int> roiY,
              std::optional<int> roiWidth, std::optional<int> roiHeight)
    -> json {
    LOG_INFO("findStar");
    return getGuiderService()->findStar(roiX, roiY, roiWidth, roiHeight);
}

auto setLockPosition(double x, double y, bool exact) -> json {
    LOG_INFO("setLockPosition: x={} y={} exact={}", x, y, exact);
    return getGuiderService()->setLockPosition(x, y, exact);
}

auto getLockPosition() -> json { return getGuiderService()->getLockPosition(); }

// ==================== Camera Control ====================

auto getExposure() -> json { return getGuiderService()->getExposure(); }

auto setExposure(int exposureMs) -> json {
    LOG_INFO("setExposure: exposureMs={}", exposureMs);
    return getGuiderService()->setExposure(exposureMs);
}

auto getExposureDurations() -> json {
    return getGuiderService()->getExposureDurations();
}

auto getCameraFrameSize() -> json {
    return getGuiderService()->getCameraFrameSize();
}

auto getCcdTemperature() -> json {
    return getGuiderService()->getCcdTemperature();
}

auto getCoolerStatus() -> json { return getGuiderService()->getCoolerStatus(); }

auto saveImage() -> json {
    LOG_INFO("saveImage");
    return getGuiderService()->saveImage();
}

auto getStarImage(int size) -> json {
    return getGuiderService()->getStarImage(size);
}

auto captureSingleFrame(std::optional<int> exposureMs) -> json {
    LOG_INFO("captureSingleFrame");
    return getGuiderService()->captureSingleFrame(exposureMs);
}

// ==================== Guide Pulse ====================

auto guidePulse(const std::string& direction, int durationMs, bool useAO)
    -> json {
    LOG_INFO("guidePulse: direction={} durationMs={} useAO={}", direction,
             durationMs, useAO);
    return getGuiderService()->guidePulse(direction, durationMs, useAO);
}

// ==================== Algorithm Settings ====================

auto getDecGuideMode() -> json { return getGuiderService()->getDecGuideMode(); }

auto setDecGuideMode(const std::string& mode) -> json {
    LOG_INFO("setDecGuideMode: mode={}", mode);
    return getGuiderService()->setDecGuideMode(mode);
}

auto getAlgoParam(const std::string& axis, const std::string& name) -> json {
    return getGuiderService()->getAlgoParam(axis, name);
}

auto setAlgoParam(const std::string& axis, const std::string& name,
                  double value) -> json {
    LOG_INFO("setAlgoParam: axis={} name={} value={}", axis, name, value);
    return getGuiderService()->setAlgoParam(axis, name, value);
}

// ==================== Equipment ====================

auto isEquipmentConnected() -> json {
    return getGuiderService()->isEquipmentConnected();
}

auto connectEquipment() -> json {
    LOG_INFO("connectEquipment");
    return getGuiderService()->connectEquipment();
}

auto disconnectEquipment() -> json {
    LOG_INFO("disconnectEquipment");
    return getGuiderService()->disconnectEquipment();
}

auto getEquipmentInfo() -> json {
    return getGuiderService()->getEquipmentInfo();
}

// ==================== Profile Management ====================

auto getProfiles() -> json { return getGuiderService()->getProfiles(); }

auto getCurrentProfile() -> json {
    return getGuiderService()->getCurrentProfile();
}

auto setProfile(int profileId) -> json {
    LOG_INFO("setProfile: profileId={}", profileId);
    return getGuiderService()->setProfile(profileId);
}

// ==================== Settings ====================

auto setGuiderSettings(const json& settings) -> json {
    LOG_INFO("setGuiderSettings");
    return getGuiderService()->updateSettings(settings);
}

// ==================== Lock Shift ====================

auto isLockShiftEnabled() -> json {
    return getGuiderService()->isLockShiftEnabled();
}

auto setLockShiftEnabled(bool enable) -> json {
    LOG_INFO("setLockShiftEnabled: enable={}", enable);
    return getGuiderService()->setLockShiftEnabled(enable);
}

// ==================== Shutdown ====================

auto shutdownGuider() -> json {
    LOG_INFO("shutdownGuider");
    return getGuiderService()->shutdown();
}

}  // namespace lithium::middleware

// ==================== Command Registration ====================

namespace lithium::app {

using json = nlohmann::json;
using command::CommandResponse;
using namespace lithium::middleware;

void registerGuider(std::shared_ptr<CommandDispatcher> dispatcher) {
    // guider.connect - Connect to PHD2
    dispatcher->registerCommand<json>("guider.connect", [](json& payload) {
        LOG_INFO("Executing guider.connect");
        try {
            std::string host = payload.value("host", std::string("localhost"));
            int port = payload.value("port", 4400);
            int timeout = payload.value("timeout", 5000);

            auto result = connectGuider(host, port, timeout);
            if (result.contains("status") && result["status"] == "error") {
                payload = result;
            } else {
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("guider.connect exception: {}", e.what());
            payload = CommandResponse::operationFailed("connect", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'guider.connect'");

    // guider.disconnect - Disconnect from PHD2
    dispatcher->registerCommand<json>("guider.disconnect", [](json& payload) {
        LOG_INFO("Executing guider.disconnect");
        try {
            auto result = disconnectGuider();
            if (result.contains("status") && result["status"] == "error") {
                payload = result;
            } else {
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("guider.disconnect exception: {}", e.what());
            payload = CommandResponse::operationFailed("disconnect", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'guider.disconnect'");

    // guider.status - Get guider status
    dispatcher->registerCommand<json>("guider.status", [](json& payload) {
        LOG_DEBUG("Executing guider.status");
        try {
            auto result = getGuiderStatus();
            if (result.contains("status") && result["status"] == "error") {
                payload = result;
            } else {
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("guider.status exception: {}", e.what());
            payload = CommandResponse::operationFailed("status", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'guider.status'");

    // guider.connection_status - Get connection status
    dispatcher->registerCommand<json>(
        "guider.connection_status", [](json& payload) {
            LOG_DEBUG("Executing guider.connection_status");
            try {
                auto result = getConnectionStatus();
                payload = CommandResponse::success(result);
            } catch (const std::exception& e) {
                LOG_ERROR("guider.connection_status exception: {}", e.what());
                payload = CommandResponse::operationFailed("connection_status",
                                                           e.what());
            }
        });
    LOG_INFO("Registered command handler for 'guider.connection_status'");

    // guider.start_guiding - Start guiding
    dispatcher->registerCommand<json>(
        "guider.start_guiding", [](json& payload) {
            LOG_INFO("Executing guider.start_guiding");
            try {
                double settlePixels = payload.value("settlePixels", 1.5);
                double settleTime = payload.value("settleTime", 10.0);
                double settleTimeout = payload.value("settleTimeout", 60.0);
                bool recalibrate = payload.value("recalibrate", false);

                auto result = startGuiding(settlePixels, settleTime,
                                           settleTimeout, recalibrate);
                if (result.contains("status") && result["status"] == "error") {
                    payload = result;
                } else {
                    LOG_INFO("guider.start_guiding completed successfully");
                    payload = CommandResponse::success(result);
                }
            } catch (const std::exception& e) {
                LOG_ERROR("guider.start_guiding exception: {}", e.what());
                payload =
                    CommandResponse::operationFailed("start_guiding", e.what());
            }
        });
    LOG_INFO("Registered command handler for 'guider.start_guiding'");

    // guider.stop_guiding - Stop guiding
    dispatcher->registerCommand<json>("guider.stop_guiding", [](json& payload) {
        LOG_INFO("Executing guider.stop_guiding");
        try {
            auto result = stopGuiding();
            if (result.contains("status") && result["status"] == "error") {
                payload = result;
            } else {
                LOG_INFO("guider.stop_guiding completed successfully");
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("guider.stop_guiding exception: {}", e.what());
            payload =
                CommandResponse::operationFailed("stop_guiding", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'guider.stop_guiding'");

    // guider.pause - Pause guiding
    dispatcher->registerCommand<json>("guider.pause", [](json& payload) {
        LOG_INFO("Executing guider.pause");
        try {
            bool full = payload.value("full", false);
            auto result = pauseGuiding(full);
            if (result.contains("status") && result["status"] == "error") {
                payload = result;
            } else {
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("guider.pause exception: {}", e.what());
            payload = CommandResponse::operationFailed("pause", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'guider.pause'");

    // guider.resume - Resume guiding
    dispatcher->registerCommand<json>("guider.resume", [](json& payload) {
        LOG_INFO("Executing guider.resume");
        try {
            auto result = resumeGuiding();
            if (result.contains("status") && result["status"] == "error") {
                payload = result;
            } else {
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("guider.resume exception: {}", e.what());
            payload = CommandResponse::operationFailed("resume", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'guider.resume'");

    // guider.dither - Perform dither
    dispatcher->registerCommand<json>("guider.dither", [](json& payload) {
        LOG_INFO("Executing guider.dither");
        try {
            double amount = payload.value("amount", 5.0);
            bool raOnly = payload.value("raOnly", false);
            double settlePixels = payload.value("settlePixels", 1.5);
            double settleTime = payload.value("settleTime", 10.0);
            double settleTimeout = payload.value("settleTimeout", 60.0);

            auto result = ditherGuider(amount, raOnly, settlePixels, settleTime,
                                       settleTimeout);
            if (result.contains("status") && result["status"] == "error") {
                payload = result;
            } else {
                LOG_INFO("guider.dither completed successfully");
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("guider.dither exception: {}", e.what());
            payload = CommandResponse::operationFailed("dither", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'guider.dither'");

    // guider.calibrate - Check/clear calibration
    dispatcher->registerCommand<json>("guider.calibrate", [](json& payload) {
        LOG_INFO("Executing guider.calibrate");
        try {
            std::string action = payload.value("action", std::string("status"));
            json result;

            if (action == "status") {
                result = isCalibrated();
            } else if (action == "clear") {
                std::string which = payload.value("which", std::string("both"));
                result = clearCalibration(which);
            } else if (action == "flip") {
                result = flipCalibration();
            } else if (action == "data") {
                result = getCalibrationData();
            } else {
                payload = CommandResponse::invalidParameter(
                    "action", "must be one of: status, clear, flip, data");
                return;
            }

            if (result.contains("status") && result["status"] == "error") {
                payload = result;
            } else {
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("guider.calibrate exception: {}", e.what());
            payload = CommandResponse::operationFailed("calibrate", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'guider.calibrate'");

    // guider.find_star - Auto-select guide star
    dispatcher->registerCommand<json>("guider.find_star", [](json& payload) {
        LOG_INFO("Executing guider.find_star");
        try {
            std::optional<int> roiX, roiY, roiWidth, roiHeight;
            if (payload.contains("roiX"))
                roiX = payload["roiX"].get<int>();
            if (payload.contains("roiY"))
                roiY = payload["roiY"].get<int>();
            if (payload.contains("roiWidth"))
                roiWidth = payload["roiWidth"].get<int>();
            if (payload.contains("roiHeight"))
                roiHeight = payload["roiHeight"].get<int>();

            auto result = findStar(roiX, roiY, roiWidth, roiHeight);
            if (result.contains("status") && result["status"] == "error") {
                payload = result;
            } else {
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("guider.find_star exception: {}", e.what());
            payload = CommandResponse::operationFailed("find_star", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'guider.find_star'");

    // guider.set_lock_position - Set lock position
    dispatcher->registerCommand<json>(
        "guider.set_lock_position", [](json& payload) {
            LOG_INFO("Executing guider.set_lock_position");
            try {
                if (!payload.contains("x") || !payload.contains("y")) {
                    payload = CommandResponse::missingParameter("x and y");
                    return;
                }
                double x = payload["x"].get<double>();
                double y = payload["y"].get<double>();
                bool exact = payload.value("exact", true);

                auto result = setLockPosition(x, y, exact);
                if (result.contains("status") && result["status"] == "error") {
                    payload = result;
                } else {
                    payload = CommandResponse::success(result);
                }
            } catch (const std::exception& e) {
                LOG_ERROR("guider.set_lock_position exception: {}", e.what());
                payload = CommandResponse::operationFailed("set_lock_position",
                                                           e.what());
            }
        });
    LOG_INFO("Registered command handler for 'guider.set_lock_position'");

    // guider.get_lock_position - Get lock position
    dispatcher->registerCommand<json>(
        "guider.get_lock_position", [](json& payload) {
            LOG_DEBUG("Executing guider.get_lock_position");
            try {
                auto result = getLockPosition();
                payload = CommandResponse::success(result);
            } catch (const std::exception& e) {
                LOG_ERROR("guider.get_lock_position exception: {}", e.what());
                payload = CommandResponse::operationFailed("get_lock_position",
                                                           e.what());
            }
        });
    LOG_INFO("Registered command handler for 'guider.get_lock_position'");

    // guider.exposure - Get/set exposure
    dispatcher->registerCommand<json>("guider.exposure", [](json& payload) {
        LOG_DEBUG("Executing guider.exposure");
        try {
            if (payload.contains("exposureMs")) {
                int exposureMs = payload["exposureMs"].get<int>();
                auto result = setExposure(exposureMs);
                if (result.contains("status") && result["status"] == "error") {
                    payload = result;
                } else {
                    payload = CommandResponse::success(result);
                }
            } else {
                auto result = getExposure();
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("guider.exposure exception: {}", e.what());
            payload = CommandResponse::operationFailed("exposure", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'guider.exposure'");

    // guider.stats - Get guide statistics
    dispatcher->registerCommand<json>("guider.stats", [](json& payload) {
        LOG_DEBUG("Executing guider.stats");
        try {
            auto result = getGuiderStats();
            payload = CommandResponse::success(result);
        } catch (const std::exception& e) {
            LOG_ERROR("guider.stats exception: {}", e.what());
            payload = CommandResponse::operationFailed("stats", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'guider.stats'");

    // guider.current_star - Get current star info
    dispatcher->registerCommand<json>("guider.current_star", [](json& payload) {
        LOG_DEBUG("Executing guider.current_star");
        try {
            auto result = getCurrentStar();
            payload = CommandResponse::success(result);
        } catch (const std::exception& e) {
            LOG_ERROR("guider.current_star exception: {}", e.what());
            payload =
                CommandResponse::operationFailed("current_star", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'guider.current_star'");

    // guider.loop - Start looping
    dispatcher->registerCommand<json>("guider.loop", [](json& payload) {
        LOG_INFO("Executing guider.loop");
        try {
            auto result = loopGuider();
            if (result.contains("status") && result["status"] == "error") {
                payload = result;
            } else {
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("guider.loop exception: {}", e.what());
            payload = CommandResponse::operationFailed("loop", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'guider.loop'");

    // guider.stop_capture - Stop capture/looping
    dispatcher->registerCommand<json>("guider.stop_capture", [](json& payload) {
        LOG_INFO("Executing guider.stop_capture");
        try {
            auto result = stopCapture();
            if (result.contains("status") && result["status"] == "error") {
                payload = result;
            } else {
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("guider.stop_capture exception: {}", e.what());
            payload =
                CommandResponse::operationFailed("stop_capture", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'guider.stop_capture'");

    // guider.pulse - Send guide pulse
    dispatcher->registerCommand<json>("guider.pulse", [](json& payload) {
        LOG_INFO("Executing guider.pulse");
        try {
            if (!payload.contains("direction")) {
                payload = CommandResponse::missingParameter("direction");
                return;
            }
            if (!payload.contains("durationMs")) {
                payload = CommandResponse::missingParameter("durationMs");
                return;
            }
            std::string direction = payload["direction"].get<std::string>();
            int durationMs = payload["durationMs"].get<int>();
            bool useAO = payload.value("useAO", false);

            auto result = guidePulse(direction, durationMs, useAO);
            if (result.contains("status") && result["status"] == "error") {
                payload = result;
            } else {
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("guider.pulse exception: {}", e.what());
            payload = CommandResponse::operationFailed("pulse", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'guider.pulse'");

    // guider.dec_guide_mode - Get/set Dec guide mode
    dispatcher->registerCommand<json>(
        "guider.dec_guide_mode", [](json& payload) {
            LOG_DEBUG("Executing guider.dec_guide_mode");
            try {
                if (payload.contains("mode")) {
                    std::string mode = payload["mode"].get<std::string>();
                    auto result = setDecGuideMode(mode);
                    if (result.contains("status") &&
                        result["status"] == "error") {
                        payload = result;
                    } else {
                        payload = CommandResponse::success(result);
                    }
                } else {
                    auto result = getDecGuideMode();
                    payload = CommandResponse::success(result);
                }
            } catch (const std::exception& e) {
                LOG_ERROR("guider.dec_guide_mode exception: {}", e.what());
                payload = CommandResponse::operationFailed("dec_guide_mode",
                                                           e.what());
            }
        });
    LOG_INFO("Registered command handler for 'guider.dec_guide_mode'");

    // guider.equipment - Equipment control
    dispatcher->registerCommand<json>("guider.equipment", [](json& payload) {
        LOG_DEBUG("Executing guider.equipment");
        try {
            std::string action = payload.value("action", std::string("status"));
            json result;

            if (action == "status") {
                result = isEquipmentConnected();
            } else if (action == "connect") {
                result = connectEquipment();
            } else if (action == "disconnect") {
                result = disconnectEquipment();
            } else if (action == "info") {
                result = getEquipmentInfo();
            } else {
                payload = CommandResponse::invalidParameter(
                    "action",
                    "must be one of: status, connect, disconnect, info");
                return;
            }

            if (result.contains("status") && result["status"] == "error") {
                payload = result;
            } else {
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("guider.equipment exception: {}", e.what());
            payload = CommandResponse::operationFailed("equipment", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'guider.equipment'");

    // guider.profiles - Profile management
    dispatcher->registerCommand<json>("guider.profiles", [](json& payload) {
        LOG_DEBUG("Executing guider.profiles");
        try {
            std::string action = payload.value("action", std::string("list"));
            json result;

            if (action == "list") {
                result = getProfiles();
            } else if (action == "current") {
                result = getCurrentProfile();
            } else if (action == "set") {
                if (!payload.contains("profileId")) {
                    payload = CommandResponse::missingParameter("profileId");
                    return;
                }
                int profileId = payload["profileId"].get<int>();
                result = setProfile(profileId);
            } else {
                payload = CommandResponse::invalidParameter(
                    "action", "must be one of: list, current, set");
                return;
            }

            if (result.contains("status") && result["status"] == "error") {
                payload = result;
            } else {
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("guider.profiles exception: {}", e.what());
            payload = CommandResponse::operationFailed("profiles", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'guider.profiles'");

    // guider.settings - Update settings
    dispatcher->registerCommand<json>("guider.settings", [](json& payload) {
        LOG_INFO("Executing guider.settings");
        try {
            if (!payload.contains("settings")) {
                payload = CommandResponse::missingParameter("settings");
                return;
            }
            auto result = setGuiderSettings(payload["settings"]);
            if (result.contains("status") && result["status"] == "error") {
                payload = result;
            } else {
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("guider.settings exception: {}", e.what());
            payload = CommandResponse::operationFailed("settings", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'guider.settings'");

    // guider.shutdown - Shutdown guider
    dispatcher->registerCommand<json>("guider.shutdown", [](json& payload) {
        LOG_INFO("Executing guider.shutdown");
        try {
            auto result = shutdownGuider();
            if (result.contains("status") && result["status"] == "error") {
                payload = result;
            } else {
                payload = CommandResponse::success(result);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("guider.shutdown exception: {}", e.what());
            payload = CommandResponse::operationFailed("shutdown", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'guider.shutdown'");
}

}  // namespace lithium::app
