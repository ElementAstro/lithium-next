/*
 * dome_service.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "dome_service.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <thread>

#include "atom/log/spdlog_logger.hpp"

#include "device/template/dome.hpp"

namespace lithium::device {

using json = nlohmann::json;

// MockDome implementation for simulation
class MockDome : public AtomDome {
private:
    bool connected_ = false;
    double azimuth_ = 0.0;
    double target_azimuth_ = 0.0;
    ShutterStatus shutter_ = ShutterStatus::CLOSED;
    DomeState state_ = DomeState::IDLE;
    bool parked_ = true;
    bool slaved_ = false;
    mutable std::mutex mutex_;

    std::thread sim_thread_;
    bool running_ = true;

public:
    MockDome() : AtomDome("Mock Dome") {
        sim_thread_ = std::thread([this]() {
            while (running_) {
                std::this_thread::sleep_for(std::chrono::milliseconds(100));
                updateSimulation();
            }
        });
    }

    ~MockDome() {
        running_ = false;
        if (sim_thread_.joinable()) {
            sim_thread_.join();
        }
    }

    void updateSimulation() {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connected_)
            return;

        if (state_ == DomeState::MOVING) {
            double diff = target_azimuth_ - azimuth_;
            if (std::abs(diff) < 1.0) {
                azimuth_ = target_azimuth_;
                state_ = DomeState::IDLE;
            } else {
                double step = 2.0;
                if (diff > 0)
                    azimuth_ += step;
                else
                    azimuth_ -= step;

                if (azimuth_ >= 360.0)
                    azimuth_ -= 360.0;
                if (azimuth_ < 0.0)
                    azimuth_ += 360.0;
            }
        }

        if (shutter_ == ShutterStatus::OPENING) {
            shutter_ = ShutterStatus::OPEN;
        } else if (shutter_ == ShutterStatus::CLOSING) {
            shutter_ = ShutterStatus::CLOSED;
        }
    }

    bool initialize() override { return true; }
    bool destroy() override { return true; }
    std::vector<std::string> scan() override { return {}; }

    bool connect(const std::string& connection_string) override {
        std::lock_guard<std::mutex> lock(mutex_);
        (void)connection_string;
        connected_ = true;
        return true;
    }

    bool connect(const std::string& port, int timeout, int maxRetry) override {
        (void)port;
        (void)timeout;
        (void)maxRetry;
        std::lock_guard<std::mutex> lock(mutex_);
        connected_ = true;
        return true;
    }

    bool disconnect() override {
        std::lock_guard<std::mutex> lock(mutex_);
        connected_ = false;
        return true;
    }

    bool isConnected() const override { return connected_; }

    auto getAzimuth() -> std::optional<double> override {
        std::lock_guard<std::mutex> lock(mutex_);
        return azimuth_;
    }

    auto getAltitude() -> std::optional<double> override { return 90.0; }

    auto setAzimuth(double azimuth) -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connected_)
            return false;
        if (parked_)
            parked_ = false;
        target_azimuth_ = azimuth;
        state_ = DomeState::MOVING;
        return true;
    }

    auto setAltitude(double altitude) -> bool override {
        (void)altitude;
        return false;
    }

    auto getShutterStatus() -> std::optional<ShutterStatus> override {
        std::lock_guard<std::mutex> lock(mutex_);
        return shutter_;
    }

    auto openShutter() -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connected_)
            return false;
        if (shutter_ == ShutterStatus::OPEN)
            return true;
        shutter_ = ShutterStatus::OPENING;
        return true;
    }

    auto closeShutter() -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connected_)
            return false;
        if (shutter_ == ShutterStatus::CLOSED)
            return true;
        shutter_ = ShutterStatus::CLOSING;
        return true;
    }

    auto isParked() -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        return parked_;
    }

    auto park() -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connected_)
            return false;
        target_azimuth_ = 0.0;
        state_ = DomeState::MOVING;
        parked_ = true;
        return true;
    }

    auto unpark() -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!connected_)
            return false;
        parked_ = false;
        return true;
    }

    auto findHome() -> bool override { return park(); }

    auto getDomeState() -> std::optional<DomeState> override {
        std::lock_guard<std::mutex> lock(mutex_);
        return state_;
    }

    auto canSlave() -> bool override { return true; }
    auto setSlaved(bool slaved) -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        slaved_ = slaved;
        return true;
    }
    auto isSlaved() -> bool override {
        std::lock_guard<std::mutex> lock(mutex_);
        return slaved_;
    }
};

class DomeService::Impl {
public:
    std::shared_ptr<MockDome> mockDome = std::make_shared<MockDome>();
};

DomeService::DomeService()
    : BaseDeviceService("DomeService"),
      impl_(std::make_unique<Impl>()) {}

DomeService::~DomeService() = default;

auto DomeService::list() -> json {
    LOG_INFO( "DomeService::list: Listing all available domes");
    json response;
    response["status"] = "success";
    json list = json::array();
    json info;
    info["deviceId"] = "dom-001";
    info["name"] = impl_->mockDome->getName();
    info["connected"] = impl_->mockDome->isConnected();
    list.push_back(info);
    response["data"] = list;
    return response;
}

auto DomeService::getStatus(const std::string& deviceId) -> json {
    LOG_INFO( "DomeService::getStatus: %s", deviceId.c_str());
    json response;

    if (deviceId != "dom-001") {
        response["status"] = "error";
        response["error"] = {{"code", "device_not_found"},
                             {"message", "Device not found"}};
        return response;
    }

    if (!impl_->mockDome->isConnected()) {
        response["status"] = "error";
        response["error"] = {{"code", "device_not_connected"},
                             {"message", "Dome not connected"}};
        return response;
    }

    json data;
    data["connected"] = true;
    auto az = impl_->mockDome->getAzimuth();
    data["azimuth"] = az.value_or(0.0);
    data["altitude"] = impl_->mockDome->getAltitude().value_or(90.0);

    auto shutter = impl_->mockDome->getShutterStatus();
    std::string shutterStr = "Unknown";
    if (shutter) {
        switch (*shutter) {
            case ShutterStatus::OPEN:
                shutterStr = "Open";
                break;
            case ShutterStatus::CLOSED:
                shutterStr = "Closed";
                break;
            case ShutterStatus::OPENING:
                shutterStr = "Opening";
                break;
            case ShutterStatus::CLOSING:
                shutterStr = "Closing";
                break;
            case ShutterStatus::ERROR:
                shutterStr = "Error";
                break;
            default:
                break;
        }
    }
    data["shutterStatus"] = shutterStr;

    auto state = impl_->mockDome->getDomeState();
    std::string stateStr = "Idle";
    if (state) {
        switch (*state) {
            case DomeState::MOVING:
                stateStr = "Moving";
                break;
            case DomeState::PARKED:
                stateStr = "Parked";
                break;
            case DomeState::PARKING:
                stateStr = "Parking";
                break;
            case DomeState::UNPARKING:
                stateStr = "Unparking";
                break;
            case DomeState::ERROR:
                stateStr = "Error";
                break;
            default:
                break;
        }
    }
    data["status"] = stateStr;
    data["slaved"] = impl_->mockDome->isSlaved();
    data["parked"] = impl_->mockDome->isParked();

    response["status"] = "success";
    response["data"] = data;
    return response;
}

auto DomeService::connect(const std::string& deviceId, bool connected) -> json {
    LOG_INFO( "DomeService::connect: %s %s", deviceId.c_str(),
          connected ? "connect" : "disconnect");
    json response;
    if (deviceId != "dom-001") {
        response["status"] = "error";
        response["error"] = {{"code", "device_not_found"},
                             {"message", "Device not found"}};
        return response;
    }

    bool success =
        connected ? impl_->mockDome->connect("") : impl_->mockDome->disconnect();
    if (success) {
        response["status"] = "success";
        response["message"] = connected ? "Dome connected" : "Dome disconnected";
    } else {
        response["status"] = "error";
        response["error"] = {{"code", "connection_failed"}, {"message", "Failed"}};
    }
    return response;
}

auto DomeService::slew(const std::string& deviceId, double azimuth) -> json {
    LOG_INFO( "DomeService::slew: %s to %f", deviceId.c_str(), azimuth);
    json response;
    if (deviceId != "dom-001") {
        response["status"] = "error";
        response["error"] = {{"code", "device_not_found"},
                             {"message", "Device not found"}};
        return response;
    }

    if (!impl_->mockDome->isConnected()) {
        response["status"] = "error";
        response["error"] = {{"code", "device_not_connected"},
                             {"message", "Not connected"}};
        return response;
    }

    if (impl_->mockDome->setAzimuth(azimuth)) {
        response["status"] = "success";
        response["message"] = "Slewing initiated";
    } else {
        response["status"] = "error";
        response["error"] = {{"code", "slew_failed"}, {"message", "Slew failed"}};
    }
    return response;
}

auto DomeService::shutterControl(const std::string& deviceId, bool open) -> json {
    LOG_INFO( "DomeService::shutterControl: %s %s", deviceId.c_str(),
          open ? "open" : "close");
    json response;
    if (deviceId != "dom-001") {
        response["status"] = "error";
        response["error"] = {{"code", "device_not_found"},
                             {"message", "Device not found"}};
        return response;
    }

    if (!impl_->mockDome->isConnected()) {
        response["status"] = "error";
        response["error"] = {{"code", "device_not_connected"},
                             {"message", "Not connected"}};
        return response;
    }

    bool success =
        open ? impl_->mockDome->openShutter() : impl_->mockDome->closeShutter();
    if (success) {
        response["status"] = "success";
        response["message"] = open ? "Opening shutter" : "Closing shutter";
    } else {
        response["status"] = "error";
        response["error"] = {{"code", "shutter_failed"},
                             {"message", "Shutter op failed"}};
    }
    return response;
}

auto DomeService::park(const std::string& deviceId) -> json {
    LOG_INFO( "DomeService::park: %s", deviceId.c_str());
    json response;
    if (deviceId != "dom-001") {
        response["status"] = "error";
        response["error"] = {{"code", "device_not_found"},
                             {"message", "Device not found"}};
        return response;
    }

    if (!impl_->mockDome->isConnected()) {
        response["status"] = "error";
        response["error"] = {{"code", "device_not_connected"},
                             {"message", "Not connected"}};
        return response;
    }

    if (impl_->mockDome->park()) {
        response["status"] = "success";
        response["message"] = "Parking initiated";
    } else {
        response["status"] = "error";
        response["error"] = {{"code", "park_failed"}, {"message", "Park failed"}};
    }
    return response;
}

auto DomeService::unpark(const std::string& deviceId) -> json {
    LOG_INFO( "DomeService::unpark: %s", deviceId.c_str());
    json response;
    if (deviceId != "dom-001") {
        response["status"] = "error";
        response["error"] = {{"code", "device_not_found"},
                             {"message", "Device not found"}};
        return response;
    }

    if (!impl_->mockDome->isConnected()) {
        response["status"] = "error";
        response["error"] = {{"code", "device_not_connected"},
                             {"message", "Not connected"}};
        return response;
    }

    if (impl_->mockDome->unpark()) {
        response["status"] = "success";
        response["message"] = "Unparked";
    } else {
        response["status"] = "error";
        response["error"] = {{"code", "unpark_failed"},
                             {"message", "Unpark failed"}};
    }
    return response;
}

auto DomeService::home(const std::string& deviceId) -> json {
    LOG_INFO( "DomeService::home: %s", deviceId.c_str());
    json response;
    if (deviceId != "dom-001") {
        response["status"] = "error";
        response["error"] = {{"code", "device_not_found"},
                             {"message", "Device not found"}};
        return response;
    }

    if (!impl_->mockDome->isConnected()) {
        response["status"] = "error";
        response["error"] = {{"code", "device_not_connected"},
                             {"message", "Not connected"}};
        return response;
    }

    if (impl_->mockDome->findHome()) {
        response["status"] = "success";
        response["message"] = "Homing initiated";
    } else {
        response["status"] = "error";
        response["error"] = {{"code", "home_failed"}, {"message", "Home failed"}};
    }
    return response;
}

auto DomeService::stop(const std::string& deviceId) -> json {
    LOG_INFO( "DomeService::stop: %s", deviceId.c_str());
    json response;
    if (deviceId != "dom-001") {
        response["status"] = "error";
        response["error"] = {{"code", "device_not_found"},
                             {"message", "Device not found"}};
        return response;
    }
    response["status"] = "success";
    response["message"] = "Stopped";
    return response;
}

auto DomeService::getCapabilities(const std::string& deviceId) -> json {
    (void)deviceId;
    json response;
    response["status"] = "success";

    json caps;
    caps["canPark"] = true;
    caps["canFindHome"] = true;
    caps["canSlaved"] = true;
    caps["hasShutter"] = true;
    caps["canAzimuth"] = true;
    caps["canAltitude"] = false;

    response["data"] = caps;
    return response;
}

// ========== INDI-specific operations ==========

auto DomeService::getINDIProperties(const std::string& deviceId) -> json {
    if (deviceId != "dom-001") {
        return makeErrorResponse(ErrorCode::DEVICE_NOT_FOUND, "Device not found");
    }

    if (!impl_->mockDome->isConnected()) {
        return makeErrorResponse(ErrorCode::DEVICE_NOT_CONNECTED, "Dome not connected");
    }

    json data;
    data["driverName"] = "INDI Dome";
    data["driverVersion"] = "1.0";

    json properties = json::object();

    // Azimuth
    if (auto az = impl_->mockDome->getAzimuth()) {
        properties["ABS_DOME_POSITION"] = {
            {"value", *az},
            {"type", "number"}
        };
    }

    // Shutter status
    if (auto shutter = impl_->mockDome->getShutterStatus()) {
        std::string status = "Unknown";
        switch (*shutter) {
            case ShutterStatus::OPEN:
                status = "Open";
                break;
            case ShutterStatus::CLOSED:
                status = "Closed";
                break;
            case ShutterStatus::OPENING:
                status = "Opening";
                break;
            case ShutterStatus::CLOSING:
                status = "Closing";
                break;
            default:
                break;
        }
        properties["DOME_SHUTTER"] = {
            {"value", status},
            {"type", "text"}
        };
    }

    // Park status
    properties["DOME_PARK"] = {
        {"value", impl_->mockDome->isParked()},
        {"type", "switch"}
    };

    // Slaved status
    properties["DOME_AUTOSYNC"] = {
        {"value", impl_->mockDome->isSlaved()},
        {"type", "switch"}
    };

    data["properties"] = properties;
    return makeSuccessResponse(data);
}

auto DomeService::setINDIProperty(const std::string& deviceId,
                                  const std::string& propertyName,
                                  const json& value) -> json {
    if (deviceId != "dom-001") {
        return makeErrorResponse(ErrorCode::DEVICE_NOT_FOUND, "Device not found");
    }

    if (!impl_->mockDome->isConnected()) {
        return makeErrorResponse(ErrorCode::DEVICE_NOT_CONNECTED, "Dome not connected");
    }

    bool success = false;

    if (propertyName == "ABS_DOME_POSITION" && value.is_number()) {
        success = impl_->mockDome->setAzimuth(value.get<double>());
    } else if (propertyName == "DOME_SHUTTER" && value.is_string()) {
        std::string cmd = value.get<std::string>();
        if (cmd == "OPEN") {
            success = impl_->mockDome->openShutter();
        } else if (cmd == "CLOSE") {
            success = impl_->mockDome->closeShutter();
        }
    } else if (propertyName == "DOME_PARK" && value.is_boolean()) {
        if (value.get<bool>()) {
            success = impl_->mockDome->park();
        } else {
            success = impl_->mockDome->unpark();
        }
    } else if (propertyName == "DOME_AUTOSYNC" && value.is_boolean()) {
        success = impl_->mockDome->setSlaved(value.get<bool>());
    } else {
        return makeErrorResponse(ErrorCode::INVALID_FIELD_VALUE,
                                 "Unknown or invalid property: " + propertyName);
    }

    if (success) {
        return makeSuccessResponse("Property " + propertyName + " updated");
    }
    return makeErrorResponse(ErrorCode::OPERATION_FAILED,
                             "Failed to set property " + propertyName);
}

auto DomeService::setSlaved(const std::string& deviceId, bool slaved) -> json {
    if (deviceId != "dom-001") {
        return makeErrorResponse(ErrorCode::DEVICE_NOT_FOUND, "Device not found");
    }

    if (!impl_->mockDome->isConnected()) {
        return makeErrorResponse(ErrorCode::DEVICE_NOT_CONNECTED, "Dome not connected");
    }

    if (impl_->mockDome->setSlaved(slaved)) {
        json data;
        data["slaved"] = slaved;
        return makeSuccessResponse(data, slaved ? "Dome slaved to mount" : "Dome unslaved");
    }
    return makeErrorResponse(ErrorCode::OPERATION_FAILED, "Failed to set slaved status");
}

auto DomeService::getSlaved(const std::string& deviceId) -> json {
    if (deviceId != "dom-001") {
        return makeErrorResponse(ErrorCode::DEVICE_NOT_FOUND, "Device not found");
    }

    if (!impl_->mockDome->isConnected()) {
        return makeErrorResponse(ErrorCode::DEVICE_NOT_CONNECTED, "Dome not connected");
    }

    json data;
    data["slaved"] = impl_->mockDome->isSlaved();
    data["canSlave"] = impl_->mockDome->canSlave();
    return makeSuccessResponse(data);
}

}  // namespace lithium::device
