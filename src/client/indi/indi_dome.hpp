/*
 * indi_dome.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI Dome Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_DOME_HPP
#define LITHIUM_CLIENT_INDI_DOME_HPP

#include "indi_device_base.hpp"

namespace lithium::client::indi {

/**
 * @brief Dome state enumeration
 */
enum class DomeState : uint8_t {
    Idle,
    Moving,
    Opening,
    Closing,
    Parked,
    Error,
    Unknown
};

/**
 * @brief Shutter state enumeration
 */
enum class ShutterState : uint8_t { Open, Closed, Opening, Closing, Unknown };

/**
 * @brief Dome motion direction
 */
enum class DomeMotion : uint8_t { Clockwise, CounterClockwise, None };

/**
 * @brief Dome position information
 */
struct DomePosition {
    double azimuth{0.0};       // Current azimuth in degrees
    double targetAzimuth{0.0}; // Target azimuth
    double minAzimuth{0.0};
    double maxAzimuth{360.0};

    [[nodiscard]] auto toJson() const -> json {
        return {{"azimuth", azimuth},
                {"targetAzimuth", targetAzimuth},
                {"minAzimuth", minAzimuth},
                {"maxAzimuth", maxAzimuth}};
    }
};

/**
 * @brief Dome shutter information
 */
struct ShutterInfo {
    ShutterState state{ShutterState::Unknown};
    bool hasShutter{false};

    [[nodiscard]] auto toJson() const -> json {
        return {{"state", static_cast<int>(state)}, {"hasShutter", hasShutter}};
    }
};

/**
 * @brief Dome park information
 */
struct DomeParkInfo {
    bool parked{false};
    bool parkEnabled{false};
    double parkAzimuth{0.0};

    [[nodiscard]] auto toJson() const -> json {
        return {{"parked", parked},
                {"parkEnabled", parkEnabled},
                {"parkAzimuth", parkAzimuth}};
    }
};

/**
 * @brief INDI Dome Device
 *
 * Provides dome-specific functionality including:
 * - Azimuth control
 * - Shutter control
 * - Parking
 * - Telescope synchronization
 */
class INDIDome : public INDIDeviceBase {
public:
    static constexpr int DEFAULT_TIMEOUT_MS = 5000;

    /**
     * @brief Construct a new INDI Dome
     * @param name Device name
     */
    explicit INDIDome(std::string name);

    /**
     * @brief Destructor
     */
    ~INDIDome() override;

    // ==================== Device Type ====================

    [[nodiscard]] auto getDeviceType() const -> std::string override {
        return "Dome";
    }

    // ==================== Connection ====================

    auto connect(const std::string& deviceName, int timeout = DEFAULT_TIMEOUT_MS,
                 int maxRetry = 3) -> bool override;

    auto disconnect() -> bool override;

    // ==================== Azimuth Control ====================

    /**
     * @brief Set dome azimuth
     * @param azimuth Target azimuth in degrees (0-360)
     * @return true if motion started
     */
    auto setAzimuth(double azimuth) -> bool;

    /**
     * @brief Get current azimuth
     * @return Azimuth in degrees, or nullopt if not available
     */
    [[nodiscard]] auto getAzimuth() const -> std::optional<double>;

    /**
     * @brief Get position information
     * @return Position info
     */
    [[nodiscard]] auto getPositionInfo() const -> DomePosition;

    /**
     * @brief Abort current motion
     * @return true if aborted
     */
    auto abortMotion() -> bool;

    /**
     * @brief Check if dome is moving
     * @return true if moving
     */
    [[nodiscard]] auto isMoving() const -> bool;

    /**
     * @brief Wait for motion to complete
     * @param timeout Timeout in milliseconds
     * @return true if motion completed
     */
    auto waitForMotion(std::chrono::milliseconds timeout =
                           std::chrono::minutes(5)) -> bool;

    /**
     * @brief Move dome in specified direction
     * @param direction Motion direction
     * @return true if motion started
     */
    auto move(DomeMotion direction) -> bool;

    /**
     * @brief Stop dome motion
     * @return true if stopped
     */
    auto stop() -> bool;

    // ==================== Shutter Control ====================

    /**
     * @brief Open dome shutter
     * @return true if opening started
     */
    auto openShutter() -> bool;

    /**
     * @brief Close dome shutter
     * @return true if closing started
     */
    auto closeShutter() -> bool;

    /**
     * @brief Get shutter state
     * @return Shutter state
     */
    [[nodiscard]] auto getShutterState() const -> ShutterState;

    /**
     * @brief Get shutter information
     * @return Shutter info
     */
    [[nodiscard]] auto getShutterInfo() const -> ShutterInfo;

    /**
     * @brief Check if dome has shutter
     * @return true if has shutter
     */
    [[nodiscard]] auto hasShutter() const -> bool;

    // ==================== Parking ====================

    /**
     * @brief Park the dome
     * @return true if parking started
     */
    auto park() -> bool;

    /**
     * @brief Unpark the dome
     * @return true if unparked
     */
    auto unpark() -> bool;

    /**
     * @brief Check if dome is parked
     * @return true if parked
     */
    [[nodiscard]] auto isParked() const -> bool;

    /**
     * @brief Set park position
     * @param azimuth Park azimuth
     * @return true if set successfully
     */
    auto setParkPosition(double azimuth) -> bool;

    /**
     * @brief Get park information
     * @return Park info
     */
    [[nodiscard]] auto getDomeParkInfo() const -> DomeParkInfo;

    // ==================== Telescope Sync ====================

    /**
     * @brief Enable/disable telescope synchronization
     * @param enable true to enable
     * @return true if set successfully
     */
    auto enableTelescopeSync(bool enable) -> bool;

    /**
     * @brief Check if telescope sync is enabled
     * @return true if enabled
     */
    [[nodiscard]] auto isTelescopeSyncEnabled() const -> bool;

    /**
     * @brief Sync dome to telescope position
     * @return true if synced
     */
    auto syncToTelescope() -> bool;

    // ==================== Status ====================

    /**
     * @brief Get dome state
     * @return Dome state
     */
    [[nodiscard]] auto getDomeState() const -> DomeState;

    /**
     * @brief Get dome status as JSON
     * @return Status JSON
     */
    [[nodiscard]] auto getStatus() const -> json override;

protected:
    // ==================== Property Handlers ====================

    void onPropertyDefined(const INDIProperty& property) override;
    void onPropertyUpdated(const INDIProperty& property) override;

private:
    // ==================== Internal Methods ====================

    void handleAzimuthProperty(const INDIProperty& property);
    void handleShutterProperty(const INDIProperty& property);
    void handleParkProperty(const INDIProperty& property);
    void handleMotionProperty(const INDIProperty& property);

    void setupPropertyWatchers();

    // ==================== Member Variables ====================

    // Dome state
    std::atomic<DomeState> domeState_{DomeState::Idle};
    std::atomic<bool> isMoving_{false};

    // Position
    DomePosition positionInfo_;
    mutable std::mutex positionMutex_;
    std::condition_variable motionCondition_;

    // Shutter
    ShutterInfo shutterInfo_;
    mutable std::mutex shutterMutex_;

    // Parking
    DomeParkInfo parkInfo_;
    mutable std::mutex parkMutex_;

    // Telescope sync
    std::atomic<bool> telescopeSyncEnabled_{false};

    // Motion
    std::atomic<DomeMotion> currentMotion_{DomeMotion::None};
};

}  // namespace lithium::client::indi

#endif  // LITHIUM_CLIENT_INDI_DOME_HPP
