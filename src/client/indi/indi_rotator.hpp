/*
 * indi_rotator.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI Rotator Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_ROTATOR_HPP
#define LITHIUM_CLIENT_INDI_ROTATOR_HPP

#include "indi_device_base.hpp"

namespace lithium::client::indi {

/**
 * @brief Rotator state enumeration
 */
enum class RotatorState : uint8_t { Idle, Rotating, Error, Unknown };

/**
 * @brief Rotator position information
 */
struct RotatorPosition {
    double angle{0.0};      // Current angle in degrees
    double targetAngle{0.0}; // Target angle
    double minAngle{0.0};
    double maxAngle{360.0};

    [[nodiscard]] auto toJson() const -> json {
        return {{"angle", angle},
                {"targetAngle", targetAngle},
                {"minAngle", minAngle},
                {"maxAngle", maxAngle}};
    }
};

/**
 * @brief INDI Rotator Device
 *
 * Provides rotator-specific functionality including:
 * - Angle control
 * - Reverse motion
 * - Synchronization
 */
class INDIRotator : public INDIDeviceBase {
public:
    static constexpr int DEFAULT_TIMEOUT_MS = 5000;

    /**
     * @brief Construct a new INDI Rotator
     * @param name Device name
     */
    explicit INDIRotator(std::string name);

    /**
     * @brief Destructor
     */
    ~INDIRotator() override;

    // ==================== Device Type ====================

    [[nodiscard]] auto getDeviceType() const -> std::string override {
        return "Rotator";
    }

    // ==================== Connection ====================

    auto connect(const std::string& deviceName, int timeout = DEFAULT_TIMEOUT_MS,
                 int maxRetry = 3) -> bool override;

    auto disconnect() -> bool override;

    // ==================== Angle Control ====================

    /**
     * @brief Set rotator angle
     * @param angle Target angle in degrees (0-360)
     * @return true if rotation started
     */
    auto setAngle(double angle) -> bool;

    /**
     * @brief Get current angle
     * @return Angle in degrees, or nullopt if not available
     */
    [[nodiscard]] auto getAngle() const -> std::optional<double>;

    /**
     * @brief Get position information
     * @return Position info
     */
    [[nodiscard]] auto getPositionInfo() const -> RotatorPosition;

    /**
     * @brief Abort current rotation
     * @return true if aborted
     */
    auto abortRotation() -> bool;

    /**
     * @brief Check if rotator is rotating
     * @return true if rotating
     */
    [[nodiscard]] auto isRotating() const -> bool;

    /**
     * @brief Wait for rotation to complete
     * @param timeout Timeout in milliseconds
     * @return true if rotation completed
     */
    auto waitForRotation(std::chrono::milliseconds timeout =
                             std::chrono::seconds(60)) -> bool;

    // ==================== Synchronization ====================

    /**
     * @brief Sync rotator to specified angle
     * @param angle Angle to sync to
     * @return true if synced
     */
    auto syncAngle(double angle) -> bool;

    // ==================== Reverse ====================

    /**
     * @brief Set reverse motion
     * @param reversed true to reverse
     * @return true if set successfully
     */
    auto setReversed(bool reversed) -> bool;

    /**
     * @brief Check if motion is reversed
     * @return true if reversed
     */
    [[nodiscard]] auto isReversed() const -> std::optional<bool>;

    // ==================== Status ====================

    /**
     * @brief Get rotator state
     * @return Rotator state
     */
    [[nodiscard]] auto getRotatorState() const -> RotatorState;

    /**
     * @brief Get rotator status as JSON
     * @return Status JSON
     */
    [[nodiscard]] auto getStatus() const -> json override;

protected:
    // ==================== Property Handlers ====================

    void onPropertyDefined(const INDIProperty& property) override;
    void onPropertyUpdated(const INDIProperty& property) override;

private:
    // ==================== Internal Methods ====================

    void handleAngleProperty(const INDIProperty& property);
    void handleReverseProperty(const INDIProperty& property);

    void setupPropertyWatchers();

    // ==================== Member Variables ====================

    // Rotator state
    std::atomic<RotatorState> rotatorState_{RotatorState::Idle};
    std::atomic<bool> isRotating_{false};

    // Position
    RotatorPosition positionInfo_;
    mutable std::mutex positionMutex_;
    std::condition_variable rotationCondition_;

    // Reverse
    std::atomic<bool> isReversed_{false};
};

}  // namespace lithium::client::indi

#endif  // LITHIUM_CLIENT_INDI_ROTATOR_HPP
