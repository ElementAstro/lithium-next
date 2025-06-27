/*
 * tracking_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-23

Description: INDI Telescope Tracking Manager Component

This component manages telescope tracking operations including
track modes, track rates, tracking state control, and tracking accuracy.

*************************************************/

#pragma once

#include <atomic>
#include <chrono>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>

#include "device/template/telescope.hpp"

namespace lithium::device::indi::telescope::components {

class HardwareInterface;

/**
 * @brief Tracking Manager for INDI Telescope
 * 
 * Manages all telescope tracking operations including track modes,
 * custom track rates, tracking state control, and tracking performance monitoring.
 */
class TrackingManager {
public:
    struct TrackingStatus {
        bool isEnabled = false;
        TrackMode mode = TrackMode::SIDEREAL;
        double trackRateRA = 0.0;   // arcsec/sec
        double trackRateDEC = 0.0;  // arcsec/sec
        double trackingError = 0.0; // arcsec RMS
        std::chrono::steady_clock::time_point lastUpdate;
        std::string statusMessage;
    };

    struct TrackingStatistics {
        std::chrono::steady_clock::time_point trackingStartTime;
        std::chrono::seconds totalTrackingTime{0};
        double maxTrackingError = 0.0;  // arcsec
        double avgTrackingError = 0.0;  // arcsec
        uint64_t trackingCorrectionCount = 0;
        double periodicErrorAmplitude = 0.0;  // arcsec
        double periodicErrorPeriod = 0.0;     // minutes
    };

    using TrackingStateCallback = std::function<void(bool enabled, TrackMode mode)>;
    using TrackingErrorCallback = std::function<void(double errorArcsec)>;

public:
    explicit TrackingManager(std::shared_ptr<HardwareInterface> hardware);
    ~TrackingManager();

    // Non-copyable and non-movable
    TrackingManager(const TrackingManager&) = delete;
    TrackingManager& operator=(const TrackingManager&) = delete;
    TrackingManager(TrackingManager&&) = delete;
    TrackingManager& operator=(TrackingManager&&) = delete;

    // Initialization
    bool initialize();
    bool shutdown();
    bool isInitialized() const { return initialized_; }

    // Tracking Control
    bool enableTracking(bool enable);
    bool isTrackingEnabled() const;
    bool setTrackingMode(TrackMode mode);
    TrackMode getTrackingMode() const { return currentMode_; }

    // Track Rates
    bool setTrackRates(double raRate, double decRate); // arcsec/sec
    bool setTrackRates(const MotionRates& rates);
    std::optional<MotionRates> getTrackRates() const;
    std::optional<MotionRates> getDefaultTrackRates(TrackMode mode) const;

    // Predefined Tracking Modes
    bool setSiderealTracking();
    bool setSolarTracking();
    bool setLunarTracking();
    bool setCustomTracking(double raRate, double decRate);

    // Tracking Status and Monitoring
    TrackingStatus getTrackingStatus() const;
    TrackingStatistics getTrackingStatistics() const;
    double getCurrentTrackingError() const;
    bool isTrackingAccurate(double toleranceArcsec = 5.0) const;

    // Tracking Corrections
    bool applyTrackingCorrection(double raCorrection, double decCorrection); // arcsec
    bool enableAutoGuiding(bool enable);
    bool isAutoGuidingEnabled() const;

    // Periodic Error Correction (PEC)
    bool enablePEC(bool enable);
    bool isPECEnabled() const;
    bool calibratePEC();
    bool isPECCalibrated() const;

    // Tracking Quality Assessment
    double calculateTrackingQuality() const; // 0.0 to 1.0
    std::string getTrackingQualityDescription() const;
    bool needsTrackingImprovement() const;

    // Callback Registration
    void setTrackingStateCallback(TrackingStateCallback callback) { trackingStateCallback_ = std::move(callback); }
    void setTrackingErrorCallback(TrackingErrorCallback callback) { trackingErrorCallback_ = std::move(callback); }

    // Advanced Features
    bool setTrackingLimits(double maxRARate, double maxDECRate); // arcsec/sec
    bool resetTrackingStatistics();
    bool saveTrackingProfile(const std::string& profileName);
    bool loadTrackingProfile(const std::string& profileName);

private:
    // Hardware interface
    std::shared_ptr<HardwareInterface> hardware_;

    // State management
    std::atomic<bool> initialized_{false};
    std::atomic<bool> trackingEnabled_{false};
    std::atomic<TrackMode> currentMode_{TrackMode::SIDEREAL};
    mutable std::recursive_mutex stateMutex_;

    // Track rates
    MotionRates currentRates_;
    std::atomic<double> trackRateRA_{0.0};
    std::atomic<double> trackRateDEC_{0.0};

    // Tracking monitoring
    TrackingStatus currentStatus_;
    TrackingStatistics statistics_;
    std::atomic<double> currentTrackingError_{0.0};

    // Auto-guiding and PEC
    std::atomic<bool> autoGuidingEnabled_{false};
    std::atomic<bool> pecEnabled_{false};
    std::atomic<bool> pecCalibrated_{false};

    // Callbacks
    TrackingStateCallback trackingStateCallback_;
    TrackingErrorCallback trackingErrorCallback_;

    // Internal methods
    void updateTrackingStatus();
    void calculateTrackingError();
    void updateTrackingStatistics();
    void handlePropertyUpdate(const std::string& propertyName);
    
    // Rate calculations
    MotionRates calculateSiderealRates() const;
    MotionRates calculateSolarRates() const;
    MotionRates calculateLunarRates() const;
    
    // Validation methods
    bool validateTrackRates(double raRate, double decRate) const;
    bool isValidTrackMode(TrackMode mode) const;
    
    // Property helpers
    void syncTrackingStateToHardware();
    void syncTrackRatesToHardware();
    
    // Utility methods
    void logInfo(const std::string& message);
    void logWarning(const std::string& message);
    void logError(const std::string& message);
    
    // Constants for default rates
    static constexpr double SIDEREAL_RATE = 15.041067; // arcsec/sec
    static constexpr double SOLAR_RATE = 15.0;         // arcsec/sec  
    static constexpr double LUNAR_RATE = 14.515;       // arcsec/sec
};

} // namespace lithium::device::indi::telescope::components
