/*
 * ascom_focuser.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: ASCOM Focuser Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_ASCOM_FOCUSER_HPP
#define LITHIUM_CLIENT_ASCOM_FOCUSER_HPP

#include "ascom_device_base.hpp"

#include <condition_variable>

namespace lithium::client::ascom {

/**
 * @brief Focuser state enumeration
 */
enum class FocuserState : uint8_t { Idle, Moving, Error };

/**
 * @brief Focuser capabilities
 */
struct FocuserCapabilities {
    bool absolute{false};
    bool canHalt{false};
    bool tempComp{false};
    bool tempCompAvailable{false};

    [[nodiscard]] auto toJson() const -> json {
        return {{"absolute", absolute},
                {"canHalt", canHalt},
                {"tempComp", tempComp},
                {"tempCompAvailable", tempCompAvailable}};
    }
};

/**
 * @brief Focuser position information
 */
struct FocuserPosition {
    int position{0};
    int maxStep{100000};
    int maxIncrement{10000};
    double stepSize{1.0};

    [[nodiscard]] auto toJson() const -> json {
        return {{"position", position},
                {"maxStep", maxStep},
                {"maxIncrement", maxIncrement},
                {"stepSize", stepSize}};
    }
};

/**
 * @brief ASCOM Focuser Device
 *
 * Provides focuser functionality including:
 * - Absolute and relative position control
 * - Temperature compensation
 * - Movement halt
 */
class ASCOMFocuser : public ASCOMDeviceBase {
public:
    /**
     * @brief Construct a new ASCOM Focuser
     * @param name Device name
     * @param deviceNumber Device number
     */
    explicit ASCOMFocuser(std::string name, int deviceNumber = 0);

    /**
     * @brief Destructor
     */
    ~ASCOMFocuser() override;

    // ==================== Device Type ====================

    [[nodiscard]] auto getDeviceType() const -> std::string override {
        return "Focuser";
    }

    // ==================== Connection ====================

    auto connect(int timeout = DEFAULT_TIMEOUT_MS) -> bool override;

    // ==================== Capabilities ====================

    /**
     * @brief Get focuser capabilities
     */
    [[nodiscard]] auto getCapabilities() -> FocuserCapabilities;

    // ==================== Position Control ====================

    /**
     * @brief Move to absolute position
     * @param position Target position
     * @return true if move started
     */
    auto moveTo(int position) -> bool;

    /**
     * @brief Move relative steps
     * @param steps Steps to move (positive or negative)
     * @return true if move started
     */
    auto moveRelative(int steps) -> bool;

    /**
     * @brief Halt movement
     * @return true if halted
     */
    auto halt() -> bool;

    /**
     * @brief Check if focuser is moving
     */
    [[nodiscard]] auto isMoving() -> bool;

    /**
     * @brief Wait for movement to complete
     * @param timeout Maximum wait time
     * @return true if movement completed
     */
    auto waitForMove(std::chrono::milliseconds timeout = std::chrono::seconds(60))
        -> bool;

    /**
     * @brief Get current position
     */
    [[nodiscard]] auto getPosition() -> int;

    /**
     * @brief Get maximum position
     */
    [[nodiscard]] auto getMaxStep() -> int;

    /**
     * @brief Get maximum increment
     */
    [[nodiscard]] auto getMaxIncrement() -> int;

    /**
     * @brief Get step size in microns
     */
    [[nodiscard]] auto getStepSize() -> double;

    // ==================== Temperature ====================

    /**
     * @brief Get focuser temperature
     */
    [[nodiscard]] auto getTemperature() -> std::optional<double>;

    /**
     * @brief Enable/disable temperature compensation
     * @param enable True to enable
     * @return true if set successfully
     */
    auto setTempComp(bool enable) -> bool;

    /**
     * @brief Check if temperature compensation is enabled
     */
    [[nodiscard]] auto isTempCompEnabled() -> bool;

    // ==================== Status ====================

    /**
     * @brief Get focuser state
     */
    [[nodiscard]] auto getFocuserState() const -> FocuserState {
        return focuserState_.load();
    }

    /**
     * @brief Get focuser status as JSON
     */
    [[nodiscard]] auto getStatus() const -> json override;

private:
    void refreshCapabilities();

    std::atomic<FocuserState> focuserState_{FocuserState::Idle};
    FocuserCapabilities capabilities_;
    FocuserPosition positionInfo_;

    mutable std::mutex moveMutex_;
    std::condition_variable moveCV_;
};

}  // namespace lithium::client::ascom

#endif  // LITHIUM_CLIENT_ASCOM_FOCUSER_HPP
