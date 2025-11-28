/*
 * phd2_client.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: PHD2 guider client implementation

*************************************************/

#include "phd2_client.hpp"
#include "exceptions.hpp"

#include <spdlog/spdlog.h>
#include <chrono>

namespace lithium::client {

PHD2Client::PHD2Client(std::string name)
    : GuiderClient(std::move(name)) {
    spdlog::info("PHD2Client created: {}", getName());
}

PHD2Client::~PHD2Client() {
    if (isConnected()) {
        disconnect();
    }
    spdlog::debug("PHD2Client destroyed: {}", getName());
}

bool PHD2Client::initialize() {
    spdlog::debug("Initializing PHD2Client");
    setState(ClientState::Initialized);
    emitEvent("initialized", "");
    return true;
}

bool PHD2Client::destroy() {
    spdlog::debug("Destroying PHD2Client");

    if (isConnected()) {
        disconnect();
    }

    setState(ClientState::Uninitialized);
    emitEvent("destroyed", "");
    return true;
}

bool PHD2Client::connect(const std::string& target, int timeout, int maxRetry) {
    spdlog::debug("Connecting to PHD2");
    setState(ClientState::Connecting);

    // Parse target (host:port or just host)
    std::string host = phd2Config_.host;
    int port = phd2Config_.port;

    if (!target.empty()) {
        auto colonPos = target.find(':');
        if (colonPos != std::string::npos) {
            host = target.substr(0, colonPos);
            port = std::stoi(target.substr(colonPos + 1));
        } else {
            host = target;
        }
    }

    phd2Config_.host = host;
    phd2Config_.port = port;

    // Create connection with this as event handler
    connection_ = std::make_unique<phd2::Connection>(
        host, port, std::shared_ptr<phd2::EventHandler>(this, [](phd2::EventHandler*){}));

    for (int attempt = 0; attempt < maxRetry; ++attempt) {
        if (connection_->connect(timeout)) {
            setState(ClientState::Connected);
            emitEvent("connected", host + ":" + std::to_string(port));
            spdlog::info("Connected to PHD2 at {}:{}", host, port);

            // Get initial state
            try {
                updateGuiderState(getAppState());
            } catch (...) {
                // Ignore initial state errors
            }

            return true;
        }

        spdlog::warn("PHD2 connection attempt {} failed", attempt + 1);
        std::this_thread::sleep_for(
            std::chrono::milliseconds(phd2Config_.reconnectDelayMs));
    }

    setError(1, "Failed to connect to PHD2");
    return false;
}

bool PHD2Client::disconnect() {
    spdlog::debug("Disconnecting from PHD2");
    setState(ClientState::Disconnecting);

    if (connection_) {
        connection_->disconnect();
        connection_.reset();
    }

    guiderState_.store(GuiderState::Stopped);
    setState(ClientState::Disconnected);
    emitEvent("disconnected", "");
    return true;
}

bool PHD2Client::isConnected() const {
    return connection_ && connection_->isConnected();
}

std::vector<std::string> PHD2Client::scan() {
    // PHD2 doesn't support scanning, return default
    return {phd2Config_.host + ":" + std::to_string(phd2Config_.port)};
}

std::future<bool> PHD2Client::startGuiding(const SettleParams& settle,
                                           bool recalibrate) {
    std::lock_guard<std::mutex> lock(promiseMutex_);

    if (settleInProgress_) {
        throw phd2::InvalidStateException("Settle operation already in progress");
    }

    settlePromise_ = std::promise<bool>();
    settleInProgress_ = true;

    try {
        phd2::json params = phd2::json::object();
        params["settle"] = {
            {"pixels", settle.pixels},
            {"time", settle.time},
            {"timeout", settle.timeout}
        };
        params["recalibrate"] = recalibrate;

        connection_->sendRpc("guide", params);
        guiderState_.store(GuiderState::Guiding);
        emitEvent("guiding_started", "");

    } catch (const std::exception& e) {
        settleInProgress_ = false;
        settlePromise_.set_value(false);
        spdlog::error("Failed to start guiding: {}", e.what());
    }

    return settlePromise_.get_future();
}

void PHD2Client::stopGuiding() {
    if (!isConnected()) return;

    try {
        connection_->sendRpc("stop_capture");
        guiderState_.store(GuiderState::Stopped);
        emitEvent("guiding_stopped", "");
    } catch (const std::exception& e) {
        spdlog::error("Failed to stop guiding: {}", e.what());
    }
}

void PHD2Client::pause(bool full) {
    if (!isConnected()) return;

    try {
        phd2::json params = {full};
        connection_->sendRpc("set_paused", params);
        guiderState_.store(GuiderState::Paused);
        emitEvent("guiding_paused", full ? "full" : "partial");
    } catch (const std::exception& e) {
        spdlog::error("Failed to pause: {}", e.what());
    }
}

void PHD2Client::resume() {
    if (!isConnected()) return;

    try {
        connection_->sendRpc("set_paused", {false});
        guiderState_.store(GuiderState::Guiding);
        emitEvent("guiding_resumed", "");
    } catch (const std::exception& e) {
        spdlog::error("Failed to resume: {}", e.what());
    }
}

std::future<bool> PHD2Client::dither(const DitherParams& params) {
    std::lock_guard<std::mutex> lock(promiseMutex_);

    if (settleInProgress_) {
        throw phd2::InvalidStateException("Settle operation already in progress");
    }

    settlePromise_ = std::promise<bool>();
    settleInProgress_ = true;

    try {
        phd2::json rpcParams = phd2::json::object();
        rpcParams["amount"] = params.amount;
        rpcParams["raOnly"] = params.raOnly;
        rpcParams["settle"] = {
            {"pixels", params.settle.pixels},
            {"time", params.settle.time},
            {"timeout", params.settle.timeout}
        };

        connection_->sendRpc("dither", rpcParams);
        emitEvent("dither_started", std::to_string(params.amount));

    } catch (const std::exception& e) {
        settleInProgress_ = false;
        settlePromise_.set_value(false);
        spdlog::error("Failed to dither: {}", e.what());
    }

    return settlePromise_.get_future();
}

void PHD2Client::loop() {
    if (!isConnected()) return;

    try {
        connection_->sendRpc("loop");
        guiderState_.store(GuiderState::Looping);
        emitEvent("looping_started", "");
    } catch (const std::exception& e) {
        spdlog::error("Failed to start loop: {}", e.what());
    }
}

bool PHD2Client::isCalibrated() const {
    if (!isConnected()) return false;

    try {
        auto response = connection_->sendRpc("get_calibrated");
        return response.result.get<bool>();
    } catch (...) {
        return false;
    }
}

void PHD2Client::clearCalibration() {
    if (!isConnected()) return;

    try {
        connection_->sendRpc("clear_calibration", {"both"});
        calibrationData_.calibrated = false;
        emitEvent("calibration_cleared", "");
    } catch (const std::exception& e) {
        spdlog::error("Failed to clear calibration: {}", e.what());
    }
}

void PHD2Client::flipCalibration() {
    if (!isConnected()) return;

    try {
        connection_->sendRpc("flip_calibration");
        emitEvent("calibration_flipped", "");
    } catch (const std::exception& e) {
        spdlog::error("Failed to flip calibration: {}", e.what());
    }
}

CalibrationData PHD2Client::getCalibrationData() const {
    if (!isConnected()) return calibrationData_;

    try {
        auto response = connection_->sendRpc("get_calibration_data", {"Mount"});
        CalibrationData data;
        data.calibrated = response.result.value("calibrated", false);
        data.raRate = response.result.value("xRate", 0.0);
        data.decRate = response.result.value("yRate", 0.0);
        data.raAngle = response.result.value("xAngle", 0.0);
        data.decAngle = response.result.value("yAngle", 0.0);
        return data;
    } catch (...) {
        return calibrationData_;
    }
}

GuideStar PHD2Client::findStar(const std::optional<std::array<int, 4>>& roi) {
    GuideStar star;
    if (!isConnected()) return star;

    try {
        phd2::json params = phd2::json::array();
        if (roi) {
            params.push_back(*roi);
        }

        auto response = connection_->sendRpc("find_star", params);
        star.x = response.result[0].get<double>();
        star.y = response.result[1].get<double>();
        star.valid = true;

        emitEvent("star_found", std::to_string(star.x) + "," + std::to_string(star.y));
    } catch (const std::exception& e) {
        spdlog::error("Failed to find star: {}", e.what());
    }

    return star;
}

void PHD2Client::setLockPosition(double x, double y, bool exact) {
    if (!isConnected()) return;

    try {
        connection_->sendRpc("set_lock_position", {x, y, exact});
        emitEvent("lock_position_set", std::to_string(x) + "," + std::to_string(y));
    } catch (const std::exception& e) {
        spdlog::error("Failed to set lock position: {}", e.what());
    }
}

std::optional<std::array<double, 2>> PHD2Client::getLockPosition() const {
    if (!isConnected()) return std::nullopt;

    try {
        auto response = connection_->sendRpc("get_lock_position");
        if (response.result.is_array() && response.result.size() >= 2) {
            return std::array<double, 2>{
                response.result[0].get<double>(),
                response.result[1].get<double>()
            };
        }
    } catch (...) {}

    return std::nullopt;
}

int PHD2Client::getExposure() const {
    if (!isConnected()) return 0;

    try {
        auto response = connection_->sendRpc("get_exposure");
        return response.result.get<int>();
    } catch (...) {
        return 0;
    }
}

void PHD2Client::setExposure(int exposureMs) {
    if (!isConnected()) return;

    try {
        connection_->sendRpc("set_exposure", {exposureMs});
        emitEvent("exposure_set", std::to_string(exposureMs));
    } catch (const std::exception& e) {
        spdlog::error("Failed to set exposure: {}", e.what());
    }
}

std::vector<int> PHD2Client::getExposureDurations() const {
    if (!isConnected()) return {};

    try {
        auto response = connection_->sendRpc("get_exposure_durations");
        return response.result.get<std::vector<int>>();
    } catch (...) {
        return {};
    }
}

GuiderState PHD2Client::getGuiderState() const {
    return guiderState_.load();
}

GuideStats PHD2Client::getGuideStats() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return guideStats_;
}

GuideStar PHD2Client::getCurrentStar() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return currentStar_;
}

double PHD2Client::getPixelScale() const {
    if (!isConnected()) return 0.0;

    try {
        auto response = connection_->sendRpc("get_pixel_scale");
        return response.result.get<double>();
    } catch (...) {
        return 0.0;
    }
}

void PHD2Client::configurePHD2(const PHD2Config& config) {
    phd2Config_ = config;
}

std::string PHD2Client::getAppState() const {
    if (!isConnected()) return "Disconnected";

    try {
        auto response = connection_->sendRpc("get_app_state");
        return response.result.get<std::string>();
    } catch (...) {
        return "Unknown";
    }
}

phd2::json PHD2Client::getProfile() const {
    if (!isConnected()) return {};

    try {
        auto response = connection_->sendRpc("get_profile");
        return response.result;
    } catch (...) {
        return {};
    }
}

void PHD2Client::setProfile(int profileId) {
    if (!isConnected()) return;

    try {
        connection_->sendRpc("set_profile", {profileId});
        emitEvent("profile_changed", std::to_string(profileId));
    } catch (const std::exception& e) {
        spdlog::error("Failed to set profile: {}", e.what());
    }
}

phd2::json PHD2Client::getProfiles() const {
    if (!isConnected()) return {};

    try {
        auto response = connection_->sendRpc("get_profiles");
        return response.result;
    } catch (...) {
        return {};
    }
}

void PHD2Client::guidePulse(int amount, const std::string& direction,
                            const std::string& which) {
    if (!isConnected()) return;

    try {
        connection_->sendRpc("guide_pulse", {amount, direction, which});
    } catch (const std::exception& e) {
        spdlog::error("Failed to send guide pulse: {}", e.what());
    }
}

std::string PHD2Client::getDecGuideMode() const {
    if (!isConnected()) return "";

    try {
        auto response = connection_->sendRpc("get_dec_guide_mode");
        return response.result.get<std::string>();
    } catch (...) {
        return "";
    }
}

void PHD2Client::setDecGuideMode(const std::string& mode) {
    if (!isConnected()) return;

    try {
        connection_->sendRpc("set_dec_guide_mode", {mode});
        emitEvent("dec_guide_mode_changed", mode);
    } catch (const std::exception& e) {
        spdlog::error("Failed to set Dec guide mode: {}", e.what());
    }
}

std::string PHD2Client::saveImage() {
    if (!isConnected()) return "";

    try {
        auto response = connection_->sendRpc("save_image");
        return response.result["filename"].get<std::string>();
    } catch (...) {
        return "";
    }
}

std::array<int, 2> PHD2Client::getCameraFrameSize() const {
    if (!isConnected()) return {0, 0};

    try {
        auto response = connection_->sendRpc("get_camera_frame_size");
        return response.result.get<std::array<int, 2>>();
    } catch (...) {
        return {0, 0};
    }
}

double PHD2Client::getCcdTemperature() const {
    if (!isConnected()) return 0.0;

    try {
        auto response = connection_->sendRpc("get_ccd_temperature");
        return response.result["temperature"].get<double>();
    } catch (...) {
        return 0.0;
    }
}

phd2::json PHD2Client::getCoolerStatus() const {
    if (!isConnected()) return {};

    try {
        auto response = connection_->sendRpc("get_cooler_status");
        return response.result;
    } catch (...) {
        return {};
    }
}

phd2::json PHD2Client::getStarImage(int size) const {
    if (!isConnected()) return {};

    try {
        phd2::json params = phd2::json::array();
        if (size >= 15) {
            params.push_back(size);
        }
        auto response = connection_->sendRpc("get_star_image", params);
        return response.result;
    } catch (...) {
        return {};
    }
}

phd2::json PHD2Client::getCurrentEquipment() const {
    if (!isConnected()) return {};

    try {
        auto response = connection_->sendRpc("get_current_equipment");
        return response.result;
    } catch (...) {
        return {};
    }
}

bool PHD2Client::getConnected() const {
    if (!isConnected()) return false;

    try {
        auto response = connection_->sendRpc("get_connected");
        return response.result.get<bool>();
    } catch (...) {
        return false;
    }
}

void PHD2Client::setConnected(bool connect) {
    if (!isConnected()) return;

    try {
        connection_->sendRpc("set_connected", {connect});
        emitEvent(connect ? "equipment_connected" : "equipment_disconnected", "");
    } catch (const std::exception& e) {
        spdlog::error("Failed to set connected: {}", e.what());
    }
}

std::vector<std::string> PHD2Client::getAlgoParamNames(const std::string& axis) const {
    if (!isConnected()) return {};

    try {
        auto response = connection_->sendRpc("get_algo_param_names", {axis});
        return response.result.get<std::vector<std::string>>();
    } catch (...) {
        return {};
    }
}

double PHD2Client::getAlgoParam(const std::string& axis, const std::string& name) const {
    if (!isConnected()) return 0.0;

    try {
        auto response = connection_->sendRpc("get_algo_param", {axis, name});
        return response.result.get<double>();
    } catch (...) {
        return 0.0;
    }
}

void PHD2Client::setAlgoParam(const std::string& axis, const std::string& name, double value) {
    if (!isConnected()) return;

    try {
        connection_->sendRpc("set_algo_param", {axis, name, value});
    } catch (const std::exception& e) {
        spdlog::error("Failed to set algo param: {}", e.what());
    }
}

bool PHD2Client::getGuideOutputEnabled() const {
    if (!isConnected()) return false;

    try {
        auto response = connection_->sendRpc("get_guide_output_enabled");
        return response.result.get<bool>();
    } catch (...) {
        return false;
    }
}

void PHD2Client::setGuideOutputEnabled(bool enable) {
    if (!isConnected()) return;

    try {
        connection_->sendRpc("set_guide_output_enabled", {enable});
    } catch (const std::exception& e) {
        spdlog::error("Failed to set guide output enabled: {}", e.what());
    }
}

bool PHD2Client::getLockShiftEnabled() const {
    if (!isConnected()) return false;

    try {
        auto response = connection_->sendRpc("get_lock_shift_enabled");
        return response.result.get<bool>();
    } catch (...) {
        return false;
    }
}

void PHD2Client::setLockShiftEnabled(bool enable) {
    if (!isConnected()) return;

    try {
        connection_->sendRpc("set_lock_shift_enabled", {enable});
    } catch (const std::exception& e) {
        spdlog::error("Failed to set lock shift enabled: {}", e.what());
    }
}

phd2::json PHD2Client::getLockShiftParams() const {
    if (!isConnected()) return {};

    try {
        auto response = connection_->sendRpc("get_lock_shift_params");
        return response.result;
    } catch (...) {
        return {};
    }
}

void PHD2Client::setLockShiftParams(const phd2::json& params) {
    if (!isConnected()) return;

    try {
        connection_->sendRpc("set_lock_shift_params", params);
    } catch (const std::exception& e) {
        spdlog::error("Failed to set lock shift params: {}", e.what());
    }
}

phd2::json PHD2Client::getVariableDelaySettings() const {
    if (!isConnected()) return {};

    try {
        auto response = connection_->sendRpc("get_variable_delay_settings");
        return response.result;
    } catch (...) {
        return {};
    }
}

void PHD2Client::setVariableDelaySettings(const phd2::json& settings) {
    if (!isConnected()) return;

    try {
        connection_->sendRpc("set_variable_delay_settings", settings);
    } catch (const std::exception& e) {
        spdlog::error("Failed to set variable delay settings: {}", e.what());
    }
}

bool PHD2Client::getSettling() const {
    if (!isConnected()) return false;

    try {
        auto response = connection_->sendRpc("get_settling");
        return response.result.get<bool>();
    } catch (...) {
        return false;
    }
}

void PHD2Client::captureSingleFrame(std::optional<int> exposureMs,
                                    std::optional<std::array<int, 4>> subframe) {
    if (!isConnected()) return;

    try {
        phd2::json params = phd2::json::object();
        if (exposureMs) {
            params["exposure"] = *exposureMs;
        }
        if (subframe) {
            params["subframe"] = *subframe;
        }
        connection_->sendRpc("capture_single_frame", params);
    } catch (const std::exception& e) {
        spdlog::error("Failed to capture single frame: {}", e.what());
    }
}

int PHD2Client::getSearchRegion() const {
    if (!isConnected()) return 0;

    try {
        auto response = connection_->sendRpc("get_search_region");
        return response.result.get<int>();
    } catch (...) {
        return 0;
    }
}

int PHD2Client::getCameraBinning() const {
    if (!isConnected()) return 1;

    try {
        auto response = connection_->sendRpc("get_camera_binning");
        return response.result.get<int>();
    } catch (...) {
        return 1;
    }
}

std::string PHD2Client::exportConfigSettings() const {
    if (!isConnected()) return "";

    try {
        auto response = connection_->sendRpc("export_config_settings");
        return response.result.get<std::string>();
    } catch (...) {
        return "";
    }
}

void PHD2Client::shutdown() {
    if (!isConnected()) return;

    try {
        connection_->sendRpc("shutdown");
        emitEvent("shutdown", "");
    } catch (const std::exception& e) {
        spdlog::error("Failed to shutdown PHD2: {}", e.what());
    }
}

void PHD2Client::onEvent(const phd2::Event& event) {
    processEvent(event);
}

void PHD2Client::onConnectionError(const std::string& error) {
    spdlog::error("PHD2 connection error: {}", error);
    setError(100, error);
    emitEvent("connection_error", error);
}

void PHD2Client::handleSettleDone(bool success) {
    std::lock_guard<std::mutex> lock(promiseMutex_);
    if (settleInProgress_) {
        settlePromise_.set_value(success);
        settleInProgress_ = false;
        emitEvent("settle_done", success ? "success" : "failed");
    }
}

void PHD2Client::updateGuiderState(const std::string& appState) {
    if (appState == "Stopped") {
        guiderState_.store(GuiderState::Stopped);
    } else if (appState == "Looping") {
        guiderState_.store(GuiderState::Looping);
    } else if (appState == "Calibrating") {
        guiderState_.store(GuiderState::Calibrating);
    } else if (appState == "Guiding") {
        guiderState_.store(GuiderState::Guiding);
    } else if (appState == "LostLock") {
        guiderState_.store(GuiderState::LostStar);
    } else if (appState == "Paused") {
        guiderState_.store(GuiderState::Paused);
    }
}

void PHD2Client::processEvent(const phd2::Event& event) {
    std::visit([this](auto&& e) {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, phd2::AppStateEvent>) {
            updateGuiderState(e.state);
            emitEvent("app_state", e.state);
        }
        else if constexpr (std::is_same_v<T, phd2::GuideStepEvent>) {
            std::lock_guard<std::mutex> lock(stateMutex_);
            currentStar_.x = e.starMass;  // Approximation
            currentStar_.snr = e.snr;
            guideStats_.rmsRA = e.raDistanceRaw;
            guideStats_.rmsDec = e.decDistanceRaw;
            emitEvent("guide_step", "");
        }
        else if constexpr (std::is_same_v<T, phd2::SettleDoneEvent>) {
            handleSettleDone(e.status == 0);
        }
        else if constexpr (std::is_same_v<T, phd2::StarLostEvent>) {
            guiderState_.store(GuiderState::LostStar);
            emitEvent("star_lost", e.status);
        }
        else if constexpr (std::is_same_v<T, phd2::CalibrationCompleteEvent>) {
            calibrationData_.calibrated = true;
            emitEvent("calibration_complete", "");
        }
        else if constexpr (std::is_same_v<T, phd2::StartGuidingEvent>) {
            guiderState_.store(GuiderState::Guiding);
            emitEvent("guiding_started", "");
        }
        else if constexpr (std::is_same_v<T, phd2::GuidingStoppedEvent>) {
            guiderState_.store(GuiderState::Stopped);
            emitEvent("guiding_stopped", "");
        }
        else if constexpr (std::is_same_v<T, phd2::PausedEvent>) {
            guiderState_.store(GuiderState::Paused);
            emitEvent("paused", "");
        }
        else if constexpr (std::is_same_v<T, phd2::ResumedEvent>) {
            guiderState_.store(GuiderState::Guiding);
            emitEvent("resumed", "");
        }
    }, event);
}

// Register with client registry
LITHIUM_REGISTER_CLIENT(PHD2Client, "phd2", "PHD2 Guiding Software",
                        ClientType::Guider, "1.0.0")

}  // namespace lithium::client
