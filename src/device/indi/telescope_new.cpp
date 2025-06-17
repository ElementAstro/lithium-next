#include "telescope.hpp"
#include "telescope/manager.hpp"

#include <spdlog/spdlog.h>
#include "atom/components/component.hpp"

INDITelescope::INDITelescope(std::string name) : AtomTelescope(name) {
    // Use the new component-based manager
    manager_ = std::make_unique<INDITelescopeManager>(name);
}

auto INDITelescope::initialize() -> bool {
    return manager_->initialize();
}

auto INDITelescope::destroy() -> bool {
    return manager_->destroy();
}

auto INDITelescope::connect(const std::string &deviceName, int timeout, int maxRetry) -> bool {
    return manager_->connect(deviceName, timeout, maxRetry);
}

auto INDITelescope::disconnect() -> bool {
    return manager_->disconnect();
}

auto INDITelescope::scan() -> std::vector<std::string> {
    return manager_->scan();
}

auto INDITelescope::isConnected() const -> bool {
    return manager_->isConnected();
}

auto INDITelescope::watchAdditionalProperty() -> bool {
    // Delegate to manager components
    return true;
}

void INDITelescope::setPropertyNumber(std::string_view propertyName, double value) {
    // Implementation for setting INDI property numbers
    spdlog::debug("Setting property {}: {}", propertyName, value);
}

auto INDITelescope::setActionAfterPositionSet(std::string_view action) -> bool {
    // Use motion component
    return manager_->getMotionComponent()->setActionAfterPositionSet(action);
}

// Delegate all other methods to the manager
auto INDITelescope::getTelescopeInfo() -> std::optional<TelescopeParameters> {
    return manager_->getTelescopeInfo();
}

auto INDITelescope::setTelescopeInfo(double aperture, double focalLength,
                                     double guiderAperture, double guiderFocalLength) -> bool {
    return manager_->setTelescopeInfo(aperture, focalLength, guiderAperture, guiderFocalLength);
}

auto INDITelescope::getPierSide() -> std::optional<PierSide> {
    return manager_->getPierSide();
}

auto INDITelescope::setPierSide(PierSide side) -> bool {
    return manager_->setPierSide(side);
}

auto INDITelescope::getTrackRate() -> std::optional<TrackMode> {
    return manager_->getTrackRate();
}

auto INDITelescope::setTrackRate(TrackMode rate) -> bool {
    return manager_->setTrackRate(rate);
}

auto INDITelescope::isTrackingEnabled() -> bool {
    return manager_->isTrackingEnabled();
}

auto INDITelescope::enableTracking(bool enable) -> bool {
    return manager_->enableTracking(enable);
}

auto INDITelescope::getTrackRates() -> MotionRates {
    return manager_->getTrackRates();
}

auto INDITelescope::setTrackRates(const MotionRates& rates) -> bool {
    return manager_->setTrackRates(rates);
}

auto INDITelescope::abortMotion() -> bool {
    return manager_->abortMotion();
}

auto INDITelescope::getStatus() -> std::optional<std::string> {
    return manager_->getStatus();
}

auto INDITelescope::emergencyStop() -> bool {
    return manager_->emergencyStop();
}

auto INDITelescope::isMoving() -> bool {
    return manager_->isMoving();
}

auto INDITelescope::setParkOption(ParkOptions option) -> bool {
    return manager_->setParkOption(option);
}

auto INDITelescope::getParkPosition() -> std::optional<EquatorialCoordinates> {
    return manager_->getParkPosition();
}

auto INDITelescope::setParkPosition(double parkRA, double parkDEC) -> bool {
    return manager_->setParkPosition(parkRA, parkDEC);
}

auto INDITelescope::isParked() -> bool {
    return manager_->isParked();
}

auto INDITelescope::park() -> bool {
    return manager_->park();
}

auto INDITelescope::unpark() -> bool {
    return manager_->unpark();
}

auto INDITelescope::canPark() -> bool {
    return manager_->canPark();
}

auto INDITelescope::initializeHome(std::string_view command) -> bool {
    return manager_->initializeHome(command);
}

auto INDITelescope::findHome() -> bool {
    return manager_->findHome();
}

auto INDITelescope::setHome() -> bool {
    return manager_->setHome();
}

auto INDITelescope::gotoHome() -> bool {
    return manager_->gotoHome();
}

auto INDITelescope::getSlewRate() -> std::optional<double> {
    return manager_->getSlewRate();
}

auto INDITelescope::setSlewRate(double speed) -> bool {
    return manager_->setSlewRate(speed);
}

auto INDITelescope::getSlewRates() -> std::vector<double> {
    return manager_->getSlewRates();
}

auto INDITelescope::setSlewRateIndex(int index) -> bool {
    return manager_->setSlewRateIndex(index);
}

auto INDITelescope::getMoveDirectionEW() -> std::optional<MotionEW> {
    return manager_->getMoveDirectionEW();
}

auto INDITelescope::setMoveDirectionEW(MotionEW direction) -> bool {
    return manager_->setMoveDirectionEW(direction);
}

auto INDITelescope::getMoveDirectionNS() -> std::optional<MotionNS> {
    return manager_->getMoveDirectionNS();
}

auto INDITelescope::setMoveDirectionNS(MotionNS direction) -> bool {
    return manager_->setMoveDirectionNS(direction);
}

auto INDITelescope::startMotion(MotionNS ns_direction, MotionEW ew_direction) -> bool {
    return manager_->startMotion(ns_direction, ew_direction);
}

auto INDITelescope::stopMotion(MotionNS ns_direction, MotionEW ew_direction) -> bool {
    return manager_->stopMotion(ns_direction, ew_direction);
}

auto INDITelescope::guideNS(int direction, int duration) -> bool {
    return manager_->guideNS(direction, duration);
}

auto INDITelescope::guideEW(int direction, int duration) -> bool {
    return manager_->guideEW(direction, duration);
}

auto INDITelescope::guidePulse(double ra_ms, double dec_ms) -> bool {
    return manager_->guidePulse(ra_ms, dec_ms);
}

auto INDITelescope::getRADECJ2000() -> std::optional<EquatorialCoordinates> {
    return manager_->getRADECJ2000();
}

auto INDITelescope::setRADECJ2000(double raHours, double decDegrees) -> bool {
    return manager_->setRADECJ2000(raHours, decDegrees);
}

auto INDITelescope::getRADECJNow() -> std::optional<EquatorialCoordinates> {
    return manager_->getRADECJNow();
}

auto INDITelescope::setRADECJNow(double raHours, double decDegrees) -> bool {
    return manager_->setRADECJNow(raHours, decDegrees);
}

auto INDITelescope::getTargetRADECJNow() -> std::optional<EquatorialCoordinates> {
    return manager_->getTargetRADECJNow();
}

auto INDITelescope::setTargetRADECJNow(double raHours, double decDegrees) -> bool {
    return manager_->setTargetRADECJNow(raHours, decDegrees);
}

auto INDITelescope::slewToRADECJNow(double raHours, double decDegrees, bool enableTracking) -> bool {
    return manager_->slewToRADECJNow(raHours, decDegrees, enableTracking);
}

auto INDITelescope::syncToRADECJNow(double raHours, double decDegrees) -> bool {
    return manager_->syncToRADECJNow(raHours, decDegrees);
}

auto INDITelescope::getAZALT() -> std::optional<HorizontalCoordinates> {
    return manager_->getAZALT();
}

auto INDITelescope::setAZALT(double azDegrees, double altDegrees) -> bool {
    return manager_->setAZALT(azDegrees, altDegrees);
}

auto INDITelescope::slewToAZALT(double azDegrees, double altDegrees) -> bool {
    return manager_->slewToAZALT(azDegrees, altDegrees);
}

auto INDITelescope::getLocation() -> std::optional<GeographicLocation> {
    return manager_->getLocation();
}

auto INDITelescope::setLocation(const GeographicLocation& location) -> bool {
    return manager_->setLocation(location);
}

auto INDITelescope::getUTCTime() -> std::optional<std::chrono::system_clock::time_point> {
    return manager_->getUTCTime();
}

auto INDITelescope::setUTCTime(const std::chrono::system_clock::time_point& time) -> bool {
    return manager_->setUTCTime(time);
}

auto INDITelescope::getLocalTime() -> std::optional<std::chrono::system_clock::time_point> {
    return manager_->getLocalTime();
}

auto INDITelescope::getAlignmentMode() -> AlignmentMode {
    return manager_->getAlignmentMode();
}

auto INDITelescope::setAlignmentMode(AlignmentMode mode) -> bool {
    return manager_->setAlignmentMode(mode);
}

auto INDITelescope::addAlignmentPoint(const EquatorialCoordinates& measured, 
                                      const EquatorialCoordinates& target) -> bool {
    return manager_->addAlignmentPoint(measured, target);
}

auto INDITelescope::clearAlignment() -> bool {
    return manager_->clearAlignment();
}

auto INDITelescope::degreesToDMS(double degrees) -> std::tuple<int, int, double> {
    return manager_->degreesToDMS(degrees);
}

auto INDITelescope::degreesToHMS(double degrees) -> std::tuple<int, int, double> {
    return manager_->degreesToHMS(degrees);
}

void INDITelescope::newMessage(INDI::BaseDevice baseDevice, int messageID) {
    manager_->newMessage(baseDevice, messageID);
}

// Component registration
ATOM_MODULE(telescope_indi, [](Component &component) {
    spdlog::info("Registering INDI telescope component");
    component.def("create_telescope", [](const std::string& name) -> std::shared_ptr<INDITelescope> {
        return std::make_shared<INDITelescope>(name);
    });
    
    component.def("telescope_connect", [](std::shared_ptr<INDITelescope> telescope, 
                                         const std::string& deviceName) -> bool {
        return telescope->connect(deviceName);
    });
    
    component.def("telescope_disconnect", [](std::shared_ptr<INDITelescope> telescope) -> bool {
        return telescope->disconnect();
    });
    
    component.def("telescope_scan", [](std::shared_ptr<INDITelescope> telescope) -> std::vector<std::string> {
        return telescope->scan();
    });
    
    component.def("telescope_is_connected", [](std::shared_ptr<INDITelescope> telescope) -> bool {
        return telescope->isConnected();
    });
    
    spdlog::info("INDI telescope component registered successfully");
});
