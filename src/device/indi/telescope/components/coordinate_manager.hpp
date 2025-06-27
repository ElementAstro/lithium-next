/*
 * coordinate_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-23

Description: INDI Telescope Coordinate Manager Component

This component manages telescope coordinate systems, transformations,
location/time settings, and coordinate validation.

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

#include "device/template/telescope.hpp"

namespace lithium::device::indi::telescope::components {

class HardwareInterface;

/**
 * @brief Coordinate Manager for INDI Telescope
 * 
 * Manages all coordinate system operations including coordinate transformations,
 * location and time management, alignment, and coordinate validation.
 */
class CoordinateManager {
public:
    struct CoordinateStatus {
        EquatorialCoordinates currentRADEC;
        EquatorialCoordinates targetRADEC;
        HorizontalCoordinates currentAltAz;
        HorizontalCoordinates targetAltAz;
        GeographicLocation location;
        std::chrono::system_clock::time_point currentTime;
        double julianDate = 0.0;
        double localSiderealTime = 0.0; // hours
        bool coordinatesValid = false;
        std::string lastError;
    };

    struct AlignmentPoint {
        EquatorialCoordinates measured;
        EquatorialCoordinates target;
        HorizontalCoordinates altAz;
        std::chrono::system_clock::time_point timestamp;
        double errorRA = 0.0;   // arcsec
        double errorDEC = 0.0;  // arcsec
        std::string name;
    };

    struct AlignmentModel {
        AlignmentMode mode = AlignmentMode::EQ_NORTH_POLE;
        std::vector<AlignmentPoint> points;
        double rmsError = 0.0;  // arcsec
        bool isActive = false;
        std::chrono::system_clock::time_point lastUpdate;
        std::string modelName;
    };

    using CoordinateUpdateCallback = std::function<void(const CoordinateStatus& status)>;
    using AlignmentUpdateCallback = std::function<void(const AlignmentModel& model)>;

public:
    explicit CoordinateManager(std::shared_ptr<HardwareInterface> hardware);
    ~CoordinateManager();

    // Non-copyable and non-movable
    CoordinateManager(const CoordinateManager&) = delete;
    CoordinateManager& operator=(const CoordinateManager&) = delete;
    CoordinateManager(CoordinateManager&&) = delete;
    CoordinateManager& operator=(CoordinateManager&&) = delete;

    // Initialization
    bool initialize();
    bool shutdown();
    bool isInitialized() const { return initialized_; }

    // Coordinate Access
    std::optional<EquatorialCoordinates> getCurrentRADEC() const;
    std::optional<EquatorialCoordinates> getTargetRADEC() const;
    std::optional<HorizontalCoordinates> getCurrentAltAz() const;
    std::optional<HorizontalCoordinates> getTargetAltAz() const;

    // Coordinate Setting
    bool setTargetRADEC(const EquatorialCoordinates& coords);
    bool setTargetRADEC(double ra, double dec);
    bool setTargetAltAz(const HorizontalCoordinates& coords);
    bool setTargetAltAz(double azimuth, double altitude);

    // Coordinate Transformations
    std::optional<HorizontalCoordinates> raDECToAltAz(const EquatorialCoordinates& radec) const;
    std::optional<EquatorialCoordinates> altAzToRADEC(const HorizontalCoordinates& altaz) const;
    std::optional<EquatorialCoordinates> j2000ToJNow(const EquatorialCoordinates& j2000) const;
    std::optional<EquatorialCoordinates> jNowToJ2000(const EquatorialCoordinates& jnow) const;

    // Location and Time Management
    bool setLocation(const GeographicLocation& location);
    std::optional<GeographicLocation> getLocation() const;
    bool setTime(const std::chrono::system_clock::time_point& time);
    std::optional<std::chrono::system_clock::time_point> getTime() const;
    bool syncTimeWithSystem();

    // Time Calculations
    double getJulianDate() const;
    double getLocalSiderealTime() const; // hours
    double getGreenwichSiderealTime() const; // hours
    std::chrono::system_clock::time_point getLocalTime() const;

    // Coordinate Validation
    bool validateRADEC(const EquatorialCoordinates& coords) const;
    bool validateAltAz(const HorizontalCoordinates& coords) const;
    bool isAboveHorizon(const EquatorialCoordinates& coords) const;
    bool isWithinSlewLimits(const EquatorialCoordinates& coords) const;

    // Alignment System
    bool addAlignmentPoint(const EquatorialCoordinates& measured, 
                          const EquatorialCoordinates& target);
    bool addAlignmentPoint(const AlignmentPoint& point);
    bool removeAlignmentPoint(size_t index);
    bool clearAlignment();
    AlignmentModel getCurrentAlignmentModel() const;
    bool setAlignmentMode(AlignmentMode mode);
    AlignmentMode getAlignmentMode() const;

    // Alignment Operations
    bool performAlignment();
    bool isAlignmentActive() const;
    double getAlignmentRMSError() const;
    size_t getAlignmentPointCount() const;
    std::vector<AlignmentPoint> getAlignmentPoints() const;

    // Coordinate Correction
    EquatorialCoordinates applyAlignmentCorrection(const EquatorialCoordinates& coords) const;
    EquatorialCoordinates removeAlignmentCorrection(const EquatorialCoordinates& coords) const;

    // Status and Information
    CoordinateStatus getCoordinateStatus() const;
    std::string getCoordinateStatusString() const;
    bool areCoordinatesValid() const;

    // Utility Functions
    std::tuple<int, int, double> degreesToDMS(double degrees) const;
    std::tuple<int, int, double> degreesToHMS(double degrees) const;
    double dmsToDecimal(int degrees, int minutes, double seconds) const;
    double hmsToDecimal(int hours, int minutes, double seconds) const;

    // Angular Calculations
    double angularSeparation(const EquatorialCoordinates& coord1, 
                           const EquatorialCoordinates& coord2) const;
    double positionAngle(const EquatorialCoordinates& from, 
                        const EquatorialCoordinates& to) const;

    // Callback Registration
    void setCoordinateUpdateCallback(CoordinateUpdateCallback callback) { coordinateUpdateCallback_ = std::move(callback); }
    void setAlignmentUpdateCallback(AlignmentUpdateCallback callback) { alignmentUpdateCallback_ = std::move(callback); }

    // Advanced Features
    bool saveAlignmentModel(const std::string& filename) const;
    bool loadAlignmentModel(const std::string& filename);
    bool enableAutomaticAlignment(bool enable);
    bool setCoordinateUpdateRate(double rateHz);

private:
    // Hardware interface
    std::shared_ptr<HardwareInterface> hardware_;

    // State management
    std::atomic<bool> initialized_{false};
    mutable std::recursive_mutex coordinateMutex_;

    // Current coordinate status
    CoordinateStatus currentStatus_;
    std::atomic<bool> coordinatesValid_{false};

    // Location and time
    GeographicLocation currentLocation_;
    std::chrono::system_clock::time_point lastTimeUpdate_;
    std::atomic<bool> locationValid_{false};

    // Alignment model
    AlignmentModel alignmentModel_;
    std::atomic<bool> alignmentActive_{false};

    // Callbacks
    CoordinateUpdateCallback coordinateUpdateCallback_;
    AlignmentUpdateCallback alignmentUpdateCallback_;

    // Internal methods
    void updateCoordinateStatus();
    void handlePropertyUpdate(const std::string& propertyName);
    void calculateDerivedCoordinates();
    
    // Time calculations
    double calculateJulianDate(const std::chrono::system_clock::time_point& time) const;
    double calculateLocalSiderealTime(double jd, double longitude) const;
    double calculateGreenwichSiderealTime(double jd) const;
    
    // Coordinate transformation implementations
    HorizontalCoordinates equatorialToHorizontal(const EquatorialCoordinates& eq,
                                                double lst, double latitude) const;
    EquatorialCoordinates horizontalToEquatorial(const HorizontalCoordinates& hz,
                                               double lst, double latitude) const;
    
    // Precession and nutation
    EquatorialCoordinates applyPrecession(const EquatorialCoordinates& coords,
                                        double fromEpoch, double toEpoch) const;
    
    // Alignment calculations
    void calculateAlignmentModel();
    double calculateAlignmentRMS() const;
    
    // Validation helpers
    bool isValidRA(double ra) const;
    bool isValidDEC(double dec) const;
    bool isValidAzimuth(double az) const;
    bool isValidAltitude(double alt) const;
    
    // Hardware synchronization
    void syncCoordinatesToHardware();
    void syncLocationToHardware();
    void syncTimeToHardware();
    
    // Utility methods
    void logInfo(const std::string& message);
    void logWarning(const std::string& message);
    void logError(const std::string& message);
    
    // Mathematical constants
    static constexpr double DEGREES_PER_HOUR = 15.0;
    static constexpr double ARCSEC_PER_DEGREE = 3600.0;
    static constexpr double J2000_EPOCH = 2451545.0;
};

} // namespace lithium::device::indi::telescope::components
