/*
 * telescope.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Telescope Implementation

*************************************************/

#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <string_view>
#include <thread>
#include <chrono>

#ifdef _WIN32
#include <windows.h>
#include <comdef.h>
#include <comutil.h>
#endif

#include "device/template/telescope.hpp"

// ASCOM-specific types and constants
enum class ASCOMTelescopeType {
    EQUATORIAL_GERMAN_POLAR = 0,
    EQUATORIAL_FORK = 1,
    EQUATORIAL_OTHER = 2,
    ALTAZIMUTH = 3
};

enum class ASCOMGuideDirection {
    GUIDE_NORTH = 0,
    GUIDE_SOUTH = 1,
    GUIDE_EAST = 2,
    GUIDE_WEST = 3
};

enum class ASCOMDriveRate {
    SIDEREAL = 0,
    LUNAR = 1,
    SOLAR = 2,
    KING = 3  
};

// ASCOM Alpaca REST API constants
constexpr const char* ASCOM_ALPACA_API_VERSION = "v1";
constexpr int ASCOM_ALPACA_DEFAULT_PORT = 11111;
constexpr int ASCOM_ALPACA_DISCOVERY_PORT = 32227;

class ASCOMTelescope : public AtomTelescope {
public:
    explicit ASCOMTelescope(std::string name);
    ~ASCOMTelescope() override;

    // Basic device operations
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string &deviceName, int timeout, int maxRetry) -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;
    auto isConnected() const -> bool override;

    // Telescope information
    auto getTelescopeInfo() -> std::optional<TelescopeParameters> override;
    auto setTelescopeInfo(double aperture, double focalLength,
                          double guiderAperture, double guiderFocalLength) -> bool override;

    // Pier side
    auto getPierSide() -> std::optional<PierSide> override;
    auto setPierSide(PierSide side) -> bool override;

    // Tracking
    auto getTrackRate() -> std::optional<TrackMode> override;
    auto setTrackRate(TrackMode rate) -> bool override;
    auto isTrackingEnabled() -> bool override;
    auto enableTracking(bool enable) -> bool override;
    auto getTrackRates() -> MotionRates override;
    auto setTrackRates(const MotionRates &rates) -> bool override;

    // Motion control
    auto abortMotion() -> bool override;
    auto getStatus() -> std::optional<std::string> override;
    auto emergencyStop() -> bool override;
    auto isMoving() -> bool override;

    // Parking
    auto setParkOption(ParkOptions option) -> bool override;
    auto getParkPosition() -> std::optional<EquatorialCoordinates> override;
    auto setParkPosition(double ra, double dec) -> bool override;
    auto isParked() -> bool override;
    auto park() -> bool override;
    auto unpark() -> bool override;
    auto canPark() -> bool override;

    // Home position
    auto initializeHome(std::string_view command = "") -> bool override;
    auto findHome() -> bool override;
    auto setHome() -> bool override;
    auto gotoHome() -> bool override;

    // Slew rates
    auto getSlewRate() -> std::optional<double> override;
    auto setSlewRate(double speed) -> bool override;
    auto getSlewRates() -> std::vector<double> override;
    auto setSlewRateIndex(int index) -> bool override;

    // Directional movement
    auto getMoveDirectionEW() -> std::optional<MotionEW> override;
    auto setMoveDirectionEW(MotionEW direction) -> bool override;
    auto getMoveDirectionNS() -> std::optional<MotionNS> override;
    auto setMoveDirectionNS(MotionNS direction) -> bool override;
    auto startMotion(MotionNS ns_direction, MotionEW ew_direction) -> bool override;
    auto stopMotion(MotionNS ns_direction, MotionEW ew_direction) -> bool override;

    // Guiding
    auto guideNS(int direction, int duration) -> bool override;
    auto guideEW(int direction, int duration) -> bool override;
    auto guidePulse(double ra_ms, double dec_ms) -> bool override;

    // Coordinate systems
    auto getRADECJ2000() -> std::optional<EquatorialCoordinates> override;
    auto setRADECJ2000(double raHours, double decDegrees) -> bool override;

    auto getRADECJNow() -> std::optional<EquatorialCoordinates> override;
    auto setRADECJNow(double raHours, double decDegrees) -> bool override;

    auto getTargetRADECJNow() -> std::optional<EquatorialCoordinates> override;
    auto setTargetRADECJNow(double raHours, double decDegrees) -> bool override;

    auto slewToRADECJNow(double raHours, double decDegrees, bool enableTracking = true) -> bool override;
    auto syncToRADECJNow(double raHours, double decDegrees) -> bool override;

    auto getAZALT() -> std::optional<HorizontalCoordinates> override;
    auto setAZALT(double azDegrees, double altDegrees) -> bool override;
    auto slewToAZALT(double azDegrees, double altDegrees) -> bool override;

    // Location and time
    auto getLocation() -> std::optional<GeographicLocation> override;
    auto setLocation(const GeographicLocation &location) -> bool override;
    auto getUTCTime() -> std::optional<std::chrono::system_clock::time_point> override;
    auto setUTCTime(const std::chrono::system_clock::time_point &time) -> bool override;
    auto getLocalTime() -> std::optional<std::chrono::system_clock::time_point> override;

    // Alignment
    auto getAlignmentMode() -> AlignmentMode override;
    auto setAlignmentMode(AlignmentMode mode) -> bool override;
    auto addAlignmentPoint(const EquatorialCoordinates &measured,
                           const EquatorialCoordinates &target) -> bool override;
    auto clearAlignment() -> bool override;

    // Utility methods
    auto degreesToDMS(double degrees) -> std::tuple<int, int, double> override;
    auto degreesToHMS(double degrees) -> std::tuple<int, int, double> override;

    // ASCOM-specific methods
    auto getASCOMDriverInfo() -> std::optional<std::string>;
    auto getASCOMVersion() -> std::optional<std::string>;
    auto getASCOMInterfaceVersion() -> std::optional<int>;
    auto setASCOMClientID(const std::string &clientId) -> bool;
    auto getASCOMClientID() -> std::optional<std::string>;

    // ASCOM Telescope-specific properties
    auto canPulseGuide() -> bool;
    auto canSetDeclinationRate() -> bool;
    auto canSetGuideRates() -> bool;
    auto canSetPark() -> bool;
    auto canSetPierSide() -> bool;
    auto canSetRightAscensionRate() -> bool;
    auto canSetTracking() -> bool;
    auto canSlew() -> bool;
    auto canSlewAltAz() -> bool;
    auto canSlewAltAzAsync() -> bool;
    auto canSlewAsync() -> bool;
    auto canSync() -> bool;
    auto canSyncAltAz() -> bool;
    auto canUnpark() -> bool;

    // ASCOM rates and capabilities
    auto getDeclinationRate() -> std::optional<double>;
    auto setDeclinationRate(double rate) -> bool;
    auto getRightAscensionRate() -> std::optional<double>;
    auto setRightAscensionRate(double rate) -> bool;
    auto getGuideRateDeclinationRate() -> std::optional<double>;
    auto setGuideRateDeclinationRate(double rate) -> bool;
    auto getGuideRateRightAscensionRate() -> std::optional<double>;
    auto setGuideRateRightAscensionRate(double rate) -> bool;

    // ASCOM Alpaca discovery and connection
    auto discoverAlpacaDevices() -> std::vector<std::string>;
    auto connectToAlpacaDevice(const std::string &host, int port, int deviceNumber) -> bool;
    auto disconnectFromAlpacaDevice() -> bool;

    // ASCOM COM object connection (Windows only)
#ifdef _WIN32
    auto connectToCOMDriver(const std::string &progId) -> bool;
    auto disconnectFromCOMDriver() -> bool;
    auto showASCOMChooser() -> std::optional<std::string>;
#endif

protected:
    // Connection management
    enum class ConnectionType {
        COM_DRIVER,
        ALPACA_REST
    } connection_type_{ConnectionType::ALPACA_REST};

    // Device state
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> is_slewing_{false};
    std::atomic<bool> is_tracking_{false};
    std::atomic<bool> is_parked_{false};

    // ASCOM device information
    std::string device_name_;
    std::string driver_info_;
    std::string driver_version_;
    std::string client_id_{"Lithium-Next"};
    int interface_version_{3};

    // Alpaca connection details
    std::string alpaca_host_{"localhost"};
    int alpaca_port_{ASCOM_ALPACA_DEFAULT_PORT};
    int alpaca_device_number_{0};

#ifdef _WIN32
    // COM object for Windows ASCOM drivers
    IDispatch* com_telescope_{nullptr};
    std::string com_prog_id_;
#endif

    // Capabilities cache
    struct ASCOMCapabilities {
        bool can_pulse_guide{false};
        bool can_set_declination_rate{false};
        bool can_set_guide_rates{false};
        bool can_set_park{false};
        bool can_set_pier_side{false};
        bool can_set_right_ascension_rate{false};
        bool can_set_tracking{false};
        bool can_slew{false};
        bool can_slew_alt_az{false};
        bool can_slew_alt_az_async{false};
        bool can_slew_async{false};
        bool can_sync{false};
        bool can_sync_alt_az{false};
        bool can_unpark{false};
    } ascom_capabilities_;

    // Threading for async operations
    std::unique_ptr<std::thread> monitor_thread_;
    std::atomic<bool> stop_monitoring_{false};

    // Helper methods
    auto sendAlpacaRequest(const std::string &method, const std::string &endpoint,
                          const std::string &params = "") -> std::optional<std::string>;
    auto parseAlpacaResponse(const std::string &response) -> std::optional<std::string>;
    auto updateCapabilities() -> bool;
    auto startMonitoring() -> void;
    auto stopMonitoring() -> void;
    auto monitoringLoop() -> void;

#ifdef _WIN32
    auto invokeCOMMethod(const std::string &method, VARIANT* params = nullptr, 
                        int param_count = 0) -> std::optional<VARIANT>;
    auto getCOMProperty(const std::string &property) -> std::optional<VARIANT>;
    auto setCOMProperty(const std::string &property, const VARIANT &value) -> bool;
#endif
};
