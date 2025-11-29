/*
 * guider_client.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-27

Description: Base class for guider clients (PHD2, etc.)

*************************************************/

#ifndef LITHIUM_CLIENT_GUIDER_CLIENT_HPP
#define LITHIUM_CLIENT_GUIDER_CLIENT_HPP

#include "client_base.hpp"

#include <array>
#include <future>
#include <optional>

namespace lithium::client {

/**
 * @brief Guider state enumeration
 */
enum class GuiderState {
    Stopped,
    Looping,
    Calibrating,
    Guiding,
    Settling,
    Paused,
    LostStar
};

/**
 * @brief Guide star information
 */
struct GuideStar {
    double x{0.0};
    double y{0.0};
    double snr{0.0};
    double mass{0.0};
    bool valid{false};
};

/**
 * @brief Guide statistics
 */
struct GuideStats {
    double rmsRA{0.0};     // arcsec
    double rmsDec{0.0};    // arcsec
    double rmsTotal{0.0};  // arcsec
    double peakRA{0.0};    // arcsec
    double peakDec{0.0};   // arcsec
    int sampleCount{0};
    double snr{0.0};
};

/**
 * @brief Settle parameters for guiding operations
 */
struct SettleParams {
    double pixels{1.5};    // Max error in pixels
    double time{10.0};     // Settle time in seconds
    double timeout{60.0};  // Timeout in seconds
};

/**
 * @brief Dither parameters
 */
struct DitherParams {
    double amount{5.0};  // Dither amount in pixels
    bool raOnly{false};  // Only dither in RA
    SettleParams settle;
};

/**
 * @brief Calibration data
 */
struct CalibrationData {
    bool calibrated{false};
    double raRate{0.0};    // arcsec/sec
    double decRate{0.0};   // arcsec/sec
    double raAngle{0.0};   // degrees
    double decAngle{0.0};  // degrees
    bool decFlipped{false};
    std::string timestamp;
};

/**
 * @brief Base class for guider clients
 *
 * Provides common interface for PHD2 and other guiding software
 */
class GuiderClient : public ClientBase {
public:
    /**
     * @brief Construct a new GuiderClient
     * @param name Guider name
     */
    explicit GuiderClient(std::string name);

    /**
     * @brief Virtual destructor
     */
    ~GuiderClient() override;

    // ==================== Guiding Control ====================

    /**
     * @brief Start guiding
     * @param settle Settle parameters
     * @param recalibrate Force recalibration
     * @return Future that completes when settled
     */
    virtual std::future<bool> startGuiding(const SettleParams& settle,
                                           bool recalibrate = false) = 0;

    /**
     * @brief Stop guiding
     */
    virtual void stopGuiding() = 0;

    /**
     * @brief Pause guiding
     * @param full If true, also pause looping
     */
    virtual void pause(bool full = false) = 0;

    /**
     * @brief Resume guiding
     */
    virtual void resume() = 0;

    /**
     * @brief Perform dither operation
     * @param params Dither parameters
     * @return Future that completes when settled
     */
    virtual std::future<bool> dither(const DitherParams& params) = 0;

    /**
     * @brief Start looping exposures
     */
    virtual void loop() = 0;

    // ==================== Calibration ====================

    /**
     * @brief Check if calibrated
     * @return true if calibrated
     */
    virtual bool isCalibrated() const = 0;

    /**
     * @brief Clear calibration
     */
    virtual void clearCalibration() = 0;

    /**
     * @brief Flip calibration (for meridian flip)
     */
    virtual void flipCalibration() = 0;

    /**
     * @brief Get calibration data
     * @return Calibration data
     */
    virtual CalibrationData getCalibrationData() const = 0;

    // ==================== Star Selection ====================

    /**
     * @brief Auto-select guide star
     * @param roi Optional region of interest [x, y, width, height]
     * @return Selected star position
     */
    virtual GuideStar findStar(
        const std::optional<std::array<int, 4>>& roi = std::nullopt) = 0;

    /**
     * @brief Set lock position manually
     * @param x X coordinate
     * @param y Y coordinate
     * @param exact Use exact position or find nearest star
     */
    virtual void setLockPosition(double x, double y, bool exact = true) = 0;

    /**
     * @brief Get current lock position
     * @return Lock position or nullopt if not set
     */
    virtual std::optional<std::array<double, 2>> getLockPosition() const = 0;

    // ==================== Camera Control ====================

    /**
     * @brief Get exposure time
     * @return Exposure in milliseconds
     */
    virtual int getExposure() const = 0;

    /**
     * @brief Set exposure time
     * @param exposureMs Exposure in milliseconds
     */
    virtual void setExposure(int exposureMs) = 0;

    /**
     * @brief Get available exposure durations
     * @return Vector of available exposures in ms
     */
    virtual std::vector<int> getExposureDurations() const = 0;

    // ==================== Status ====================

    /**
     * @brief Get guider state
     * @return Current guider state
     */
    virtual GuiderState getGuiderState() const = 0;

    /**
     * @brief Get guider state as string
     * @return State name
     */
    std::string getGuiderStateName() const;

    /**
     * @brief Check if guiding
     * @return true if actively guiding
     */
    bool isGuiding() const { return getGuiderState() == GuiderState::Guiding; }

    /**
     * @brief Check if paused
     * @return true if paused
     */
    bool isPaused() const { return getGuiderState() == GuiderState::Paused; }

    /**
     * @brief Get guide statistics
     * @return Current guide stats
     */
    virtual GuideStats getGuideStats() const = 0;

    /**
     * @brief Get current guide star info
     * @return Guide star information
     */
    virtual GuideStar getCurrentStar() const = 0;

    /**
     * @brief Get pixel scale
     * @return Pixel scale in arcsec/pixel
     */
    virtual double getPixelScale() const = 0;

protected:
    std::atomic<GuiderState> guiderState_{GuiderState::Stopped};
};

}  // namespace lithium::client

#endif  // LITHIUM_CLIENT_GUIDER_CLIENT_HPP
