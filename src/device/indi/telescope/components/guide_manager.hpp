/*
 * guide_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-23

Description: INDI Telescope Guide Manager Component

This component manages telescope guiding operations including
guide pulses, guiding calibration, and autoguiding support.

*************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <queue>

#include "device/template/telescope.hpp"

namespace lithium::device::indi::telescope::components {

class HardwareInterface;

/**
 * @brief Guide Manager for INDI Telescope
 *
 * Manages all telescope guiding operations including guide pulses,
 * guiding calibration, pulse queuing, and autoguiding coordination.
 */
class GuideManager {
public:
    enum class GuideDirection {
        NORTH,
        SOUTH,
        EAST,
        WEST
    };

    struct GuidePulse {
        GuideDirection direction;
        std::chrono::milliseconds duration;
        std::chrono::steady_clock::time_point timestamp;
        bool completed = false;
        std::string id;
    };

    struct GuideCalibration {
        double northRate = 0.0;     // arcsec/ms
        double southRate = 0.0;     // arcsec/ms
        double eastRate = 0.0;      // arcsec/ms
        double westRate = 0.0;      // arcsec/ms
        double northAngle = 0.0;    // degrees
        double southAngle = 0.0;    // degrees
        double eastAngle = 0.0;     // degrees
        double westAngle = 0.0;     // degrees
        bool isValid = false;
        std::chrono::system_clock::time_point calibrationTime;
        std::string calibrationMethod;
    };

    struct GuideStatistics {
        uint64_t totalPulses = 0;
        uint64_t northPulses = 0;
        uint64_t southPulses = 0;
        uint64_t eastPulses = 0;
        uint64_t westPulses = 0;
        std::chrono::milliseconds totalPulseTime{0};
        std::chrono::milliseconds avgPulseDuration{0};
        std::chrono::milliseconds maxPulseDuration{0};
        std::chrono::milliseconds minPulseDuration{0};
        double guideRMS = 0.0;      // arcsec
        std::chrono::steady_clock::time_point sessionStartTime;
    };

    using GuidePulseCompleteCallback = std::function<void(const GuidePulse& pulse, bool success)>;
    using GuideCalibrationCallback = std::function<void(const GuideCalibration& calibration)>;

public:
    explicit GuideManager(std::shared_ptr<HardwareInterface> hardware);
    ~GuideManager();

    // Non-copyable and non-movable
    GuideManager(const GuideManager&) = delete;
    GuideManager& operator=(const GuideManager&) = delete;
    GuideManager(GuideManager&&) = delete;
    GuideManager& operator=(GuideManager&&) = delete;

    // Initialization
    bool initialize();
    bool shutdown();
    bool isInitialized() const { return initialized_; }

    // Basic Guiding Operations
    bool guidePulse(GuideDirection direction, std::chrono::milliseconds duration);
    bool guidePulse(double raPulseMs, double decPulseMs); // Positive = East/North
    bool guideNorth(std::chrono::milliseconds duration);
    bool guideSouth(std::chrono::milliseconds duration);
    bool guideEast(std::chrono::milliseconds duration);
    bool guideWest(std::chrono::milliseconds duration);

    // Pulse Queue Management
    bool queueGuidePulse(GuideDirection direction, std::chrono::milliseconds duration);
    bool clearGuideQueue();
    size_t getQueueSize() const;
    bool isGuiding() const;
    std::optional<GuidePulse> getCurrentPulse() const;

    // Guide Rates
    bool setGuideRate(double rateArcsecPerSec);
    std::optional<double> getGuideRate() const; // arcsec/sec
    bool setGuideRates(double raRate, double decRate); // arcsec/sec
    std::optional<MotionRates> getGuideRates() const;

    // Calibration
    bool startCalibration();
    bool abortCalibration();
    bool isCalibrating() const;
    GuideCalibration getCalibration() const;
    bool setCalibration(const GuideCalibration& calibration);
    bool isCalibrated() const;
    bool clearCalibration();

    // Advanced Calibration
    bool calibrateDirection(GuideDirection direction, std::chrono::milliseconds pulseDuration,
                           int pulseCount = 5);
    bool autoCalibrate(std::chrono::milliseconds basePulseDuration = std::chrono::milliseconds(1000));
    double calculateCalibrationAccuracy() const;

    // Pulse Conversion
    std::chrono::milliseconds arcsecToPulseDuration(double arcsec, GuideDirection direction) const;
    double pulseDurationToArcsec(std::chrono::milliseconds duration, GuideDirection direction) const;

    // Statistics and Monitoring
    GuideStatistics getGuideStatistics() const;
    bool resetGuideStatistics();
    double getCurrentGuideRMS() const;
    std::vector<GuidePulse> getRecentPulses(std::chrono::seconds timeWindow) const;

    // Pulse Limits and Safety
    bool setMaxPulseDuration(std::chrono::milliseconds maxDuration);
    std::chrono::milliseconds getMaxPulseDuration() const;
    bool setMinPulseDuration(std::chrono::milliseconds minDuration);
    std::chrono::milliseconds getMinPulseDuration() const;
    bool enablePulseLimits(bool enable);

    // Dithering Support
    bool dither(double amountArcsec, double angleRadians);
    bool ditherRandom(double maxAmountArcsec);
    bool ditherSpiral(double radiusArcsec, int steps);

    // Callback Registration
    void setGuidePulseCompleteCallback(GuidePulseCompleteCallback callback) { pulseCompleteCallback_ = std::move(callback); }
    void setGuideCalibrationCallback(GuideCalibrationCallback callback) { calibrationCallback_ = std::move(callback); }

    // Advanced Features
    bool enableGuideLogging(bool enable, const std::string& logFile = "");
    bool saveCalibration(const std::string& filename) const;
    bool loadCalibration(const std::string& filename);
    bool setGuidingProfile(const std::string& profileName);

private:
    // Hardware interface
    std::shared_ptr<HardwareInterface> hardware_;

    // State management
    std::atomic<bool> initialized_{false};
    std::atomic<bool> isGuiding_{false};
    std::atomic<bool> isCalibrating_{false};
    mutable std::recursive_mutex guideMutex_;

    // Guide queue and current pulse
    std::queue<GuidePulse> guideQueue_;
    std::optional<GuidePulse> currentPulse_;
    std::string nextPulseId_;

    // Calibration data
    GuideCalibration calibration_;
    std::atomic<bool> calibrated_{false};

    // Guide rates and limits
    MotionRates guideRates_;
    std::chrono::milliseconds maxPulseDuration_{10000}; // 10 seconds
    std::chrono::milliseconds minPulseDuration_{10};    // 10 ms
    std::atomic<bool> pulseLimitsEnabled_{true};

    // Statistics
    GuideStatistics statistics_;
    std::vector<GuidePulse> recentPulses_;
    std::atomic<double> currentGuideRMS_{0.0};

    // Callbacks
    GuidePulseCompleteCallback pulseCompleteCallback_;
    GuideCalibrationCallback calibrationCallback_;

    // Internal methods
    void processGuideQueue();
    void executePulse(const GuidePulse& pulse);
    void updateGuideStatistics(const GuidePulse& pulse);
    void handlePropertyUpdate(const std::string& propertyName);

    // Pulse management
    std::string generatePulseId();
    bool validatePulseDuration(std::chrono::milliseconds duration) const;
    GuideDirection convertMotionToDirection(int nsDirection, int ewDirection) const;

    // Calibration helpers
    void performCalibrationSequence();
    bool calibrateDirectionSequence(GuideDirection direction,
                                   std::chrono::milliseconds pulseDuration,
                                   int pulseCount);
    void calculateCalibrationRates();

    // Rate calculations
    double calculateEffectiveGuideRate(GuideDirection direction) const;
    std::chrono::milliseconds calculatePulseDuration(double arcsec, double rateArcsecPerMs) const;

    // Validation methods
    bool isValidGuideDirection(GuideDirection direction) const;
    bool isValidPulseParameters(GuideDirection direction, std::chrono::milliseconds duration) const;

    // Hardware interaction
    bool sendGuidePulseToHardware(GuideDirection direction, std::chrono::milliseconds duration);
    void syncGuideRatesToHardware();

    // Utility methods
    std::string directionToString(GuideDirection direction) const;
    GuideDirection stringToDirection(const std::string& directionStr) const;
    void logInfo(const std::string& message);
    void logWarning(const std::string& message);
    void logError(const std::string& message);

    // Constants
    static constexpr double DEFAULT_GUIDE_RATE = 0.5;  // arcsec/sec
    static constexpr size_t MAX_RECENT_PULSES = 100;
    static constexpr int DEFAULT_CALIBRATION_PULSES = 5;
};

} // namespace lithium::device::indi::telescope::components
