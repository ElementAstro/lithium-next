#include "client.h"
#include "exceptions.h"

#include <cmath>

namespace phd2 {

// Internal event handler implementation
class Client::InternalEventHandler : public EventHandler {
public:
    explicit InternalEventHandler(Client* client,
                                  std::shared_ptr<EventHandler> userHandler)
        : client_(client), userHandler_(std::move(userHandler)) {}

    void setUserHandler(std::shared_ptr<EventHandler> userHandler) {
        std::lock_guard<std::mutex> lock(mutex_);
        userHandler_ = std::move(userHandler);
    }

    void onEvent(const Event& event) override {
        // First, handle the event for our internal state management
        handleInternalEvent(event);

        // Then, forward to the user's handler if available
        std::lock_guard<std::mutex> lock(mutex_);
        if (userHandler_) {
            userHandler_->onEvent(event);
        }
    }

    void onConnectionError(const std::string& error) override {
        // Forward to the user's handler if available
        std::lock_guard<std::mutex> lock(mutex_);
        if (userHandler_) {
            userHandler_->onConnectionError(error);
        }
    }

private:
    Client* client_;
    std::shared_ptr<EventHandler> userHandler_;
    std::mutex mutex_;  // To protect userHandler_ from concurrent access

    void handleInternalEvent(const Event& event) {
        // Check for settle done event for async operation completion
        if (std::holds_alternative<SettleDoneEvent>(event)) {
            const auto& settleEvent = std::get<SettleDoneEvent>(event);
            client_->handleSettleDone(settleEvent.status == 0);
        }
    }
};

// Client implementation

Client::Client(std::string host, int port,
               std::shared_ptr<EventHandler> eventHandler)
    : eventHandler_(std::move(eventHandler)),
      host_(std::move(host)),
      port_(port) {
    // Create the internal event handler
    internalHandler_ =
        std::make_shared<InternalEventHandler>(this, eventHandler_);

    // Create the connection
    connection_ = std::make_unique<Connection>(host_, port_, internalHandler_);
}

Client::~Client() { disconnect(); }

bool Client::connect(int timeoutMs) { return connection_->connect(timeoutMs); }

void Client::disconnect() {
    if (connection_) {
        connection_->disconnect();
    }
}

bool Client::isConnected() const {
    return connection_ && connection_->isConnected();
}

void Client::setEventHandler(std::shared_ptr<EventHandler> handler) {
    eventHandler_ = handler;
    if (internalHandler_) {
        internalHandler_->setUserHandler(handler);
    }
}

void Client::handleSettleDone(bool success) {
    std::lock_guard<std::mutex> lock(promiseMutex_);
    if (settleInProgress_) {
        settlePromise_.set_value(success);
        settleInProgress_ = false;
    }
}

// Camera control methods

int Client::getExposure() {
    auto response = connection_->sendRpc("get_exposure");
    return response.result.get<int>();
}

void Client::setExposure(int exposureMs) {
    connection_->sendRpc("set_exposure", {exposureMs});
}

std::vector<int> Client::getExposureDurations() {
    auto response = connection_->sendRpc("get_exposure_durations");
    return response.result.get<std::vector<int>>();
}

bool Client::getUseSubframes() {
    auto response = connection_->sendRpc("get_use_subframes");
    return response.result.get<bool>();
}

void Client::captureSingleFrame(std::optional<int> exposureMs,
                                std::optional<std::array<int, 4>> subframe) {
    json params = json::array();

    if (exposureMs) {
        params.push_back(exposureMs.value());
        if (subframe) {
            params.push_back(subframe.value());
        }
    }

    connection_->sendRpc("capture_single_frame", params);
}

std::array<int, 2> Client::getCameraFrameSize() {
    auto response = connection_->sendRpc("get_camera_frame_size");
    return response.result.get<std::array<int, 2>>();
}

double Client::getCcdTemperature() {
    auto response = connection_->sendRpc("get_ccd_temperature");
    return response.result["temperature"].get<double>();
}

json Client::getCoolerStatus() {
    auto response = connection_->sendRpc("get_cooler_status");
    return response.result;
}

std::string Client::saveImage() {
    auto response = connection_->sendRpc("save_image");
    return response.result["filename"].get<std::string>();
}

json Client::getStarImage(std::optional<int> size) {
    json params = json::array();
    if (size) {
        params.push_back(size.value());
    }

    auto response = connection_->sendRpc("get_star_image", params);
    return response.result;
}

// Equipment control methods

bool Client::getConnected() {
    auto response = connection_->sendRpc("get_connected");
    return response.result.get<bool>();
}

void Client::setConnected(bool connect) {
    connection_->sendRpc("set_connected", {connect});
}

json Client::getCurrentEquipment() {
    auto response = connection_->sendRpc("get_current_equipment");
    return response.result;
}

json Client::getProfiles() {
    auto response = connection_->sendRpc("get_profiles");
    return response.result;
}

json Client::getProfile() {
    auto response = connection_->sendRpc("get_profile");
    return response.result;
}

void Client::setProfile(int profileId) {
    connection_->sendRpc("set_profile", {profileId});
}

// Guiding control methods

std::future<bool> Client::startGuiding(
    const SettleParams& settle, bool recalibrate,
    const std::optional<std::array<int, 4>>& roi) {
    // Create a promise for the settle done event
    std::lock_guard<std::mutex> lock(promiseMutex_);
    if (settleInProgress_) {
        throw InvalidStateException(
            "Another operation that requires settling is already in progress");
    }

    settlePromise_ = std::promise<bool>();
    settleInProgress_ = true;

    // Build the parameters
    json params = {{"settle", settle.toJson()}};
    if (recalibrate) {
        params["recalibrate"] = true;
    }
    if (roi) {
        params["roi"] = roi.value();
    }

    // Send the RPC
    connection_->sendRpc("guide", params);

    // Return the future
    return settlePromise_.get_future();
}

void Client::stopCapture() { connection_->sendRpc("stop_capture"); }

void Client::loop() { connection_->sendRpc("loop"); }

std::future<bool> Client::dither(double amount, bool raOnly,
                                 const SettleParams& settle) {
    // Create a promise for the settle done event
    std::lock_guard<std::mutex> lock(promiseMutex_);
    if (settleInProgress_) {
        throw InvalidStateException(
            "Another operation that requires settling is already in progress");
    }

    settlePromise_ = std::promise<bool>();
    settleInProgress_ = true;

    // Build the parameters
    json params = {
        {"amount", amount}, {"raOnly", raOnly}, {"settle", settle.toJson()}};

    // Send the RPC
    connection_->sendRpc("dither", params);

    // Return the future
    return settlePromise_.get_future();
}

AppStateType Client::getAppState() {
    auto response = connection_->sendRpc("get_app_state");
    return appStateFromString(response.result.get<std::string>());
}

void Client::guidePulse(int amount, const std::string& direction,
                        const std::string& which) {
    json params = {amount, direction};
    if (which != "Mount") {
        params.push_back(which);
    }
    connection_->sendRpc("guide_pulse", params);
}

bool Client::getPaused() {
    auto response = connection_->sendRpc("get_paused");
    return response.result.get<bool>();
}

void Client::setPaused(bool pause, bool full) {
    json params = {pause};
    if (pause && full) {
        params.push_back("full");
    }
    connection_->sendRpc("set_paused", params);
}

bool Client::getGuideOutputEnabled() {
    auto response = connection_->sendRpc("get_guide_output_enabled");
    return response.result.get<bool>();
}

void Client::setGuideOutputEnabled(bool enable) {
    connection_->sendRpc("set_guide_output_enabled", {enable});
}

json Client::getVariableDelaySettings() {
    auto response = connection_->sendRpc("get_variable_delay_settings");
    return response.result;
}

void Client::setVariableDelaySettings(const json& settings) {
    connection_->sendRpc("set_variable_delay_settings", settings);
}

// Calibration methods

bool Client::isCalibrated() {
    auto response = connection_->sendRpc("get_calibrated");
    return response.result.get<bool>();
}

void Client::clearCalibration(const std::string& which) {
    connection_->sendRpc("clear_calibration", {which});
}

void Client::flipCalibration() { connection_->sendRpc("flip_calibration"); }

json Client::getCalibrationData(const std::string& which) {
    auto response = connection_->sendRpc("get_calibration_data", {which});
    return response.result;
}

// Guide algorithm methods

void Client::setDecGuideMode(const std::string& mode) {
    connection_->sendRpc("set_dec_guide_mode", {mode});
}

std::string Client::getDecGuideMode() {
    auto response = connection_->sendRpc("get_dec_guide_mode");
    return response.result.get<std::string>();
}

void Client::setAlgoParam(const std::string& axis, const std::string& name,
                          double value) {
    connection_->sendRpc("set_algo_param", {axis, name, value});
}

double Client::getAlgoParam(const std::string& axis, const std::string& name) {
    auto response = connection_->sendRpc("get_algo_param", {axis, name});
    return response.result.get<double>();
}

std::vector<std::string> Client::getAlgoParamNames(const std::string& axis) {
    auto response = connection_->sendRpc("get_algo_param_names", {axis});
    return response.result.get<std::vector<std::string>>();
}

// Star selection methods

std::array<double, 2> Client::findStar(
    const std::optional<std::array<int, 4>>& roi) {
    json params = json::array();
    if (roi) {
        params.push_back(roi.value());
    }

    auto response = connection_->sendRpc("find_star", params);
    return response.result.get<std::array<double, 2>>();
}

std::optional<std::array<double, 2>> Client::getLockPosition() {
    auto response = connection_->sendRpc("get_lock_position");

    if (response.result.is_null()) {
        return std::nullopt;
    }

    return response.result.get<std::array<double, 2>>();
}

void Client::setLockPosition(double x, double y, bool exact) {
    connection_->sendRpc("set_lock_position", {x, y, exact});
}

int Client::getSearchRegion() {
    auto response = connection_->sendRpc("get_search_region");
    return response.result.get<int>();
}

double Client::getPixelScale() {
    auto response = connection_->sendRpc("get_pixel_scale");
    return response.result.get<double>();
}

// Lock shift methods

bool Client::getLockShiftEnabled() {
    auto response = connection_->sendRpc("get_lock_shift_enabled");
    return response.result.get<bool>();
}

void Client::setLockShiftEnabled(bool enable) {
    connection_->sendRpc("set_lock_shift_enabled", {enable});
}

json Client::getLockShiftParams() {
    auto response = connection_->sendRpc("get_lock_shift_params");
    return response.result;
}

void Client::setLockShiftParams(const json& params) {
    connection_->sendRpc("set_lock_shift_params", params);
}

// Advanced methods

void Client::shutdown() { connection_->sendRpc("shutdown"); }

}  // namespace phd2