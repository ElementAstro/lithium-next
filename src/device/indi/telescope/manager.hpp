#pragma once

#include <memory>
#include <atomic>
#include <functional>
#include <spdlog/spdlog.h>
#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include "device/template/telescope.hpp"
#include "connection.hpp"
#include "motion.hpp"
#include "tracking.hpp"
#include "coordinates.hpp"
#include "parking.hpp"
#include "indi.hpp"

/**
 * @brief Enhanced INDI telescope implementation with component-based architecture
 *
 * This class orchestrates multiple specialized components to provide comprehensive
 * telescope control functionality following INDI protocol standards.
 */
class INDITelescopeManager : public INDI::BaseClient, public AtomTelescope {
public:
    explicit INDITelescopeManager(std::string name);
    ~INDITelescopeManager() override = default;

    // Basic device operations
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string &deviceName, int timeout = 5, int maxRetry = 3) -> bool override;
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
    auto setTrackRates(const MotionRates& rates) -> bool override;

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
    auto setLocation(const GeographicLocation& location) -> bool override;
    auto getUTCTime() -> std::optional<std::chrono::system_clock::time_point> override;
    auto setUTCTime(const std::chrono::system_clock::time_point& time) -> bool override;
    auto getLocalTime() -> std::optional<std::chrono::system_clock::time_point> override;

    // Alignment
    auto getAlignmentMode() -> AlignmentMode override;
    auto setAlignmentMode(AlignmentMode mode) -> bool override;
    auto addAlignmentPoint(const EquatorialCoordinates& measured,
                           const EquatorialCoordinates& target) -> bool override;
    auto clearAlignment() -> bool override;

    // Utility methods
    auto degreesToDMS(double degrees) -> std::tuple<int, int, double> override;
    auto degreesToHMS(double degrees) -> std::tuple<int, int, double> override;

    // INDI BaseClient overrides
    void newMessage(INDI::BaseDevice baseDevice, int messageID) override;

    // Component access (for advanced usage)
    auto getConnectionComponent() -> std::shared_ptr<TelescopeConnection> { return connection_; }
    auto getMotionComponent() -> std::shared_ptr<TelescopeMotion> { return motion_; }
    auto getTrackingComponent() -> std::shared_ptr<TelescopeTracking> { return tracking_; }
    auto getCoordinatesComponent() -> std::shared_ptr<TelescopeCoordinates> { return coordinates_; }
    auto getParkingComponent() -> std::shared_ptr<TelescopeParking> { return parking_; }
    auto getINDIComponent() -> std::shared_ptr<TelescopeINDI> { return indi_; }

    // INDI virtual method overrides
    auto MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand cmd) -> bool override;
    auto MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand cmd) -> bool override;
    auto Abort() -> bool override;
    auto Park() -> bool override;
    auto UnPark() -> bool override;
    auto SetTrackMode(uint8_t mode) -> bool override;
    auto SetTrackEnabled(bool enabled) -> bool override;
    auto SetTrackRate(double raRate, double deRate) -> bool override;
    auto Goto(double ra, double dec) -> bool override;
    auto Sync(double ra, double dec) -> bool override;
    auto UpdateLocation(double latitude, double longitude, double elevation) -> bool override;
    auto UpdateTime(ln_date* utc, double utc_offset) -> bool override;
    auto ReadScopeParameters() -> bool override;
    auto SetCurrentPark() -> bool override;
    auto SetDefaultPark() -> bool override;

    // INDI callback overrides
    auto saveConfigItems(void* fp) -> bool override;
    auto ISNewNumber(const char *dev, const char *name, double values[],
                   char *names[], int n) -> bool override;
    auto ISNewSwitch(const char *dev, const char *name, ISState *states,
                   char *names[], int n) -> bool override;
    auto ISNewText(const char *dev, const char *name, char *texts[],
                 char *names[], int n) -> bool override;
    auto ISNewBLOB(const char *dev, const char *name, int sizes[], int blobsizes[],
                 char *blobs[], char *formats[], char *names[], int n) -> bool override;
    auto getProperties(const char *dev) -> void override;
    auto TimerHit() -> void override;
    auto getDefaultName() -> const char* override;
    auto initProperties() -> bool override;
    auto updateProperties() -> bool override;
    auto Connect() -> bool override;
    auto Disconnect() -> bool override;

private:
    std::string name_;

    // Component instances
    std::shared_ptr<TelescopeConnection> connection_;
    std::shared_ptr<TelescopeMotion> motion_;
    std::shared_ptr<TelescopeTracking> tracking_;
    std::shared_ptr<TelescopeCoordinates> coordinates_;
    std::shared_ptr<TelescopeParking> parking_;
    std::shared_ptr<TelescopeINDI> indi_;

    // State management
    std::atomic_bool initialized_{false};
    AlignmentMode alignmentMode_{AlignmentMode::EQ_NORTH_POLE};

    // Telescope parameters
    TelescopeParameters telescopeParams_{};

    // Helper methods
    auto initializeComponents() -> bool;
    auto destroyComponents() -> bool;
    auto ensureConnected() -> bool;
    auto updateTelescopeState() -> void;
};
