/*
 * indigo_dome.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDIGO Dome Device Implementation

**************************************************/

#include "indigo_dome.hpp"

#include <cmath>
#include <mutex>

#include "atom/log/loguru.hpp"

namespace lithium::client::indigo {

// ============================================================================
// State conversion functions
// ============================================================================

auto shutterStateFromString(std::string_view str) -> ShutterState {
    if (str == "Open" || str == "OPEN") return ShutterState::Open;
    if (str == "Closed" || str == "CLOSED") return ShutterState::Closed;
    if (str == "Opening" || str == "OPENING") return ShutterState::Opening;
    if (str == "Closing" || str == "CLOSING") return ShutterState::Closing;
    return ShutterState::Unknown;
}

auto parkStateFromString(std::string_view str) -> ParkState {
    if (str == "Parked" || str == "PARKED") return ParkState::Parked;
    if (str == "Unparked" || str == "UNPARKED") return ParkState::Unparked;
    if (str == "Parking" || str == "PARKING") return ParkState::Parking;
    return ParkState::Unknown;
}

auto directionFromString(std::string_view str) -> DomeDirection {
    if (str == "CW" || str == "Clockwise") return DomeDirection::Clockwise;
    if (str == "CCW" || str == "CounterClockwise")
        return DomeDirection::CounterClockwise;
    return DomeDirection::Clockwise;
}

// ============================================================================
// INDIGODome Implementation
// ============================================================================

class INDIGODome::Impl {
public:
    explicit Impl(INDIGODome* parent) : parent_(parent) {}

    void onConnected() {
        // Query and cache initial dome state
        updateDomeStatus();

        LOG_F(INFO, "INDIGO Dome[{}]: Connected and initialized",
              parent_->getINDIGODeviceName());
    }

    void onDisconnected() {
        movementStatus_.moving = false;
        LOG_F(INFO, "INDIGO Dome[{}]: Disconnected",
              parent_->getINDIGODeviceName());
    }

    void onPropertyUpdated(const Property& property) {
        const auto& name = property.name;

        if (name == "DOME_SHUTTER") {
            handleShutterUpdate(property);
        } else if (name == "DOME_HORIZONTAL_COORDINATES") {
            handleAzimuthUpdate(property);
        } else if (name == "DOME_DIRECTION") {
            handleDirectionUpdate(property);
        } else if (name == "DOME_SPEED") {
            handleSpeedUpdate(property);
        } else if (name == "DOME_PARK") {
            handleParkUpdate(property);
        } else if (name == "DOME_SLAVING") {
            handleSlavingUpdate(property);
        } else if (name == "DOME_STEPS") {
            handleStepsUpdate(property);
        }
    }

    auto openShutter() -> DeviceResult<bool> {
        auto result = parent_->setSwitchProperty("DOME_SHUTTER", "OPEN", true);
        if (result.has_value()) {
            LOG_F(INFO, "INDIGO Dome[{}]: Shutter open requested",
                  parent_->getINDIGODeviceName());
        }
        return result;
    }

    auto closeShutter() -> DeviceResult<bool> {
        auto result = parent_->setSwitchProperty("DOME_SHUTTER", "CLOSE", true);
        if (result.has_value()) {
            LOG_F(INFO, "INDIGO Dome[{}]: Shutter close requested",
                  parent_->getINDIGODeviceName());
        }
        return result;
    }

    [[nodiscard]] auto getShutterState() const -> ShutterState {
        return shutterState_;
    }

    [[nodiscard]] auto isShutterOpen() const -> bool {
        return shutterState_ == ShutterState::Open;
    }

    void setShutterCallback(ShutterCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        shutterCallback_ = std::move(callback);
    }

    auto moveToAzimuth(double azimuth) -> DeviceResult<bool> {
        // Normalize azimuth to 0-360 range
        azimuth = std::fmod(azimuth, 360.0);
        if (azimuth < 0.0) azimuth += 360.0;

        auto result = parent_->setNumberProperty(
            "DOME_HORIZONTAL_COORDINATES", "AZ", azimuth);
        if (result.has_value()) {
            movementStatus_.moving = true;
            movementStatus_.targetAzimuth = azimuth;
            movementStatus_.state = PropertyState::Busy;

            LOG_F(INFO, "INDIGO Dome[{}]: Moving to azimuth {:.2f}°",
                  parent_->getINDIGODeviceName(), azimuth);
        }
        return result;
    }

    auto moveRelative(double offset) -> DeviceResult<bool> {
        // Calculate new azimuth from current position
        double newAzimuth = movementStatus_.currentAzimuth + offset;
        return moveToAzimuth(newAzimuth);
    }

    [[nodiscard]] auto getCurrentAzimuth() const -> double {
        return movementStatus_.currentAzimuth;
    }

    [[nodiscard]] auto getTargetAzimuth() const -> double {
        return movementStatus_.targetAzimuth;
    }

    auto abortMotion() -> DeviceResult<bool> {
        auto result =
            parent_->setSwitchProperty("DOME_ABORT_MOTION", "ABORT", true);
        if (result.has_value()) {
            movementStatus_.moving = false;
            movementStatus_.state = PropertyState::Alert;

            LOG_F(INFO, "INDIGO Dome[{}]: Motion aborted",
                  parent_->getINDIGODeviceName());
        }
        return result;
    }

    [[nodiscard]] auto isMoving() const -> bool {
        return movementStatus_.moving;
    }

    [[nodiscard]] auto getMovementStatus() const -> DomeMovementStatus {
        return movementStatus_;
    }

    void setMovementCallback(MovementCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        movementCallback_ = std::move(callback);
    }

    auto setSpeed(double speed) -> DeviceResult<bool> {
        // Clamp speed to valid range
        auto [minSpeed, maxSpeed] = getSpeedRange();
        speed = std::clamp(speed, minSpeed, maxSpeed);

        auto result =
            parent_->setNumberProperty("DOME_SPEED", "SPEED", speed);
        if (result.has_value()) {
            movementStatus_.speed = speed;
            LOG_F(INFO, "INDIGO Dome[{}]: Speed set to {:.2f}°/s",
                  parent_->getINDIGODeviceName(), speed);
        }
        return result;
    }

    [[nodiscard]] auto getSpeed() const -> double {
        return movementStatus_.speed;
    }

    [[nodiscard]] auto getSpeedRange() const -> std::pair<double, double> {
        auto prop = parent_->getProperty("DOME_SPEED");
        if (!prop.has_value() || prop.value().numberElements.empty()) {
            return {0.1, 10.0};  // Default range
        }
        const auto& elem = prop.value().numberElements[0];
        return {elem.min, elem.max};
    }

    auto setDirection(DomeDirection direction) -> DeviceResult<bool> {
        std::string dirName = (direction == DomeDirection::Clockwise) ? "CW" : "CCW";
        auto result = parent_->setSwitchProperty("DOME_DIRECTION", dirName, true);
        if (result.has_value()) {
            movementStatus_.direction = direction;
            LOG_F(INFO, "INDIGO Dome[{}]: Direction set to {}",
                  parent_->getINDIGODeviceName(), directionToString(direction));
        }
        return result;
    }

    [[nodiscard]] auto getDirection() const -> DomeDirection {
        return movementStatus_.direction;
    }

    auto park() -> DeviceResult<bool> {
        auto result = parent_->setSwitchProperty("DOME_PARK", "PARK", true);
        if (result.has_value()) {
            parkState_ = ParkState::Parking;
            LOG_F(INFO, "INDIGO Dome[{}]: Park requested",
                  parent_->getINDIGODeviceName());
        }
        return result;
    }

    auto unpark() -> DeviceResult<bool> {
        auto result = parent_->setSwitchProperty("DOME_PARK", "UNPARK", true);
        if (result.has_value()) {
            parkState_ = ParkState::Unparked;
            LOG_F(INFO, "INDIGO Dome[{}]: Unpark requested",
                  parent_->getINDIGODeviceName());
        }
        return result;
    }

    [[nodiscard]] auto getParkState() const -> ParkState { return parkState_; }

    [[nodiscard]] auto isParked() const -> bool {
        return parkState_ == ParkState::Parked;
    }

    void setParkCallback(ParkCallback callback) {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        parkCallback_ = std::move(callback);
    }

    auto setSlavingEnabled(bool enabled) -> DeviceResult<bool> {
        std::string state = enabled ? "ON" : "OFF";
        auto result =
            parent_->setSwitchProperty("DOME_SLAVING", state, true);
        if (result.has_value()) {
            slavingEnabled_ = enabled;
            LOG_F(INFO, "INDIGO Dome[{}]: Slaving {}",
                  parent_->getINDIGODeviceName(),
                  enabled ? "enabled" : "disabled");
        }
        return result;
    }

    [[nodiscard]] auto isSlavingEnabled() const -> bool {
        return slavingEnabled_;
    }

    auto syncWithMount(const EquatorialCoordinates& coords)
        -> DeviceResult<bool> {
        // Sync dome with mount by setting equatorial coordinates
        auto result = parent_->setNumberProperty(
            "DOME_EQUATORIAL_COORDINATES", "RA",
            coords.rightAscension);
        if (!result.has_value()) return result;

        result = parent_->setNumberProperty(
            "DOME_EQUATORIAL_COORDINATES", "DEC",
            coords.declination);

        if (result.has_value()) {
            LOG_F(INFO,
                  "INDIGO Dome[{}]: Synced with mount RA={:.4f}h, "
                  "DEC={:.4f}°",
                  parent_->getINDIGODeviceName(),
                  coords.rightAscension, coords.declination);
        }
        return result;
    }

    [[nodiscard]] auto getCapabilities() const -> json {
        json caps;
        caps["supportsShutter"] = true;
        caps["supportsSlaving"] = true;
        caps["supportsPark"] = true;
        caps["supportsRelativeMovement"] = true;

        auto [minSpeed, maxSpeed] = getSpeedRange();
        caps["speedRange"] = {{"min", minSpeed}, {"max", maxSpeed}};

        return caps;
    }

    [[nodiscard]] auto getStatus() const -> json {
        json status;
        status["connected"] = parent_->isConnected();
        status["shutterState"] = std::string(shutterStateToString(shutterState_));
        status["parkState"] = std::string(parkStateToString(parkState_));
        status["slavingEnabled"] = slavingEnabled_;
        status["currentAzimuth"] = movementStatus_.currentAzimuth;

        status["movement"] = {
            {"moving", movementStatus_.moving},
            {"targetAzimuth", movementStatus_.targetAzimuth},
            {"speed", movementStatus_.speed},
            {"direction", std::string(directionToString(movementStatus_.direction))},
            {"state", static_cast<int>(movementStatus_.state)}
        };

        return status;
    }

    [[nodiscard]] auto getDomeInfo() const -> json {
        json info;
        auto driverName = parent_->getDriverName();
        auto driverVersion = parent_->getDriverVersion();
        info["driver"] = driverName;
        info["version"] = driverVersion;
        info["capabilities"] = getCapabilities();
        return info;
    }

private:
    void handleShutterUpdate(const Property& property) {
        ShutterState newState = ShutterState::Unknown;

        for (const auto& elem : property.switchElements) {
            if (elem.value) {
                if (elem.name == "OPEN") {
                    newState = ShutterState::Open;
                } else if (elem.name == "CLOSE") {
                    newState = ShutterState::Closed;
                }
            }
        }

        // Handle transient states based on property state
        if (newState == ShutterState::Unknown && property.state == PropertyState::Busy) {
            // Check which direction we're moving
            auto activeSwitch = parent_->getActiveSwitchName("DOME_SHUTTER");
            if (activeSwitch.has_value()) {
                if (activeSwitch.value() == "OPEN") {
                    newState = ShutterState::Opening;
                } else if (activeSwitch.value() == "CLOSE") {
                    newState = ShutterState::Closing;
                }
            }
        }

        if (newState != ShutterState::Unknown) {
            shutterState_ = newState;

            LOG_F(INFO, "INDIGO Dome[{}]: Shutter state = {}",
                  parent_->getINDIGODeviceName(),
                  shutterStateToString(shutterState_));

            // Call callback
            std::lock_guard<std::mutex> lock(callbackMutex_);
            if (shutterCallback_) {
                shutterCallback_(shutterState_);
            }
        }
    }

    void handleAzimuthUpdate(const Property& property) {
        for (const auto& elem : property.numberElements) {
            if (elem.name == "AZ") {
                movementStatus_.currentAzimuth = elem.value;

                // Update movement status based on property state
                if (property.state == PropertyState::Ok) {
                    movementStatus_.moving = false;
                    LOG_F(INFO, "INDIGO Dome[{}]: Reached azimuth {:.2f}°",
                          parent_->getINDIGODeviceName(),
                          movementStatus_.currentAzimuth);
                } else if (property.state == PropertyState::Busy) {
                    movementStatus_.moving = true;
                } else if (property.state == PropertyState::Alert) {
                    movementStatus_.moving = false;
                }

                movementStatus_.state = property.state;

                // Call callback
                std::lock_guard<std::mutex> lock(callbackMutex_);
                if (movementCallback_) {
                    movementCallback_(movementStatus_);
                }
                break;
            }
        }
    }

    void handleDirectionUpdate(const Property& property) {
        for (const auto& elem : property.switchElements) {
            if (elem.value) {
                movementStatus_.direction = directionFromString(elem.name);
                LOG_F(INFO, "INDIGO Dome[{}]: Direction = {}",
                      parent_->getINDIGODeviceName(),
                      directionToString(movementStatus_.direction));
                break;
            }
        }
    }

    void handleSpeedUpdate(const Property& property) {
        for (const auto& elem : property.numberElements) {
            if (elem.name == "SPEED") {
                movementStatus_.speed = elem.value;
                LOG_F(DEBUG, "INDIGO Dome[{}]: Speed = {:.2f}°/s",
                      parent_->getINDIGODeviceName(), movementStatus_.speed);
                break;
            }
        }
    }

    void handleParkUpdate(const Property& property) {
        ParkState newState = ParkState::Unknown;

        for (const auto& elem : property.switchElements) {
            if (elem.value) {
                if (elem.name == "PARK") {
                    newState = ParkState::Parked;
                } else if (elem.name == "UNPARK") {
                    newState = ParkState::Unparked;
                }
            }
        }

        // Handle transient states
        if (newState == ParkState::Unknown && property.state == PropertyState::Busy) {
            auto activeSwitch = parent_->getActiveSwitchName("DOME_PARK");
            if (activeSwitch.has_value()) {
                if (activeSwitch.value() == "PARK") {
                    newState = ParkState::Parking;
                }
            }
        }

        if (newState != ParkState::Unknown) {
            parkState_ = newState;

            LOG_F(INFO, "INDIGO Dome[{}]: Park state = {}",
                  parent_->getINDIGODeviceName(),
                  parkStateToString(parkState_));

            // Call callback
            std::lock_guard<std::mutex> lock(callbackMutex_);
            if (parkCallback_) {
                parkCallback_(parkState_);
            }
        }
    }

    void handleSlavingUpdate(const Property& property) {
        for (const auto& elem : property.switchElements) {
            if (elem.name == "ON" && elem.value) {
                slavingEnabled_ = true;
                LOG_F(INFO, "INDIGO Dome[{}]: Slaving enabled",
                      parent_->getINDIGODeviceName());
                break;
            } else if (elem.name == "OFF" && elem.value) {
                slavingEnabled_ = false;
                LOG_F(INFO, "INDIGO Dome[{}]: Slaving disabled",
                      parent_->getINDIGODeviceName());
                break;
            }
        }
    }

    void handleStepsUpdate(const Property& property) {
        // DOME_STEPS is used for relative movement in steps
        // This is handled transparently by moveRelative()
        LOG_F(DEBUG, "INDIGO Dome[{}]: Steps property updated",
              parent_->getINDIGODeviceName());
    }

    void updateDomeStatus() {
        // Query current state of key properties
        auto shutterProp = parent_->getProperty("DOME_SHUTTER");
        if (shutterProp.has_value()) {
            handleShutterUpdate(shutterProp.value());
        }

        auto azimuthProp =
            parent_->getProperty("DOME_HORIZONTAL_COORDINATES");
        if (azimuthProp.has_value()) {
            handleAzimuthUpdate(azimuthProp.value());
        }

        auto parkProp = parent_->getProperty("DOME_PARK");
        if (parkProp.has_value()) {
            handleParkUpdate(parkProp.value());
        }

        auto slavingProp = parent_->getProperty("DOME_SLAVING");
        if (slavingProp.has_value()) {
            handleSlavingUpdate(slavingProp.value());
        }

        auto speedProp = parent_->getProperty("DOME_SPEED");
        if (speedProp.has_value()) {
            handleSpeedUpdate(speedProp.value());
        }
    }

    INDIGODome* parent_;

    // State variables
    ShutterState shutterState_{ShutterState::Unknown};
    ParkState parkState_{ParkState::Unknown};
    bool slavingEnabled_{false};
    DomeMovementStatus movementStatus_;

    // Callbacks
    std::mutex callbackMutex_;
    ShutterCallback shutterCallback_;
    ParkCallback parkCallback_;
    MovementCallback movementCallback_;
};

// ============================================================================
// INDIGODome public interface
// ============================================================================

INDIGODome::INDIGODome(const std::string& deviceName)
    : INDIGODeviceBase(deviceName, "Dome"),
      domeImpl_(std::make_unique<Impl>(this)) {}

INDIGODome::~INDIGODome() = default;

auto INDIGODome::openShutter() -> DeviceResult<bool> {
    return domeImpl_->openShutter();
}

auto INDIGODome::closeShutter() -> DeviceResult<bool> {
    return domeImpl_->closeShutter();
}

auto INDIGODome::getShutterState() const -> ShutterState {
    return domeImpl_->getShutterState();
}

auto INDIGODome::isShutterOpen() const -> bool {
    return domeImpl_->isShutterOpen();
}

void INDIGODome::setShutterCallback(ShutterCallback callback) {
    domeImpl_->setShutterCallback(std::move(callback));
}

auto INDIGODome::moveToAzimuth(double azimuth) -> DeviceResult<bool> {
    return domeImpl_->moveToAzimuth(azimuth);
}

auto INDIGODome::moveRelative(double offset) -> DeviceResult<bool> {
    return domeImpl_->moveRelative(offset);
}

auto INDIGODome::getCurrentAzimuth() const -> double {
    return domeImpl_->getCurrentAzimuth();
}

auto INDIGODome::getTargetAzimuth() const -> double {
    return domeImpl_->getTargetAzimuth();
}

auto INDIGODome::abortMotion() -> DeviceResult<bool> {
    return domeImpl_->abortMotion();
}

auto INDIGODome::isMoving() const -> bool { return domeImpl_->isMoving(); }

auto INDIGODome::getMovementStatus() const -> DomeMovementStatus {
    return domeImpl_->getMovementStatus();
}

void INDIGODome::setMovementCallback(MovementCallback callback) {
    domeImpl_->setMovementCallback(std::move(callback));
}

auto INDIGODome::setSpeed(double speed) -> DeviceResult<bool> {
    return domeImpl_->setSpeed(speed);
}

auto INDIGODome::getSpeed() const -> double { return domeImpl_->getSpeed(); }

auto INDIGODome::getSpeedRange() const -> std::pair<double, double> {
    return domeImpl_->getSpeedRange();
}

auto INDIGODome::setDirection(DomeDirection direction) -> DeviceResult<bool> {
    return domeImpl_->setDirection(direction);
}

auto INDIGODome::getDirection() const -> DomeDirection {
    return domeImpl_->getDirection();
}

auto INDIGODome::park() -> DeviceResult<bool> { return domeImpl_->park(); }

auto INDIGODome::unpark() -> DeviceResult<bool> {
    return domeImpl_->unpark();
}

auto INDIGODome::getParkState() const -> ParkState {
    return domeImpl_->getParkState();
}

auto INDIGODome::isParked() const -> bool { return domeImpl_->isParked(); }

void INDIGODome::setParkCallback(ParkCallback callback) {
    domeImpl_->setParkCallback(std::move(callback));
}

auto INDIGODome::setSlavingEnabled(bool enabled) -> DeviceResult<bool> {
    return domeImpl_->setSlavingEnabled(enabled);
}

auto INDIGODome::isSlavingEnabled() const -> bool {
    return domeImpl_->isSlavingEnabled();
}

auto INDIGODome::syncWithMount(const EquatorialCoordinates& coords)
    -> DeviceResult<bool> {
    return domeImpl_->syncWithMount(coords);
}

auto INDIGODome::getCapabilities() const -> json {
    return domeImpl_->getCapabilities();
}

auto INDIGODome::getStatus() const -> json { return domeImpl_->getStatus(); }

auto INDIGODome::getDomeInfo() const -> json {
    return domeImpl_->getDomeInfo();
}

void INDIGODome::onConnected() {
    INDIGODeviceBase::onConnected();
    domeImpl_->onConnected();
}

void INDIGODome::onDisconnected() {
    INDIGODeviceBase::onDisconnected();
    domeImpl_->onDisconnected();
}

void INDIGODome::onPropertyUpdated(const Property& property) {
    INDIGODeviceBase::onPropertyUpdated(property);
    domeImpl_->onPropertyUpdated(property);
}

}  // namespace lithium::client::indigo
