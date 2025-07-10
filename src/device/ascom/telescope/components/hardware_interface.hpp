/*
 * hardware_interface.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Telescope Hardware Interface Component

This component provides a clean interface to ASCOM Telescope APIs,
handling low-level hardware communication, device management,
and both COM and Alpaca protocol integration.

*************************************************/

#pragma once

#include <atomic>
#include <mutex>
#include <optional>
#include <string>
#include <vector>
#include <memory>

#include <boost/asio/io_context.hpp>

#include "../../alpaca_client.hpp"

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <comdef.h>
#include <comutil.h>
// clang-format on
#endif

namespace lithium::device::ascom::telescope::components {

/**
 * @brief Connection type enumeration
 */
enum class ConnectionType {
    COM_DRIVER,    // Windows COM/ASCOM driver
    ALPACA_REST    // ASCOM Alpaca REST protocol
};

/**
 * @brief ASCOM Telescope states
 */
enum class ASCOMTelescopeState {
    IDLE = 0,
    SLEWING = 1,
    TRACKING = 2,
    PARKED = 3,
    PARKING = 4,
    HOMING = 5,
    ERROR = 6
};

/**
 * @brief ASCOM Telescope types
 */
enum class ASCOMTelescopeType {
    EQUATORIAL_GERMAN_POLAR = 0,
    EQUATORIAL_FORK = 1,
    EQUATORIAL_OTHER = 2,
    ALTAZIMUTH = 3
};

/**
 * @brief ASCOM Guide directions
 */
enum class ASCOMGuideDirection {
    GUIDE_NORTH = 0,
    GUIDE_SOUTH = 1,
    GUIDE_EAST = 2,
    GUIDE_WEST = 3
};

/**
 * @brief Alignment modes
 */
enum class AlignmentMode {
    UNKNOWN = 0,
    ONE_STAR = 1,
    TWO_STAR = 2,
    THREE_STAR = 3,
    AUTO = 4
};

/**
 * @brief Equatorial coordinates structure
 */
struct EquatorialCoordinates {
    double ra;     // Right Ascension in hours
    double dec;    // Declination in degrees
};

/**
 * @brief Hardware Interface for ASCOM Telescope communication
 *
 * This component encapsulates all direct interaction with ASCOM Telescope APIs,
 * providing a clean C++ interface for hardware operations while managing
 * both COM driver and Alpaca REST communication, device enumeration,
 * connection management, and low-level parameter control.
 */
class HardwareInterface {
public:
    struct TelescopeInfo {
        std::string name;
        std::string description;
        std::string driverInfo;
        std::string driverVersion;
        std::string interfaceVersion;
        ASCOMTelescopeType telescopeType = ASCOMTelescopeType::EQUATORIAL_GERMAN_POLAR;
        double aperture = 0.0;          // meters
        double apertureArea = 0.0;      // square meters
        double focalLength = 0.0;       // meters
        bool canFindHome = false;
        bool canPark = false;
        bool canPulseGuide = false;
        bool canSetDeclinationRate = false;
        bool canSetGuideRates = false;
        bool canSetPark = false;
        bool canSetPierSide = false;
        bool canSetRightAscensionRate = false;
        bool canSetTracking = false;
        bool canSlew = false;
        bool canSlewAltAz = false;
        bool canSlewAltAzAsync = false;
        bool canSlewAsync = false;
        bool canSync = false;
        bool canSyncAltAz = false;
        bool canUnpark = false;
    };

    struct ConnectionSettings {
        ConnectionType type = ConnectionType::ALPACA_REST;
        std::string deviceName;
        
        // COM driver settings
        std::string progId;
        
        // Alpaca settings
        std::string host = "localhost";
        int port = 11111;
        int deviceNumber = 0;
        std::string clientId = "Lithium-Next";
        int clientTransactionId = 1;
    };

public:
    HardwareInterface(boost::asio::io_context& io_context);
    ~HardwareInterface();

    // Non-copyable and non-movable
    HardwareInterface(const HardwareInterface&) = delete;
    HardwareInterface& operator=(const HardwareInterface&) = delete;
    HardwareInterface(HardwareInterface&&) = delete;
    HardwareInterface& operator=(HardwareInterface&&) = delete;

    // =========================================================================
    // Initialization and Device Management
    // =========================================================================

    /**
     * @brief Initialize the hardware interface
     * @return true if initialization successful
     */
    bool initialize();

    /**
     * @brief Shutdown the hardware interface
     * @return true if shutdown successful
     */
    bool shutdown();

    /**
     * @brief Check if interface is initialized
     * @return true if initialized
     */
    bool isInitialized() const { return initialized_; }

    // =========================================================================
    // Device Discovery and Connection
    // =========================================================================

    /**
     * @brief Discover available ASCOM telescope devices
     * @return Vector of device names/identifiers
     */
    std::vector<std::string> discoverDevices();

    /**
     * @brief Connect to a telescope device
     * @param settings Connection settings
     * @return true if connection successful
     */
    bool connect(const ConnectionSettings& settings);

    /**
     * @brief Disconnect from current telescope
     * @return true if disconnection successful
     */
    bool disconnect();

    /**
     * @brief Check if connected to a telescope
     * @return true if connected
     */
    bool isConnected() const { return connected_; }

    /**
     * @brief Get connection type
     * @return Current connection type
     */
    ConnectionType getConnectionType() const { return connectionType_; }

    // =========================================================================
    // Telescope Information and Properties
    // =========================================================================

    /**
     * @brief Get telescope information
     * @return Optional telescope info structure
     */
    std::optional<TelescopeInfo> getTelescopeInfo() const;

    /**
     * @brief Get telescope state
     * @return Current telescope state
     */
    ASCOMTelescopeState getTelescopeState() const;

    /**
     * @brief Get interface version
     * @return ASCOM interface version
     */
    int getInterfaceVersion() const;

    /**
     * @brief Get driver info
     * @return Driver information string
     */
    std::string getDriverInfo() const;

    /**
     * @brief Get driver version
     * @return Driver version string
     */
    std::string getDriverVersion() const;

    // =========================================================================
    // Coordinate System
    // =========================================================================

    /**
     * @brief Get current Right Ascension
     * @return RA in hours
     */
    double getRightAscension() const;

    /**
     * @brief Get current Declination
     * @return Declination in degrees
     */
    double getDeclination() const;

    /**
     * @brief Get current Azimuth
     * @return Azimuth in degrees
     */
    double getAzimuth() const;

    /**
     * @brief Get current Altitude
     * @return Altitude in degrees
     */
    double getAltitude() const;

    /**
     * @brief Get target Right Ascension
     * @return Target RA in hours
     */
    double getTargetRightAscension() const;

    /**
     * @brief Get target Declination
     * @return Target Declination in degrees
     */
    double getTargetDeclination() const;

    // =========================================================================
    // Slewing Operations
    // =========================================================================

    /**
     * @brief Start slewing to RA/DEC coordinates
     * @param ra Right Ascension in hours
     * @param dec Declination in degrees
     * @return true if slew started successfully
     */
    bool slewToCoordinates(double ra, double dec);

    /**
     * @brief Start slewing to RA/DEC coordinates asynchronously
     * @param ra Right Ascension in hours
     * @param dec Declination in degrees
     * @return true if slew started successfully
     */
    bool slewToCoordinatesAsync(double ra, double dec);

    /**
     * @brief Start slewing to AZ/ALT coordinates
     * @param az Azimuth in degrees
     * @param alt Altitude in degrees
     * @return true if slew started successfully
     */
    bool slewToAltAz(double az, double alt);

    /**
     * @brief Start slewing to AZ/ALT coordinates asynchronously
     * @param az Azimuth in degrees
     * @param alt Altitude in degrees
     * @return true if slew started successfully
     */
    bool slewToAltAzAsync(double az, double alt);

    /**
     * @brief Sync telescope to coordinates
     * @param ra Right Ascension in hours
     * @param dec Declination in degrees
     * @return true if sync successful
     */
    bool syncToCoordinates(double ra, double dec);

    /**
     * @brief Sync telescope to AZ/ALT coordinates
     * @param az Azimuth in degrees
     * @param alt Altitude in degrees
     * @return true if sync successful
     */
    bool syncToAltAz(double az, double alt);

    /**
     * @brief Check if telescope is slewing
     * @return true if slewing
     */
    bool isSlewing() const;

    /**
     * @brief Abort current slew
     * @return true if abort successful
     */
    bool abortSlew();

    // =========================================================================
    // Tracking Control
    // =========================================================================

    /**
     * @brief Check if tracking is enabled
     * @return true if tracking
     */
    bool isTracking() const;

    /**
     * @brief Enable or disable tracking
     * @param enable true to enable tracking
     * @return true if operation successful
     */
    bool setTracking(bool enable);

    /**
     * @brief Get tracking rate
     * @return Tracking rate in arcsec/sec
     */
    double getTrackingRate() const;

    /**
     * @brief Set tracking rate
     * @param rate Tracking rate in arcsec/sec
     * @return true if operation successful
     */
    bool setTrackingRate(double rate);

    /**
     * @brief Get right ascension rate
     * @return RA rate in arcsec/sec
     */
    double getRightAscensionRate() const;

    /**
     * @brief Set right ascension rate
     * @param rate RA rate in arcsec/sec
     * @return true if operation successful
     */
    bool setRightAscensionRate(double rate);

    /**
     * @brief Get declination rate
     * @return DEC rate in arcsec/sec
     */
    double getDeclinationRate() const;

    /**
     * @brief Set declination rate
     * @param rate DEC rate in arcsec/sec
     * @return true if operation successful
     */
    bool setDeclinationRate(double rate);

    // =========================================================================
    // Parking Operations
    // =========================================================================

    /**
     * @brief Check if telescope is parked
     * @return true if parked
     */
    bool isParked() const;

    /**
     * @brief Park the telescope
     * @return true if park operation started
     */
    bool park();

    /**
     * @brief Unpark the telescope
     * @return true if unpark operation successful
     */
    bool unpark();

    /**
     * @brief Check if at park position
     * @return true if at park position
     */
    bool isAtPark() const;

    /**
     * @brief Set park position
     * @param ra Right Ascension in hours
     * @param dec Declination in degrees
     * @return true if operation successful
     */
    bool setPark();

    // =========================================================================
    // Homing Operations
    // =========================================================================

    /**
     * @brief Find home position
     * @return true if home finding started
     */
    bool findHome();

    /**
     * @brief Check if at home position
     * @return true if at home
     */
    bool isAtHome() const;

    // =========================================================================
    // Guide Operations
    // =========================================================================

    /**
     * @brief Send guide pulse
     * @param direction Guide direction
     * @param duration Duration in milliseconds
     * @return true if pulse sent successfully
     */
    bool pulseGuide(ASCOMGuideDirection direction, int duration);

    /**
     * @brief Check if pulse guiding
     * @return true if currently pulse guiding
     */
    bool isPulseGuiding() const;

    /**
     * @brief Get guide rate for Right Ascension
     * @return Guide rate in arcsec/sec
     */
    double getGuideRateRightAscension() const;

    /**
     * @brief Set guide rate for Right Ascension
     * @param rate Guide rate in arcsec/sec
     * @return true if operation successful
     */
    bool setGuideRateRightAscension(double rate);

    /**
     * @brief Get guide rate for Declination
     * @return Guide rate in arcsec/sec
     */
    double getGuideRateDeclination() const;

    /**
     * @brief Set guide rate for Declination
     * @param rate Guide rate in arcsec/sec
     * @return true if operation successful
     */
    bool setGuideRateDeclination(double rate);

    // =========================================================================
    // Alignment Operations
    // =========================================================================

    /**
     * @brief Get current alignment mode
     * @return Current alignment mode
     */
    std::optional<AlignmentMode> getAlignmentMode() const;

    /**
     * @brief Set alignment mode
     * @param mode New alignment mode
     * @return true if operation successful
     */
    bool setAlignmentMode(AlignmentMode mode);

    /**
     * @brief Add alignment point
     * @param measured Measured coordinates
     * @param target Target coordinates
     * @return true if operation successful
     */
    bool addAlignmentPoint(const EquatorialCoordinates& measured,
                          const EquatorialCoordinates& target);

    /**
     * @brief Clear all alignment points
     * @return true if operation successful
     */
    bool clearAlignment();

    /**
     * @brief Get number of alignment points
     * @return Number of alignment points, or std::nullopt on error
     */
    std::optional<int> getAlignmentPointCount() const;

    // =========================================================================
    // Error Handling
    // =========================================================================

    /**
     * @brief Get last error message
     * @return Error message string
     */
    std::string getLastError() const { return lastError_; }

    /**
     * @brief Clear last error
     */
    void clearError() { lastError_.clear(); }

private:
    // State management
    std::atomic<bool> initialized_{false};
    std::atomic<bool> connected_{false};
    mutable std::mutex mutex_;
    mutable std::mutex infoMutex_;
    
    // Connection details
    ConnectionType connectionType_{ConnectionType::ALPACA_REST};
    ConnectionSettings currentSettings_;
    std::string deviceName_;
    
    // Alpaca client integration
    boost::asio::io_context& io_context_;
    std::unique_ptr<lithium::device::ascom::DeviceClient<lithium::device::ascom::DeviceType::Telescope>> alpaca_client_;
    
    // Telescope information cache
    mutable std::optional<TelescopeInfo> telescopeInfo_;
    mutable std::chrono::steady_clock::time_point lastInfoUpdate_;
    
    // Error handling
    mutable std::string lastError_;

#ifdef _WIN32
    // COM interface
    IDispatch* comTelescope_ = nullptr;
    
    // COM helper methods
    auto invokeCOMMethod(const std::string& method, VARIANT* params = nullptr, int paramCount = 0) -> std::optional<VARIANT>;
    auto getCOMProperty(const std::string& property) -> std::optional<VARIANT>;
    auto setCOMProperty(const std::string& property, const VARIANT& value) -> bool;
#endif

    // Alpaca helper methods
    auto connectAlpaca(const ConnectionSettings& settings) -> bool;
    auto disconnectAlpaca() -> bool;
    
    // Connection type specific methods
    auto connectCOM(const ConnectionSettings& settings) -> bool;
    auto disconnectCOM() -> bool;
    
    // Alpaca discovery
    auto discoverAlpacaDevices() -> std::vector<std::string>;
    
    // Information caching
    auto updateTelescopeInfo() -> bool;
    auto shouldUpdateInfo() const -> bool;
    
    // Communication helper
    auto sendAlpacaRequest(const std::string& method, const std::string& endpoint, 
                          const std::string& params = "") const -> std::optional<std::string>;
    
    // Error handling helpers
    void setLastError(const std::string& error) const { lastError_ = error; }
};

} // namespace lithium::device::ascom::telescope::components
