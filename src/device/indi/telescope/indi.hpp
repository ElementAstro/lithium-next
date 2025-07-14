#pragma once

#include <optional>
#include <atomic>
#include <spdlog/spdlog.h>
#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include "device/template/telescope.hpp"

/**
 * @brief INDI-specific implementations for telescope interface
 *
 * Handles INDI protocol-specific methods and property handling
 */
class TelescopeINDI {
public:
    explicit TelescopeINDI(const std::string& name);
    ~TelescopeINDI() = default;

    /**
     * @brief Initialize INDI-specific component
     */
    auto initialize(INDI::BaseDevice device) -> bool;

    /**
     * @brief Destroy INDI component
     */
    auto destroy() -> bool;

    // INDI-specific virtual method implementations
    /**
     * @brief Move telescope north/south (INDI virtual method)
     * @param dir Direction (NORTH/SOUTH)
     * @param cmd Command (START/STOP)
     */
    auto MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand cmd) -> bool;

    /**
     * @brief Move telescope west/east (INDI virtual method)
     * @param dir Direction (WEST/EAST)
     * @param cmd Command (START/STOP)
     */
    auto MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand cmd) -> bool;

    /**
     * @brief Abort telescope motion (INDI virtual method)
     */
    auto Abort() -> bool;

    /**
     * @brief Park telescope (INDI virtual method)
     */
    auto Park() -> bool;

    /**
     * @brief Unpark telescope (INDI virtual method)
     */
    auto UnPark() -> bool;

    /**
     * @brief Set tracking mode (INDI virtual method)
     * @param mode Tracking mode
     */
    auto SetTrackMode(uint8_t mode) -> bool;

    /**
     * @brief Enable/disable tracking (INDI virtual method)
     * @param enabled Tracking state
     */
    auto SetTrackEnabled(bool enabled) -> bool;

    /**
     * @brief Set tracking rate (INDI virtual method)
     * @param raRate RA tracking rate
     * @param deRate DEC tracking rate
     */
    auto SetTrackRate(double raRate, double deRate) -> bool;

    /**
     * @brief Goto coordinates (INDI virtual method)
     * @param ra Right ascension
     * @param dec Declination
     */
    auto Goto(double ra, double dec) -> bool;

    /**
     * @brief Sync coordinates (INDI virtual method)
     * @param ra Right ascension
     * @param dec Declination
     */
    auto Sync(double ra, double dec) -> bool;

    /**
     * @brief Update location (INDI virtual method)
     * @param latitude Latitude in degrees
     * @param longitude Longitude in degrees
     * @param elevation Elevation in meters
     */
    auto UpdateLocation(double latitude, double longitude, double elevation) -> bool;

    /**
     * @brief Update time (INDI virtual method)
     * @param utc UTC time string
     * @param utc_offset UTC offset
     */
    auto UpdateTime(ln_date* utc, double utc_offset) -> bool;

    /**
     * @brief Read telescope scope parameters (INDI virtual method)
     * @param primaryFocalLength Primary focal length
     * @param primaryAperture Primary aperture
     * @param guiderFocalLength Guider focal length
     * @param guiderAperture Guider aperture
     */
    auto ReadScopeParameters() -> bool;

    // Additional INDI methods
    /**
     * @brief Set current park position (INDI virtual method)
     */
    auto SetCurrentPark() -> bool;

    /**
     * @brief Set default park position (INDI virtual method)
     */
    auto SetDefaultPark() -> bool;

    /**
     * @brief Save configuration data (INDI virtual method)
     */
    auto saveConfigItems(FILE *fp) -> bool;

    /**
     * @brief Handle new number property (INDI virtual method)
     */
    auto ISNewNumber(const char *dev, const char *name, double values[], char *names[], int n) -> bool;

    /**
     * @brief Handle new switch property (INDI virtual method)
     */
    auto ISNewSwitch(const char *dev, const char *name, ISState *states, char *names[], int n) -> bool;

    /**
     * @brief Handle new text property (INDI virtual method)
     */
    auto ISNewText(const char *dev, const char *name, char *texts[], char *names[], int n) -> bool;

    /**
     * @brief Handle new BLOB property (INDI virtual method)
     */
    auto ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n) -> bool;

    /**
     * @brief Get device properties (INDI virtual method)
     */
    auto getProperties(const char *dev) -> void;

    /**
     * @brief Timer hit handler (INDI virtual method)
     */
    auto TimerHit() -> void;

    /**
     * @brief Get default name (INDI virtual method)
     */
    auto getDefaultName() -> const char*;

    /**
     * @brief Initialize properties (INDI virtual method)
     */
    auto initProperties() -> bool;

    /**
     * @brief Update properties (INDI virtual method)
     */
    auto updateProperties() -> bool;

    /**
     * @brief Connect to device (INDI virtual method)
     */
    auto Connect() -> bool;

    /**
     * @brief Disconnect from device (INDI virtual method)
     */
    auto Disconnect() -> bool;

    // Capability methods
    /**
     * @brief Set telescope capabilities
     */
    auto SetTelescopeCapability(uint32_t cap, uint8_t slewRateCount) -> void;

    /**
     * @brief Set park data type
     */
    auto SetParkDataType(TelescopeParkData type) -> void;

    /**
     * @brief Initialize park data
     */
    auto InitPark() -> bool;

    /**
     * @brief Check if telescope has tracking
     */
    auto HasTrackMode() -> bool;

    /**
     * @brief Check if telescope has tracking rate
     */
    auto HasTrackRate() -> bool;

    /**
     * @brief Check if telescope has location
     */
    auto HasLocation() -> bool;

    /**
     * @brief Check if telescope has time
     */
    auto HasTime() -> bool;

    /**
     * @brief Check if telescope has pier side
     */
    auto HasPierSide() -> bool;

    /**
     * @brief Check if telescope has pier side simulation
     */
    auto HasPierSideSimulation() -> bool;

private:
    std::string name_;
    INDI::BaseDevice device_;

    // INDI state
    std::atomic_bool indiConnected_{false};
    std::atomic_bool indiInitialized_{false};

    // Telescope capabilities
    uint32_t telescopeCapability_{0};
    uint8_t slewRateCount_{4};
    TelescopeParkData parkDataType_{PARK_NONE};

    // Helper methods
    auto processCoordinateUpdate() -> void;
    auto processTrackingUpdate() -> void;
    auto processParkingUpdate() -> void;
    auto handlePropertyUpdate(const char* name) -> void;
};
