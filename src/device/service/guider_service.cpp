/*
 * guider_service.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "guider_service.hpp"

#include "atom/log/spdlog_logger.hpp"

namespace lithium::device {

GuiderService::GuiderService()
    : BaseDeviceService("GuiderService") {
    LOG_INFO( "GuiderService: Initialized");
}

// ==================== Connection ====================

auto GuiderService::connect(const std::string& host, int port,
                            int timeout) -> json {
    return executeWithErrorHandling("connect", [&]() -> json {
        // Create PHD2 client if not exists
        if (!guider_) {
            guider_ = std::make_shared<GuiderClient>("PHD2");
            lithium::client::PHD2Config config;
            config.host = host;
            config.port = port;
            guider_->configurePHD2(config);
        }

        if (!guider_->initialize()) {
            return makeErrorResponse(ErrorCode::INTERNAL_ERROR,
                                     "Failed to initialize guider");
        }

        std::string target = host + ":" + std::to_string(port);
        if (!guider_->connect(target, timeout, 3)) {
            return makeErrorResponse(ErrorCode::CONNECTION_FAILED,
                                     "Failed to connect to PHD2 at " + target);
        }

        publishDeviceStateChange("Guider", "phd2", "connected");

        json data;
        data["host"] = host;
        data["port"] = port;
        data["state"] = guider_->getGuiderStateName();
        return makeSuccessResponse(data, "Connected to PHD2");
    });
}

auto GuiderService::disconnect() -> json {
    return executeWithErrorHandling("disconnect", [&]() -> json {
        if (!guider_) {
            return makeSuccessResponse("Guider not connected");
        }

        guider_->disconnect();
        publishDeviceStateChange("Guider", "phd2", "disconnected");

        return makeSuccessResponse("Disconnected from PHD2");
    });
}

auto GuiderService::getConnectionStatus() -> json {
    return executeWithErrorHandling("getConnectionStatus", [&]() -> json {
        json data;
        data["connected"] = guider_ && guider_->isConnected();
        if (guider_ && guider_->isConnected()) {
            data["state"] = guider_->getGuiderStateName();
            data["equipmentConnected"] = guider_->getConnected();
        }
        return makeSuccessResponse(data);
    });
}

// ==================== Guiding Control ====================

auto GuiderService::startGuiding(double settlePixels, double settleTime,
                                 double settleTimeout,
                                 bool recalibrate) -> json {
    return executeWithErrorHandling("startGuiding", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        lithium::client::SettleParams settle;
        settle.pixels = settlePixels;
        settle.time = settleTime;
        settle.timeout = settleTimeout;

        auto future = guider->startGuiding(settle, recalibrate);

        // Don't wait for settle - return immediately
        // The settle status can be queried via getStatus

        json data;
        data["state"] = "starting";
        data["recalibrate"] = recalibrate;
        return makeSuccessResponse(data, "Guiding started");
    });
}

auto GuiderService::stopGuiding() -> json {
    return executeWithErrorHandling("stopGuiding", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        guider->stopGuiding();
        return makeSuccessResponse("Guiding stopped");
    });
}

auto GuiderService::pause(bool full) -> json {
    return executeWithErrorHandling("pause", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        guider->pause(full);
        return makeSuccessResponse("Guiding paused");
    });
}

auto GuiderService::resume() -> json {
    return executeWithErrorHandling("resume", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        guider->resume();
        return makeSuccessResponse("Guiding resumed");
    });
}

auto GuiderService::dither(double amount, bool raOnly, double settlePixels,
                           double settleTime, double settleTimeout) -> json {
    return executeWithErrorHandling("dither", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        lithium::client::DitherParams params;
        params.amount = amount;
        params.raOnly = raOnly;
        params.settle.pixels = settlePixels;
        params.settle.time = settleTime;
        params.settle.timeout = settleTimeout;

        auto future = guider->dither(params);

        json data;
        data["amount"] = amount;
        data["raOnly"] = raOnly;
        return makeSuccessResponse(data, "Dither started");
    });
}

auto GuiderService::loop() -> json {
    return executeWithErrorHandling("loop", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        guider->loop();
        return makeSuccessResponse("Looping started");
    });
}

auto GuiderService::stopCapture() -> json {
    return executeWithErrorHandling("stopCapture", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        guider->stopGuiding();  // PHD2 uses stopGuiding to stop capture
        return makeSuccessResponse("Capture stopped");
    });
}

// ==================== Status ====================

auto GuiderService::getStatus() -> json {
    return executeWithErrorHandling("getStatus", [&]() -> json {
        json data;

        if (!guider_ || !guider_->isConnected()) {
            data["connected"] = false;
            data["state"] = "DISCONNECTED";
            return makeSuccessResponse(data);
        }

        data["connected"] = true;
        data["state"] = guider_->getGuiderStateName();
        data["isGuiding"] = guider_->isGuiding();
        data["isPaused"] = guider_->isPaused();
        data["isLooping"] = guider_->getGuiderState() == lithium::client::GuiderState::Looping;
        data["isCalibrated"] = guider_->isCalibrated();
        data["equipmentConnected"] = guider_->getConnected();
        data["pixelScale"] = guider_->getPixelScale();
        data["exposure"] = guider_->getExposure();

        // Current star
        auto star = guider_->getCurrentStar();
        if (star.valid) {
            data["star"] = {{"x", star.x},
                            {"y", star.y},
                            {"snr", star.snr},
                            {"mass", star.mass}};
        }

        // Lock position
        auto lockPos = guider_->getLockPosition();
        if (lockPos) {
            data["lockPosition"] = {{"x", (*lockPos)[0]}, {"y", (*lockPos)[1]}};
        }

        return makeSuccessResponse(data);
    });
}

auto GuiderService::getStats() -> json {
    return executeWithErrorHandling("getStats", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        auto stats = guider->getGuideStats();

        json data;
        data["rmsRA"] = stats.rmsRA;
        data["rmsDec"] = stats.rmsDec;
        data["rmsTotal"] = stats.rmsTotal;
        data["peakRA"] = stats.peakRA;
        data["peakDec"] = stats.peakDec;
        data["sampleCount"] = stats.sampleCount;
        data["snr"] = stats.snr;

        return makeSuccessResponse(data);
    });
}

auto GuiderService::getCurrentStar() -> json {
    return executeWithErrorHandling("getCurrentStar", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        auto star = guider->getCurrentStar();

        json data;
        data["valid"] = star.valid;
        if (star.valid) {
            data["x"] = star.x;
            data["y"] = star.y;
            data["snr"] = star.snr;
            data["mass"] = star.mass;
        }

        return makeSuccessResponse(data);
    });
}

// ==================== Calibration ====================

auto GuiderService::isCalibrated() -> json {
    return executeWithErrorHandling("isCalibrated", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        json data;
        data["calibrated"] = guider->isCalibrated();
        return makeSuccessResponse(data);
    });
}

auto GuiderService::clearCalibration(const std::string& which) -> json {
    return executeWithErrorHandling("clearCalibration", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        guider->clearCalibration();  // PHD2Client doesn't take 'which' param
        return makeSuccessResponse("Calibration cleared");
    });
}

auto GuiderService::flipCalibration() -> json {
    return executeWithErrorHandling("flipCalibration", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        guider->flipCalibration();
        return makeSuccessResponse("Calibration flipped");
    });
}

auto GuiderService::getCalibrationData() -> json {
    return executeWithErrorHandling("getCalibrationData", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        auto calData = guider->getCalibrationData();

        json data;
        data["calibrated"] = calData.calibrated;
        data["raRate"] = calData.raRate;
        data["decRate"] = calData.decRate;
        data["raAngle"] = calData.raAngle;
        data["decAngle"] = calData.decAngle;
        data["decFlipped"] = calData.decFlipped;
        data["timestamp"] = calData.timestamp;

        return makeSuccessResponse(data);
    });
}

// ==================== Star Selection ====================

auto GuiderService::findStar(std::optional<int> roiX, std::optional<int> roiY,
                             std::optional<int> roiWidth,
                             std::optional<int> roiHeight) -> json {
    return executeWithErrorHandling("findStar", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        std::optional<std::array<int, 4>> roi;
        if (roiX && roiY && roiWidth && roiHeight) {
            roi = std::array<int, 4>{*roiX, *roiY, *roiWidth, *roiHeight};
        }

        auto star = guider->findStar(roi);

        json data;
        data["valid"] = star.valid;
        if (star.valid) {
            data["x"] = star.x;
            data["y"] = star.y;
        }

        return makeSuccessResponse(data);
    });
}

auto GuiderService::setLockPosition(double x, double y, bool exact) -> json {
    return executeWithErrorHandling("setLockPosition", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        guider->setLockPosition(x, y, exact);
        return makeSuccessResponse("Lock position set");
    });
}

auto GuiderService::getLockPosition() -> json {
    return executeWithErrorHandling("getLockPosition", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        auto pos = guider->getLockPosition();

        json data;
        if (pos) {
            data["x"] = (*pos)[0];
            data["y"] = (*pos)[1];
            data["set"] = true;
        } else {
            data["set"] = false;
        }

        return makeSuccessResponse(data);
    });
}

// ==================== Camera Control ====================

auto GuiderService::getExposure() -> json {
    return executeWithErrorHandling("getExposure", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        json data;
        data["exposureMs"] = guider->getExposure();
        return makeSuccessResponse(data);
    });
}

auto GuiderService::setExposure(int exposureMs) -> json {
    return executeWithErrorHandling("setExposure", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        guider->setExposure(exposureMs);
        return makeSuccessResponse("Exposure set");
    });
}

auto GuiderService::getExposureDurations() -> json {
    return executeWithErrorHandling("getExposureDurations", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        json data;
        data["durations"] = guider->getExposureDurations();
        return makeSuccessResponse(data);
    });
}

auto GuiderService::getCameraFrameSize() -> json {
    return executeWithErrorHandling("getCameraFrameSize", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        auto size = guider->getCameraFrameSize();
        json data;
        data["width"] = size[0];
        data["height"] = size[1];
        return makeSuccessResponse(data);
    });
}

auto GuiderService::getCcdTemperature() -> json {
    return executeWithErrorHandling("getCcdTemperature", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        json data;
        data["temperature"] = guider->getCcdTemperature();
        return makeSuccessResponse(data);
    });
}

auto GuiderService::getCoolerStatus() -> json {
    return executeWithErrorHandling("getCoolerStatus", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        json data = guider->getCoolerStatus();
        return makeSuccessResponse(data);
    });
}

auto GuiderService::saveImage() -> json {
    return executeWithErrorHandling("saveImage", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        json data;
        data["filename"] = guider->saveImage();
        return makeSuccessResponse(data);
    });
}

auto GuiderService::getStarImage(int size) -> json {
    return executeWithErrorHandling("getStarImage", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        json data = guider->getStarImage(size);
        return makeSuccessResponse(data);
    });
}

auto GuiderService::captureSingleFrame(std::optional<int> exposureMs) -> json {
    return executeWithErrorHandling("captureSingleFrame", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        guider->captureSingleFrame(exposureMs, std::nullopt);
        return makeSuccessResponse("Frame capture started");
    });
}

// ==================== Guide Pulse ====================

auto GuiderService::guidePulse(const std::string& direction, int durationMs,
                               bool useAO) -> json {
    return executeWithErrorHandling("guidePulse", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        std::string which = useAO ? "AO" : "Mount";
        guider->guidePulse(durationMs, direction, which);
        return makeSuccessResponse("Guide pulse sent");
    });
}

// ==================== Algorithm Settings ====================

auto GuiderService::getDecGuideMode() -> json {
    return executeWithErrorHandling("getDecGuideMode", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        json data;
        data["mode"] = guider->getDecGuideMode();
        return makeSuccessResponse(data);
    });
}

auto GuiderService::setDecGuideMode(const std::string& mode) -> json {
    return executeWithErrorHandling("setDecGuideMode", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        guider->setDecGuideMode(mode);
        return makeSuccessResponse("Dec guide mode set");
    });
}

auto GuiderService::getAlgoParam(const std::string& axis,
                                 const std::string& name) -> json {
    return executeWithErrorHandling("getAlgoParam", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        json data;
        data["axis"] = axis;
        data["name"] = name;
        data["value"] = guider->getAlgoParam(axis, name);
        return makeSuccessResponse(data);
    });
}

auto GuiderService::setAlgoParam(const std::string& axis,
                                 const std::string& name, double value) -> json {
    return executeWithErrorHandling("setAlgoParam", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        guider->setAlgoParam(axis, name, value);
        return makeSuccessResponse("Algorithm parameter set");
    });
}

// ==================== Equipment ====================

auto GuiderService::isEquipmentConnected() -> json {
    return executeWithErrorHandling("isEquipmentConnected", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        json data;
        data["connected"] = guider->getConnected();
        return makeSuccessResponse(data);
    });
}

auto GuiderService::connectEquipment() -> json {
    return executeWithErrorHandling("connectEquipment", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        guider->setConnected(true);
        return makeSuccessResponse("Equipment connected");
    });
}

auto GuiderService::disconnectEquipment() -> json {
    return executeWithErrorHandling("disconnectEquipment", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        guider->setConnected(false);
        return makeSuccessResponse("Equipment disconnected");
    });
}

auto GuiderService::getEquipmentInfo() -> json {
    return executeWithErrorHandling("getEquipmentInfo", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        json data = guider->getCurrentEquipment();
        return makeSuccessResponse(data);
    });
}

// ==================== Profile Management ====================

auto GuiderService::getProfiles() -> json {
    return executeWithErrorHandling("getProfiles", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        json data = guider->getProfiles();

        return makeSuccessResponse(data);
    });
}

auto GuiderService::getCurrentProfile() -> json {
    return executeWithErrorHandling("getCurrentProfile", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        json data;
        auto profile = guider->getProfile();
        data["id"] = profile.value("id", -1);
        data["name"] = profile.value("name", "");
        return makeSuccessResponse(data);
    });
}

auto GuiderService::setProfile(int profileId) -> json {
    return executeWithErrorHandling("setProfile", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        guider->setProfile(profileId);
        return makeSuccessResponse("Profile set");
    });
}

// ==================== Settings ====================

auto GuiderService::updateSettings(const json& settings) -> json {
    return executeWithErrorHandling("updateSettings", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        if (settings.contains("exposure")) {
            guider->setExposure(settings["exposure"].get<int>());
        }

        if (settings.contains("decGuideMode")) {
            guider->setDecGuideMode(settings["decGuideMode"].get<std::string>());
        }

        if (settings.contains("lockShiftEnabled")) {
            guider->setLockShiftEnabled(settings["lockShiftEnabled"].get<bool>());
        }

        return makeSuccessResponse("Settings updated");
    });
}

// ==================== Lock Shift ====================

auto GuiderService::isLockShiftEnabled() -> json {
    return executeWithErrorHandling("isLockShiftEnabled", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        json data;
        data["enabled"] = guider->isLockShiftEnabled();
        return makeSuccessResponse(data);
    });
}

auto GuiderService::setLockShiftEnabled(bool enable) -> json {
    return executeWithErrorHandling("setLockShiftEnabled", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        guider->setLockShiftEnabled(enable);
        return makeSuccessResponse("Lock shift updated");
    });
}

// ==================== Shutdown ====================

auto GuiderService::shutdown() -> json {
    return executeWithErrorHandling("shutdown", [&]() -> json {
        auto [guider, error] = getConnectedGuider();
        if (error) return *error;

        guider->shutdown();
        guider_.reset();
        return makeSuccessResponse("Guider shutdown");
    });
}

// ==================== Private Helpers ====================

auto GuiderService::getConnectedGuider()
    -> std::pair<std::shared_ptr<GuiderClient>, std::optional<json>> {
    if (!guider_) {
        return {nullptr,
                makeErrorResponse(ErrorCode::DEVICE_NOT_FOUND, "Guider not found")};
    }

    if (!guider_->isConnected()) {
        return {nullptr, makeErrorResponse(ErrorCode::DEVICE_NOT_CONNECTED,
                                           "Guider not connected")};
    }

    return {guider_, std::nullopt};
}

}  // namespace lithium::device
