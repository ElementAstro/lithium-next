/*
 * client.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-28

Description: PHD2 JSON-RPC client with modern C++ features

*************************************************/

#pragma once

#include "connection.hpp"
#include "event_handler.hpp"
#include "types.hpp"

#include <array>
#include <chrono>
#include <expected>
#include <future>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace phd2 {

/**
 * @brief Main client class for interacting with PHD2
 *
 * Provides a high-level interface for all PHD2 operations including:
 * - Camera control
 * - Equipment management
 * - Guiding control
 * - Calibration
 * - Star selection
 * - Algorithm settings
 */
class Client {
public:
    /**
     * @brief Construct a new Client
     * @param host The hostname where PHD2 is running
     * @param port The port number (default: 4400)
     * @param eventHandler Optional event handler
     */
    explicit Client(std::string host = "localhost", int port = 4400,
                    std::shared_ptr<EventHandler> eventHandler = nullptr);

    /**
     * @brief Construct with connection config
     */
    explicit Client(ConnectionConfig config,
                    std::shared_ptr<EventHandler> eventHandler = nullptr);

    ~Client();

    // Non-copyable, movable
    Client(const Client&) = delete;
    Client& operator=(const Client&) = delete;
    Client(Client&&) noexcept;
    Client& operator=(Client&&) noexcept;

    // ==================== Connection ====================

    /**
     * @brief Connect to PHD2
     * @param timeoutMs Connection timeout in milliseconds
     * @return true if connection was successful
     */
    [[nodiscard]] auto connect(int timeoutMs = 5000) -> bool;

    /**
     * @brief Connect with chrono duration
     */
    template <typename Rep, typename Period>
    [[nodiscard]] auto connect(std::chrono::duration<Rep, Period> timeout)
        -> bool {
        return connect(static_cast<int>(
            std::chrono::duration_cast<std::chrono::milliseconds>(timeout)
                .count()));
    }

    /**
     * @brief Disconnect from PHD2
     */
    void disconnect();

    /**
     * @brief Check if connected
     */
    [[nodiscard]] auto isConnected() const noexcept -> bool;

    /**
     * @brief Set event handler
     */
    void setEventHandler(std::shared_ptr<EventHandler> handler);

    // ==================== Camera Control ====================

    /**
     * @brief Get current exposure time
     * @return Exposure time in milliseconds
     */
    [[nodiscard]] auto getExposure() -> int;

    /**
     * @brief Set exposure time
     * @param exposureMs Exposure in milliseconds
     */
    void setExposure(int exposureMs);

    /**
     * @brief Get available exposure durations
     * @return Vector of available exposures in ms
     */
    [[nodiscard]] auto getExposureDurations() -> std::vector<int>;

    /**
     * @brief Get subframe status
     */
    [[nodiscard]] auto getUseSubframes() -> bool;

    /**
     * @brief Capture a single frame
     * @param exposureMs Optional exposure time
     * @param subframe Optional subframe region [x, y, width, height]
     */
    void captureSingleFrame(
        std::optional<int> exposureMs = std::nullopt,
        std::optional<std::array<int, 4>> subframe = std::nullopt);

    /**
     * @brief Get camera frame size
     * @return [width, height] in pixels
     */
    [[nodiscard]] auto getCameraFrameSize() -> std::array<int, 2>;

    /**
     * @brief Get CCD temperature
     * @return Temperature in degrees C
     */
    [[nodiscard]] auto getCcdTemperature() -> double;

    /**
     * @brief Get cooler status
     * @return JSON with cooler info
     */
    [[nodiscard]] auto getCoolerStatus() -> json;

    /**
     * @brief Save current image
     * @return Path to saved FITS file
     */
    [[nodiscard]] auto saveImage() -> std::string;

    /**
     * @brief Get star image
     * @param size Image size (>= 15)
     * @return JSON with image data
     */
    [[nodiscard]] auto getStarImage(int size = 15) -> json;

    // ==================== Equipment Control ====================

    /**
     * @brief Check if all equipment is connected
     */
    [[nodiscard]] auto getConnected() -> bool;

    /**
     * @brief Connect or disconnect equipment
     * @param connect True to connect
     */
    void setConnected(bool connect);

    /**
     * @brief Get current equipment info
     */
    [[nodiscard]] auto getCurrentEquipment() -> json;

    /**
     * @brief Get available profiles
     */
    [[nodiscard]] auto getProfiles() -> json;

    /**
     * @brief Get current profile
     */
    [[nodiscard]] auto getProfile() -> json;

    /**
     * @brief Set profile
     * @param profileId Profile ID
     */
    void setProfile(int profileId);

    // ==================== Guiding Control ====================

    /**
     * @brief Start guiding
     * @param settle Settlement parameters
     * @param recalibrate Force recalibration
     * @param roi Optional region of interest
     * @return Future that completes when settled
     */
    [[nodiscard]] auto startGuiding(
        const SettleParams& settle, bool recalibrate = false,
        std::optional<std::array<int, 4>> roi = std::nullopt)
        -> std::future<bool>;

    /**
     * @brief Stop guiding and capturing
     */
    void stopCapture();

    /**
     * @brief Start looping exposures
     */
    void loop();

    /**
     * @brief Perform dither
     * @param amount Dither amount in pixels
     * @param raOnly Only dither in RA
     * @param settle Settlement parameters
     * @return Future that completes when settled
     */
    [[nodiscard]] auto dither(double amount, bool raOnly,
                              const SettleParams& settle) -> std::future<bool>;

    /**
     * @brief Get current app state
     */
    [[nodiscard]] auto getAppState() -> AppStateType;

    /**
     * @brief Send guide pulse
     * @param amount Duration in ms or AO step count
     * @param direction Direction ("N"/"S"/"E"/"W")
     * @param which "Mount" or "AO"
     */
    void guidePulse(int amount, std::string_view direction,
                    std::string_view which = "Mount");

    /**
     * @brief Check if paused
     */
    [[nodiscard]] auto getPaused() -> bool;

    /**
     * @brief Pause or unpause
     * @param pause True to pause
     * @param full Also pause looping
     */
    void setPaused(bool pause, bool full = false);

    /**
     * @brief Check if guide output enabled
     */
    [[nodiscard]] auto getGuideOutputEnabled() -> bool;

    /**
     * @brief Enable/disable guide output
     */
    void setGuideOutputEnabled(bool enable);

    /**
     * @brief Get variable delay settings
     */
    [[nodiscard]] auto getVariableDelaySettings() -> json;

    /**
     * @brief Set variable delay settings
     */
    void setVariableDelaySettings(const json& settings);

    // ==================== Calibration ====================

    /**
     * @brief Check if calibrated
     */
    [[nodiscard]] auto isCalibrated() -> bool;

    /**
     * @brief Clear calibration
     * @param which "mount", "ao", or "both"
     */
    void clearCalibration(std::string_view which = "both");

    /**
     * @brief Flip calibration for meridian flip
     */
    void flipCalibration();

    /**
     * @brief Get calibration data
     * @param which "Mount" or "AO"
     */
    [[nodiscard]] auto getCalibrationData(std::string_view which = "Mount")
        -> json;

    // ==================== Algorithm Settings ====================

    /**
     * @brief Set Dec guide mode
     * @param mode "Off", "Auto", "North", "South"
     */
    void setDecGuideMode(std::string_view mode);

    /**
     * @brief Get Dec guide mode
     */
    [[nodiscard]] auto getDecGuideMode() -> std::string;

    /**
     * @brief Set algorithm parameter
     * @param axis "ra", "dec", "x", "y"
     * @param name Parameter name
     * @param value Parameter value
     */
    void setAlgoParam(std::string_view axis, std::string_view name,
                      double value);

    /**
     * @brief Get algorithm parameter
     */
    [[nodiscard]] auto getAlgoParam(std::string_view axis,
                                    std::string_view name) -> double;

    /**
     * @brief Get available parameter names
     */
    [[nodiscard]] auto getAlgoParamNames(std::string_view axis)
        -> std::vector<std::string>;

    // ==================== Star Selection ====================

    /**
     * @brief Find star automatically
     * @param roi Optional region of interest
     * @return Star position [x, y]
     */
    [[nodiscard]] auto findStar(std::optional<std::array<int, 4>> roi =
                                    std::nullopt) -> std::array<double, 2>;

    /**
     * @brief Get lock position
     * @return Position or nullopt if not set
     */
    [[nodiscard]] auto getLockPosition()
        -> std::optional<std::array<double, 2>>;

    /**
     * @brief Set lock position
     * @param x X coordinate
     * @param y Y coordinate
     * @param exact Use exact position or find nearest star
     */
    void setLockPosition(double x, double y, bool exact = true);

    /**
     * @brief Get search region size
     */
    [[nodiscard]] auto getSearchRegion() -> int;

    /**
     * @brief Get pixel scale
     * @return Arcsec per pixel
     */
    [[nodiscard]] auto getPixelScale() -> double;

    // ==================== Lock Shift ====================

    /**
     * @brief Check if lock shift enabled
     */
    [[nodiscard]] auto getLockShiftEnabled() -> bool;

    /**
     * @brief Enable/disable lock shift
     */
    void setLockShiftEnabled(bool enable);

    /**
     * @brief Get lock shift parameters
     */
    [[nodiscard]] auto getLockShiftParams() -> json;

    /**
     * @brief Set lock shift parameters
     */
    void setLockShiftParams(const json& params);

    // ==================== Advanced ====================

    /**
     * @brief Shutdown PHD2
     */
    void shutdown();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace phd2
