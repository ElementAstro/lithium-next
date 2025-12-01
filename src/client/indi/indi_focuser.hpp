/*
 * indi_focuser.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: INDI Focuser Device Implementation

**************************************************/

#ifndef LITHIUM_CLIENT_INDI_FOCUSER_HPP
#define LITHIUM_CLIENT_INDI_FOCUSER_HPP

#include "indi_device_base.hpp"

namespace lithium::client::indi {

/**
 * @brief Focus direction enumeration
 */
enum class FocusDirection : uint8_t { In, Out, None };

/**
 * @brief Focus mode enumeration
 */
enum class FocusMode : uint8_t { All, Absolute, Relative, Timer, None };

/**
 * @brief Baud rate enumeration
 */
enum class BaudRate : uint8_t {
    B9600,
    B19200,
    B38400,
    B57600,
    B115200,
    B230400,
    None
};

/**
 * @brief Focuser state enumeration
 */
enum class FocuserState : uint8_t { Idle, Moving, Error, Unknown };

/**
 * @brief Focuser position information
 */
struct FocuserPosition {
    int absolute{0};
    int relative{0};
    int maxPosition{100000};
    int minPosition{0};

    [[nodiscard]] auto toJson() const -> json {
        return {{"absolute", absolute},
                {"relative", relative},
                {"maxPosition", maxPosition},
                {"minPosition", minPosition}};
    }
};

/**
 * @brief Focuser speed information
 */
struct FocuserSpeed {
    double current{0.0};
    double min{0.0};
    double max{100.0};

    [[nodiscard]] auto toJson() const -> json {
        return {{"current", current}, {"min", min}, {"max", max}};
    }
};

/**
 * @brief Focuser temperature information
 */
struct FocuserTemperature {
    double external{0.0};
    double chip{0.0};
    bool hasExternal{false};
    bool hasChip{false};

    [[nodiscard]] auto toJson() const -> json {
        return {{"external", external},
                {"chip", chip},
                {"hasExternal", hasExternal},
                {"hasChip", hasChip}};
    }
};

/**
 * @brief Focuser backlash information
 */
struct BacklashInfo {
    bool enabled{false};
    int steps{0};

    [[nodiscard]] auto toJson() const -> json {
        return {{"enabled", enabled}, {"steps", steps}};
    }
};

/**
 * @brief INDI Focuser Device
 *
 * Provides focuser-specific functionality including:
 * - Position control (absolute and relative)
 * - Speed control
 * - Temperature monitoring
 * - Backlash compensation
 * - Direction control
 */
class INDIFocuser : public INDIDeviceBase {
public:
    static constexpr int DEFAULT_TIMEOUT_MS = 5000;

    /**
     * @brief Construct a new INDI Focuser
     * @param name Device name
     */
    explicit INDIFocuser(std::string name);

    /**
     * @brief Destructor
     */
    ~INDIFocuser() override;

    // ==================== Device Type ====================

    [[nodiscard]] auto getDeviceType() const -> std::string override {
        return "Focuser";
    }

    // ==================== Connection ====================

    auto connect(const std::string& deviceName, int timeout = DEFAULT_TIMEOUT_MS,
                 int maxRetry = 3) -> bool override;

    auto disconnect() -> bool override;

    // ==================== Position Control ====================

    /**
     * @brief Move to absolute position
     * @param position Target position
     * @return true if move started
     */
    auto moveToPosition(int position) -> bool;

    /**
     * @brief Move relative steps
     * @param steps Number of steps (positive = out, negative = in)
     * @return true if move started
     */
    auto moveSteps(int steps) -> bool;

    /**
     * @brief Move for specified duration
     * @param durationMs Duration in milliseconds
     * @return true if move started
     */
    auto moveForDuration(int durationMs) -> bool;

    /**
     * @brief Abort current move
     * @return true if aborted
     */
    auto abortMove() -> bool;

    /**
     * @brief Sync position to specified value
     * @param position Position to sync to
     * @return true if synced
     */
    auto syncPosition(int position) -> bool;

    /**
     * @brief Get current position
     * @return Position, or nullopt if not available
     */
    [[nodiscard]] auto getPosition() const -> std::optional<int>;

    /**
     * @brief Get position information
     * @return Position info
     */
    [[nodiscard]] auto getPositionInfo() const -> FocuserPosition;

    /**
     * @brief Check if focuser is moving
     * @return true if moving
     */
    [[nodiscard]] auto isMoving() const -> bool;

    /**
     * @brief Wait for move to complete
     * @param timeout Timeout in milliseconds
     * @return true if move completed
     */
    auto waitForMove(std::chrono::milliseconds timeout =
                         std::chrono::seconds(60)) -> bool;

    // ==================== Speed Control ====================

    /**
     * @brief Set movement speed
     * @param speed Speed value
     * @return true if set successfully
     */
    auto setSpeed(double speed) -> bool;

    /**
     * @brief Get current speed
     * @return Speed, or nullopt if not available
     */
    [[nodiscard]] auto getSpeed() const -> std::optional<double>;

    /**
     * @brief Get speed information
     * @return Speed info
     */
    [[nodiscard]] auto getSpeedInfo() const -> FocuserSpeed;

    // ==================== Direction Control ====================

    /**
     * @brief Set movement direction
     * @param direction Direction
     * @return true if set successfully
     */
    auto setDirection(FocusDirection direction) -> bool;

    /**
     * @brief Get current direction
     * @return Direction
     */
    [[nodiscard]] auto getDirection() const -> FocusDirection;

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

    // ==================== Limits ====================

    /**
     * @brief Set maximum position limit
     * @param maxLimit Maximum position
     * @return true if set successfully
     */
    auto setMaxLimit(int maxLimit) -> bool;

    /**
     * @brief Get maximum position limit
     * @return Max limit, or nullopt if not available
     */
    [[nodiscard]] auto getMaxLimit() const -> std::optional<int>;

    // ==================== Temperature ====================

    /**
     * @brief Get external temperature
     * @return Temperature in Celsius, or nullopt if not available
     */
    [[nodiscard]] auto getExternalTemperature() const -> std::optional<double>;

    /**
     * @brief Get chip temperature
     * @return Temperature in Celsius, or nullopt if not available
     */
    [[nodiscard]] auto getChipTemperature() const -> std::optional<double>;

    /**
     * @brief Get temperature information
     * @return Temperature info
     */
    [[nodiscard]] auto getTemperatureInfo() const -> FocuserTemperature;

    // ==================== Backlash ====================

    /**
     * @brief Enable/disable backlash compensation
     * @param enabled true to enable
     * @return true if set successfully
     */
    auto setBacklashEnabled(bool enabled) -> bool;

    /**
     * @brief Set backlash steps
     * @param steps Backlash steps
     * @return true if set successfully
     */
    auto setBacklashSteps(int steps) -> bool;

    /**
     * @brief Get backlash information
     * @return Backlash info
     */
    [[nodiscard]] auto getBacklashInfo() const -> BacklashInfo;

    // ==================== Mode ====================

    /**
     * @brief Get supported focus modes
     * @return Focus mode
     */
    [[nodiscard]] auto getFocusMode() const -> FocusMode;

    // ==================== Status ====================

    /**
     * @brief Get focuser state
     * @return Focuser state
     */
    [[nodiscard]] auto getFocuserState() const -> FocuserState;

    /**
     * @brief Get focuser status as JSON
     * @return Status JSON
     */
    [[nodiscard]] auto getStatus() const -> json override;

protected:
    // ==================== Property Handlers ====================

    void onPropertyDefined(const INDIProperty& property) override;
    void onPropertyUpdated(const INDIProperty& property) override;

private:
    // ==================== Internal Methods ====================

    void handlePositionProperty(const INDIProperty& property);
    void handleSpeedProperty(const INDIProperty& property);
    void handleDirectionProperty(const INDIProperty& property);
    void handleTemperatureProperty(const INDIProperty& property);
    void handleBacklashProperty(const INDIProperty& property);
    void handleMaxLimitProperty(const INDIProperty& property);
    void handleAbortProperty(const INDIProperty& property);

    void setupPropertyWatchers();

    // ==================== Member Variables ====================

    // Focuser state
    std::atomic<FocuserState> focuserState_{FocuserState::Idle};
    std::atomic<bool> isMoving_{false};

    // Position
    FocuserPosition positionInfo_;
    mutable std::mutex positionMutex_;
    std::condition_variable moveCondition_;

    // Speed
    FocuserSpeed speedInfo_;
    mutable std::mutex speedMutex_;

    // Direction
    std::atomic<FocusDirection> direction_{FocusDirection::None};
    std::atomic<bool> isReversed_{false};

    // Temperature
    FocuserTemperature temperatureInfo_;
    mutable std::mutex temperatureMutex_;

    // Backlash
    BacklashInfo backlashInfo_;
    mutable std::mutex backlashMutex_;

    // Mode
    std::atomic<FocusMode> focusMode_{FocusMode::All};

    // Connection settings
    std::atomic<BaudRate> baudRate_{BaudRate::B9600};
    std::atomic<bool> deviceAutoSearch_{false};
    std::atomic<bool> devicePortScan_{false};
};

}  // namespace lithium::client::indi

#endif  // LITHIUM_CLIENT_INDI_FOCUSER_HPP
