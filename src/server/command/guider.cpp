/*
 * guider.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "guider.hpp"
#include "device/service/guider_service.hpp"

#include "atom/log/spdlog_logger.hpp"

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
    LOG_INFO( "connectGuider: host={} port={} timeout={}", host, port, timeout);
    return getGuiderService()->connect(host, port, timeout);
}

auto disconnectGuider() -> json {
    LOG_INFO( "disconnectGuider");
    return getGuiderService()->disconnect();
}

auto getConnectionStatus() -> json {
    return getGuiderService()->getConnectionStatus();
}

// ==================== Guiding Control ====================

auto startGuiding(double settlePixels, double settleTime, double settleTimeout,
                  bool recalibrate) -> json {
    LOG_INFO( "startGuiding: settlePixels={} settleTime={} settleTimeout={} recalibrate={}",
          settlePixels, settleTime, settleTimeout, recalibrate);
    return getGuiderService()->startGuiding(settlePixels, settleTime, settleTimeout, recalibrate);
}

auto stopGuiding() -> json {
    LOG_INFO( "stopGuiding");
    return getGuiderService()->stopGuiding();
}

auto pauseGuiding(bool full) -> json {
    LOG_INFO( "pauseGuiding: full={}", full);
    return getGuiderService()->pause(full);
}

auto resumeGuiding() -> json {
    LOG_INFO( "resumeGuiding");
    return getGuiderService()->resume();
}

auto ditherGuider(double amount, bool raOnly, double settlePixels,
                  double settleTime, double settleTimeout) -> json {
    LOG_INFO( "ditherGuider: amount={} raOnly={}", amount, raOnly);
    return getGuiderService()->dither(amount, raOnly, settlePixels, settleTime, settleTimeout);
}

auto loopGuider() -> json {
    LOG_INFO( "loopGuider");
    return getGuiderService()->loop();
}

auto stopCapture() -> json {
    LOG_INFO( "stopCapture");
    return getGuiderService()->stopCapture();
}

// ==================== Status ====================

auto getGuiderStatus() -> json {
    return getGuiderService()->getStatus();
}

auto getGuiderStats() -> json {
    return getGuiderService()->getStats();
}

auto getCurrentStar() -> json {
    return getGuiderService()->getCurrentStar();
}

// ==================== Calibration ====================

auto isCalibrated() -> json {
    return getGuiderService()->isCalibrated();
}

auto clearCalibration(const std::string& which) -> json {
    LOG_INFO( "clearCalibration: which={}", which);
    return getGuiderService()->clearCalibration(which);
}

auto flipCalibration() -> json {
    LOG_INFO( "flipCalibration");
    return getGuiderService()->flipCalibration();
}

auto getCalibrationData() -> json {
    return getGuiderService()->getCalibrationData();
}

// ==================== Star Selection ====================

auto findStar(std::optional<int> roiX, std::optional<int> roiY,
              std::optional<int> roiWidth, std::optional<int> roiHeight) -> json {
    LOG_INFO( "findStar");
    return getGuiderService()->findStar(roiX, roiY, roiWidth, roiHeight);
}

auto setLockPosition(double x, double y, bool exact) -> json {
    LOG_INFO( "setLockPosition: x={} y={} exact={}", x, y, exact);
    return getGuiderService()->setLockPosition(x, y, exact);
}

auto getLockPosition() -> json {
    return getGuiderService()->getLockPosition();
}

// ==================== Camera Control ====================

auto getExposure() -> json {
    return getGuiderService()->getExposure();
}

auto setExposure(int exposureMs) -> json {
    LOG_INFO( "setExposure: exposureMs={}", exposureMs);
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

auto getCoolerStatus() -> json {
    return getGuiderService()->getCoolerStatus();
}

auto saveImage() -> json {
    LOG_INFO( "saveImage");
    return getGuiderService()->saveImage();
}

auto getStarImage(int size) -> json {
    return getGuiderService()->getStarImage(size);
}

auto captureSingleFrame(std::optional<int> exposureMs) -> json {
    LOG_INFO( "captureSingleFrame");
    return getGuiderService()->captureSingleFrame(exposureMs);
}

// ==================== Guide Pulse ====================

auto guidePulse(const std::string& direction, int durationMs, bool useAO) -> json {
    LOG_INFO( "guidePulse: direction={} durationMs={} useAO={}", direction, durationMs, useAO);
    return getGuiderService()->guidePulse(direction, durationMs, useAO);
}

// ==================== Algorithm Settings ====================

auto getDecGuideMode() -> json {
    return getGuiderService()->getDecGuideMode();
}

auto setDecGuideMode(const std::string& mode) -> json {
    LOG_INFO( "setDecGuideMode: mode={}", mode);
    return getGuiderService()->setDecGuideMode(mode);
}

auto getAlgoParam(const std::string& axis, const std::string& name) -> json {
    return getGuiderService()->getAlgoParam(axis, name);
}

auto setAlgoParam(const std::string& axis, const std::string& name,
                  double value) -> json {
    LOG_INFO( "setAlgoParam: axis={} name={} value={}", axis, name, value);
    return getGuiderService()->setAlgoParam(axis, name, value);
}

// ==================== Equipment ====================

auto isEquipmentConnected() -> json {
    return getGuiderService()->isEquipmentConnected();
}

auto connectEquipment() -> json {
    LOG_INFO( "connectEquipment");
    return getGuiderService()->connectEquipment();
}

auto disconnectEquipment() -> json {
    LOG_INFO( "disconnectEquipment");
    return getGuiderService()->disconnectEquipment();
}

auto getEquipmentInfo() -> json {
    return getGuiderService()->getEquipmentInfo();
}

// ==================== Profile Management ====================

auto getProfiles() -> json {
    return getGuiderService()->getProfiles();
}

auto getCurrentProfile() -> json {
    return getGuiderService()->getCurrentProfile();
}

auto setProfile(int profileId) -> json {
    LOG_INFO( "setProfile: profileId={}", profileId);
    return getGuiderService()->setProfile(profileId);
}

// ==================== Settings ====================

auto setGuiderSettings(const json& settings) -> json {
    LOG_INFO( "setGuiderSettings");
    return getGuiderService()->updateSettings(settings);
}

// ==================== Lock Shift ====================

auto isLockShiftEnabled() -> json {
    return getGuiderService()->isLockShiftEnabled();
}

auto setLockShiftEnabled(bool enable) -> json {
    LOG_INFO( "setLockShiftEnabled: enable={}", enable);
    return getGuiderService()->setLockShiftEnabled(enable);
}

// ==================== Shutdown ====================

auto shutdownGuider() -> json {
    LOG_INFO( "shutdownGuider");
    return getGuiderService()->shutdown();
}

}  // namespace lithium::middleware
