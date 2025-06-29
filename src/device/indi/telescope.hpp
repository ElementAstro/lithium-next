#ifndef LITHIUM_CLIENT_INDI_TELESCOPE_HPP
#define LITHIUM_CLIENT_INDI_TELESCOPE_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <atomic>
#include <memory>
#include <optional>
#include <string_view>

#include "device/template/telescope.hpp"

// INDI-specific types and constants
enum class TelescopeMotionCommand { MOTION_START, MOTION_STOP };
enum class TelescopeParkData { PARK_NONE, PARK_RA_DEC, PARK_HA_DEC, PARK_AZ_ALT };

// INDI telescope capabilities (bitfield)
constexpr uint32_t TELESCOPE_CAN_GOTO = (1 << 0);
constexpr uint32_t TELESCOPE_CAN_SYNC = (1 << 1);
constexpr uint32_t TELESCOPE_CAN_PARK = (1 << 2);
constexpr uint32_t TELESCOPE_CAN_ABORT = (1 << 3);
constexpr uint32_t TELESCOPE_HAS_TRACK_MODE = (1 << 4);
constexpr uint32_t TELESCOPE_HAS_TRACK_RATE = (1 << 5);
constexpr uint32_t TELESCOPE_HAS_PIER_SIDE = (1 << 6);
constexpr uint32_t TELESCOPE_HAS_PIER_SIDE_SIMULATION = (1 << 7);
constexpr uint32_t TELESCOPE_HAS_LOCATION = (1 << 8);
constexpr uint32_t TELESCOPE_HAS_TIME = (1 << 9);
constexpr uint32_t TELESCOPE_CAN_CONTROL_TRACK = (1 << 10);
constexpr uint32_t TELESCOPE_HAS_TRACK_STATE = (1 << 11);

class INDITelescope : public INDI::BaseClient, public AtomTelescope {
public:
    explicit INDITelescope(std::string name);
    ~INDITelescope() override = default;

    auto initialize() -> bool override;

    auto destroy() -> bool override;

    auto connect(const std::string &deviceName, int timeout,
                 int maxRetry) -> bool override;

    auto disconnect() -> bool override;

    auto scan() -> std::vector<std::string> override;
    auto isConnected() const -> bool override;

    virtual auto watchAdditionalProperty() -> bool;

    void setPropertyNumber(std::string_view propertyName, double value);
    auto setActionAfterPositionSet(std::string_view action) -> bool;

    auto getTelescopeInfo()
        -> std::optional<TelescopeParameters> override;
    auto setTelescopeInfo(double telescopeAperture, double telescopeFocal,
                          double guiderAperture,
                          double guiderFocal) -> bool override;
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
    auto setParkPosition(double parkRA, double parkDEC) -> bool override;
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

    // INDI-specific virtual methods
    virtual auto MoveNS(INDI_DIR_NS dir, TelescopeMotionCommand cmd) -> bool;
    virtual auto MoveWE(INDI_DIR_WE dir, TelescopeMotionCommand cmd) -> bool;
    virtual auto Abort() -> bool;
    virtual auto Park() -> bool;
    virtual auto UnPark() -> bool;
    virtual auto SetTrackMode(uint8_t mode) -> bool;
    virtual auto SetTrackEnabled(bool enabled) -> bool;
    virtual auto SetTrackRate(double raRate, double deRate) -> bool;
    virtual auto Goto(double ra, double dec) -> bool;
    virtual auto Sync(double ra, double dec) -> bool;
    virtual auto UpdateLocation(double latitude, double longitude, double elevation) -> bool;
    virtual auto UpdateTime(ln_date *utc, double utc_offset) -> bool;
    virtual auto ReadScopeParameters() -> bool;
    virtual auto SetCurrentPark() -> bool;
    virtual auto SetDefaultPark() -> bool;

    // INDI callback interface methods
    virtual auto saveConfigItems(void *fp) -> bool;
    virtual auto ISNewNumber(const char *dev, const char *name, double values[],
                             char *names[], int n) -> bool;
    virtual auto ISNewSwitch(const char *dev, const char *name, ISState *states,
                             char *names[], int n) -> bool;
    virtual auto ISNewText(const char *dev, const char *name, char *texts[],
                           char *names[], int n) -> bool;
    virtual auto ISNewBLOB(const char *dev, const char *name, int sizes[],
                           int blobsizes[], char *blobs[], char *formats[],
                           char *names[], int n) -> bool;
    virtual auto getProperties(const char *dev) -> void;
    virtual auto TimerHit() -> void;
    virtual auto getDefaultName() -> const char *;
    virtual auto initProperties() -> bool;
    virtual auto updateProperties() -> bool;
    virtual auto Connect() -> bool;
    virtual auto Disconnect() -> bool;

protected:
    void newMessage(INDI::BaseDevice baseDevice, int messageID) override;

private:
    std::string name_;
    std::string deviceName_;

    std::string driverExec_;
    std::string driverVersion_;
    std::string driverInterface_;
    bool deviceAutoSearch_;
    bool devicePortScan_;

    std::atomic<double> currentPollingPeriod_;

    std::atomic_bool isDebug_;

    std::atomic_bool isConnected_;

    INDI::BaseDevice device_;
    INDI::BaseDevice gps_;
    INDI::BaseDevice dome_;
    INDI::BaseDevice joystick_;

    ConnectionMode connectionMode_;

    std::string devicePort_;
    T_BAUD_RATE baudRate_;

    bool isTrackingEnabled_;
    std::atomic_bool isTracking_;
    TrackMode trackMode_;
    std::atomic<double> trackRateRA_;
    std::atomic<double> trackRateDEC_;
    PierSide pierSide_;

    SlewRate slewRate_;
    int totalSlewRate_;
    double maxSlewRate_;
    double minSlewRate_;

    std::atomic<double> targetSlewRA_;
    std::atomic<double> targetSlewDEC_;

    MotionEW motionEW_;
    std::atomic_bool motionEWReserved_;
    MotionNS motionNS_;
    std::atomic_bool motionNSReserved_;

    double telescopeAperture_;
    double telescopeFocalLength_;
    double telescopeGuiderAperture_;
    double telescopeGuiderFocalLength_;

    bool isParkEnabled_;
    std::atomic_bool isParked_;
    double telescopeParkPositionRA_;
    double telescopeParkPositionDEC_;
    ParkOptions parkOption_;

    std::atomic_bool isHomed_;
    std::atomic_bool isHomeInitEnabled_;
    std::atomic_bool isHomeInitInProgress_;

    bool isJoystickEnabled_;

    DomePolicy domePolicy_;

    // Forward declaration
    class INDITelescopeManager;

    // Unique pointer to the manager
    std::unique_ptr<INDITelescopeManager> manager_;
};

#endif
