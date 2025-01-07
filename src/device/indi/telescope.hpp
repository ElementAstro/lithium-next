#ifndef LITHIUM_CLIENT_INDI_TELESCOPE_HPP
#define LITHIUM_CLIENT_INDI_TELESCOPE_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <atomic>
#include <optional>
#include <string_view>

#include "device/template/telescope.hpp"

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

    auto getTelescopeInfo()
        -> std::optional<std::tuple<double, double, double, double>> override;
    auto setTelescopeInfo(double telescopeAperture, double telescopeFocal,
                          double guiderAperture,
                          double guiderFocal) -> bool override;
    auto getPierSide() -> std::optional<PierSide> override;

    auto getTrackRate() -> std::optional<TrackMode> override;
    auto setTrackRate(TrackMode rate) -> bool override;

    auto isTrackingEnabled() -> bool override;
    auto enableTracking(bool enable) -> bool override;

    auto abortMotion() -> bool override;
    auto getStatus() -> std::optional<std::string> override;

    auto setParkOption(ParkOptions option) -> bool override;

    auto getParkPosition() -> std::optional<std::pair<double, double>> override;
    auto setParkPosition(double parkRA, double parkDEC) -> bool override;

    auto isParked() -> bool override;
    auto park(bool isParked) -> bool override;

    auto initializeHome(std::string_view command) -> bool override;

    auto getSlewRate() -> std::optional<double> override;
    auto setSlewRate(double speed) -> bool override;
    auto getTotalSlewRate() -> std::optional<double> override;

    auto getMoveDirectionEW() -> std::optional<MotionEW> override;
    auto setMoveDirectionEW(MotionEW direction) -> bool override;
    auto getMoveDirectionNS() -> std::optional<MotionNS> override;
    auto setMoveDirectionNS(MotionNS direction) -> bool override;

    auto guideNS(int dir, int timeGuide) -> bool override;
    auto guideEW(int dir, int timeGuide) -> bool override;

    auto setActionAfterPositionSet(std::string_view action) -> bool override;

    auto getRADECJ2000() -> std::optional<std::pair<double, double>> override;
    auto setRADECJ2000(double RAHours, double DECDegree) -> bool override;

    auto getRADECJNow() -> std::optional<std::pair<double, double>> override;
    auto setRADECJNow(double RAHours, double DECDegree) -> bool override;

    auto getTargetRADECJNow()
        -> std::optional<std::pair<double, double>> override;
    auto setTargetRADECJNow(double RAHours, double DECDegree) -> bool override;
    auto slewToRADECJNow(double RAHours, double DECDegree,
                         bool EnableTracking) -> bool override;

    auto syncToRADECJNow(double RAHours, double DECDegree) -> bool override;
    auto getAZALT() -> std::optional<std::pair<double, double>> override;
    auto setAZALT(double AZ_DEGREE, double ALT_DEGREE) -> bool override;

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
};

#endif