/*
 * telescope.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Telescope Implementation

*************************************************/

#include "telescope.hpp"

#include <chrono>
#include <sstream>
#include <iomanip>
#include <cmath>

#ifdef _WIN32
#include <objbase.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <curl/curl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#endif

#include "atom/log/loguru.hpp"

ASCOMTelescope::ASCOMTelescope(std::string name) 
    : AtomTelescope(std::move(name)) {
    LOG_F(INFO, "ASCOMTelescope constructor called with name: {}", getName());
}

ASCOMTelescope::~ASCOMTelescope() {
    LOG_F(INFO, "ASCOMTelescope destructor called");
    disconnect();
    
#ifdef _WIN32
    if (com_telescope_) {
        com_telescope_->Release();
        com_telescope_ = nullptr;
    }
    CoUninitialize();
#endif
}

auto ASCOMTelescope::initialize() -> bool {
    LOG_F(INFO, "Initializing ASCOM Telescope");
    
#ifdef _WIN32
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        LOG_F(ERROR, "Failed to initialize COM: {}", hr);
        return false;
    }
#else
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif
    
    return true;
}

auto ASCOMTelescope::destroy() -> bool {
    LOG_F(INFO, "Destroying ASCOM Telescope");
    
    stopMonitoring();
    disconnect();
    
#ifndef _WIN32
    curl_global_cleanup();
#endif
    
    return true;
}

auto ASCOMTelescope::connect(const std::string &deviceName, int timeout, int maxRetry) -> bool {
    LOG_F(INFO, "Connecting to ASCOM device: {}", deviceName);
    
    device_name_ = deviceName;
    
    // Try to determine if this is a COM ProgID or Alpaca device
    if (deviceName.find("://") != std::string::npos) {
        // Looks like an HTTP URL for Alpaca
        size_t start = deviceName.find("://") + 3;
        size_t colon = deviceName.find(":", start);
        size_t slash = deviceName.find("/", start);
        
        if (colon != std::string::npos) {
            alpaca_host_ = deviceName.substr(start, colon - start);
            if (slash != std::string::npos) {
                alpaca_port_ = std::stoi(deviceName.substr(colon + 1, slash - colon - 1));
            } else {
                alpaca_port_ = std::stoi(deviceName.substr(colon + 1));
            }
        } else {
            alpaca_host_ = deviceName.substr(start, slash != std::string::npos ? slash - start : std::string::npos);
        }
        
        connection_type_ = ConnectionType::ALPACA_REST;
        return connectToAlpacaDevice(alpaca_host_, alpaca_port_, alpaca_device_number_);
    }
    
#ifdef _WIN32
    // Try as COM ProgID
    connection_type_ = ConnectionType::COM_DRIVER;
    return connectToCOMDriver(deviceName);
#else
    LOG_F(ERROR, "COM drivers not supported on non-Windows platforms");
    return false;
#endif
}

auto ASCOMTelescope::disconnect() -> bool {
    LOG_F(INFO, "Disconnecting ASCOM Telescope");
    
    stopMonitoring();
    
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        return disconnectFromAlpacaDevice();
    }
    
#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        return disconnectFromCOMDriver();
    }
#endif
    
    return true;
}

auto ASCOMTelescope::scan() -> std::vector<std::string> {
    LOG_F(INFO, "Scanning for ASCOM devices");
    
    std::vector<std::string> devices;
    
    // Discover Alpaca devices
    auto alpaca_devices = discoverAlpacaDevices();
    devices.insert(devices.end(), alpaca_devices.begin(), alpaca_devices.end());
    
#ifdef _WIN32
    // TODO: Scan Windows registry for ASCOM COM drivers
    // This would involve querying HKEY_LOCAL_MACHINE\\SOFTWARE\\ASCOM\\Telescope Drivers
#endif
    
    return devices;
}

auto ASCOMTelescope::isConnected() const -> bool {
    return is_connected_.load();
}

// Telescope information methods
auto ASCOMTelescope::getTelescopeInfo() -> std::optional<TelescopeParameters> {
    if (!isConnected()) {
        return std::nullopt;
    }
    
    TelescopeParameters params;
    params.aperture = telescope_parameters_.aperture;
    params.focalLength = telescope_parameters_.focalLength;
    params.guiderAperture = telescope_parameters_.guiderAperture;
    params.guiderFocalLength = telescope_parameters_.guiderFocalLength;
    
    return params;
}

auto ASCOMTelescope::setTelescopeInfo(double aperture, double focalLength,
                                      double guiderAperture, double guiderFocalLength) -> bool {
    telescope_parameters_.aperture = aperture;
    telescope_parameters_.focalLength = focalLength;
    telescope_parameters_.guiderAperture = guiderAperture;
    telescope_parameters_.guiderFocalLength = guiderFocalLength;
    
    return true;
}

// Pier side methods
auto ASCOMTelescope::getPierSide() -> std::optional<PierSide> {
    if (!isConnected()) {
        return std::nullopt;
    }
    
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "sideofpier");
        if (response) {
            int side = std::stoi(*response);
            return static_cast<PierSide>(side);
        }
    }
    
#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("SideOfPier");
        if (result) {
            return static_cast<PierSide>(result->intVal);
        }
    }
#endif
    
    return std::nullopt;
}

auto ASCOMTelescope::setPierSide(PierSide side) -> bool {
    if (!isConnected()) {
        return false;
    }
    
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        std::string params = "SideOfPier=" + std::to_string(static_cast<int>(side));
        auto response = sendAlpacaRequest("PUT", "sideofpier", params);
        return response.has_value();
    }
    
#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_I4;
        value.intVal = static_cast<int>(side);
        return setCOMProperty("SideOfPier", value);
    }
#endif
    
    return false;
}

// Tracking methods
auto ASCOMTelescope::getTrackRate() -> std::optional<TrackMode> {
    if (!isConnected()) {
        return std::nullopt;
    }
    
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "trackingrate");
        if (response) {
            int rate = std::stoi(*response);
            return static_cast<TrackMode>(rate);
        }
    }
    
#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("TrackingRate");
        if (result) {
            return static_cast<TrackMode>(result->intVal);
        }
    }
#endif
    
    return std::nullopt;
}

auto ASCOMTelescope::setTrackRate(TrackMode rate) -> bool {
    if (!isConnected()) {
        return false;
    }
    
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        std::string params = "TrackingRate=" + std::to_string(static_cast<int>(rate));
        auto response = sendAlpacaRequest("PUT", "trackingrate", params);
        return response.has_value();
    }
    
#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_I4;
        value.intVal = static_cast<int>(rate);
        return setCOMProperty("TrackingRate", value);
    }
#endif
    
    return false;
}

auto ASCOMTelescope::isTrackingEnabled() -> bool {
    if (!isConnected()) {
        return false;
    }
    
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "tracking");
        if (response) {
            return *response == "true";
        }
    }
    
#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("Tracking");
        if (result) {
            return result->boolVal == VARIANT_TRUE;
        }
    }
#endif
    
    return false;
}

auto ASCOMTelescope::enableTracking(bool enable) -> bool {
    if (!isConnected()) {
        return false;
    }
    
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        std::string params = "Tracking=" + std::string(enable ? "true" : "false");
        auto response = sendAlpacaRequest("PUT", "tracking", params);
        return response.has_value();
    }
    
#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_BOOL;
        value.boolVal = enable ? VARIANT_TRUE : VARIANT_FALSE;
        return setCOMProperty("Tracking", value);
    }
#endif
    
    return false;
}

// Placeholder implementations for remaining pure virtual methods
auto ASCOMTelescope::getTrackRates() -> MotionRates {
    return motion_rates_;
}

auto ASCOMTelescope::setTrackRates(const MotionRates &rates) -> bool {
    motion_rates_ = rates;
    return true;
}

auto ASCOMTelescope::abortMotion() -> bool {
    if (!isConnected()) {
        return false;
    }
    
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("PUT", "abortslew");
        return response.has_value();
    }
    
#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = invokeCOMMethod("AbortSlew");
        return result.has_value();
    }
#endif
    
    return false;
}

auto ASCOMTelescope::getStatus() -> std::optional<std::string> {
    if (!isConnected()) {
        return "Disconnected";
    }
    
    if (is_slewing_.load()) {
        return "Slewing";
    }
    
    if (is_tracking_.load()) {
        return "Tracking";
    }
    
    if (is_parked_.load()) {
        return "Parked";
    }
    
    return "Idle";
}

auto ASCOMTelescope::emergencyStop() -> bool {
    return abortMotion();
}

auto ASCOMTelescope::isMoving() -> bool {
    if (!isConnected()) {
        return false;
    }
    
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "slewing");
        if (response) {
            return *response == "true";
        }
    }
    
#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("Slewing");
        if (result) {
            return result->boolVal == VARIANT_TRUE;
        }
    }
#endif
    
    return false;
}

// Coordinate system methods (placeholder implementations)
auto ASCOMTelescope::getRADECJ2000() -> std::optional<EquatorialCoordinates> {
    return getRADECJNow(); // For now, return JNow coordinates
}

auto ASCOMTelescope::setRADECJ2000(double raHours, double decDegrees) -> bool {
    return setRADECJNow(raHours, decDegrees);
}

auto ASCOMTelescope::getRADECJNow() -> std::optional<EquatorialCoordinates> {
    if (!isConnected()) {
        return std::nullopt;
    }
    
    EquatorialCoordinates coords;
    
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto ra_response = sendAlpacaRequest("GET", "rightascension");
        auto dec_response = sendAlpacaRequest("GET", "declination");
        
        if (ra_response && dec_response) {
            coords.ra = std::stod(*ra_response);
            coords.dec = std::stod(*dec_response);
            return coords;
        }
    }
    
#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto ra_result = getCOMProperty("RightAscension");
        auto dec_result = getCOMProperty("Declination");
        
        if (ra_result && dec_result) {
            coords.ra = ra_result->dblVal;
            coords.dec = dec_result->dblVal;
            return coords;
        }
    }
#endif
    
    return std::nullopt;
}

auto ASCOMTelescope::setRADECJNow(double raHours, double decDegrees) -> bool {
    target_radec_.ra = raHours;
    target_radec_.dec = decDegrees;
    return true;
}

auto ASCOMTelescope::getTargetRADECJNow() -> std::optional<EquatorialCoordinates> {
    return target_radec_;
}

auto ASCOMTelescope::setTargetRADECJNow(double raHours, double decDegrees) -> bool {
    return setRADECJNow(raHours, decDegrees);
}

auto ASCOMTelescope::slewToRADECJNow(double raHours, double decDegrees, bool enableTracking) -> bool {
    if (!isConnected()) {
        return false;
    }
    
    setTargetRADECJNow(raHours, decDegrees);
    
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        std::ostringstream params;
        params << "RightAscension=" << std::fixed << std::setprecision(8) << raHours
               << "&Declination=" << std::fixed << std::setprecision(8) << decDegrees;
        
        auto response = sendAlpacaRequest("PUT", "slewtocoordinatesasync", params.str());
        if (response) {
            is_slewing_.store(true);
            if (enableTracking) {
                this->enableTracking(true);
            }
            return true;
        }
    }
    
#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        VARIANT params[2];
        VariantInit(&params[0]);
        VariantInit(&params[1]);
        params[0].vt = VT_R8;
        params[0].dblVal = raHours;
        params[1].vt = VT_R8;
        params[1].dblVal = decDegrees;
        
        auto result = invokeCOMMethod("SlewToCoordinatesAsync", params, 2);
        if (result) {
            is_slewing_.store(true);
            if (enableTracking) {
                this->enableTracking(true);
            }
            return true;
        }
    }
#endif
    
    return false;
}

auto ASCOMTelescope::syncToRADECJNow(double raHours, double decDegrees) -> bool {
    if (!isConnected()) {
        return false;
    }
    
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        std::ostringstream params;
        params << "RightAscension=" << std::fixed << std::setprecision(8) << raHours
               << "&Declination=" << std::fixed << std::setprecision(8) << decDegrees;
        
        auto response = sendAlpacaRequest("PUT", "synctocoordinates", params.str());
        return response.has_value();
    }
    
#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        VARIANT params[2];
        VariantInit(&params[0]);
        VariantInit(&params[1]);
        params[0].vt = VT_R8;
        params[0].dblVal = raHours;
        params[1].vt = VT_R8;
        params[1].dblVal = decDegrees;
        
        auto result = invokeCOMMethod("SyncToCoordinates", params, 2);
        return result.has_value();
    }
#endif
    
    return false;
}

// Utility methods
auto ASCOMTelescope::degreesToDMS(double degrees) -> std::tuple<int, int, double> {
    bool negative = degrees < 0;
    degrees = std::abs(degrees);
    
    int deg = static_cast<int>(degrees);
    double temp = (degrees - deg) * 60.0;
    int min = static_cast<int>(temp);
    double sec = (temp - min) * 60.0;
    
    if (negative) {
        deg = -deg;
    }
    
    return std::make_tuple(deg, min, sec);
}

auto ASCOMTelescope::degreesToHMS(double degrees) -> std::tuple<int, int, double> {
    double hours = degrees / 15.0;
    int hour = static_cast<int>(hours);
    double temp = (hours - hour) * 60.0;
    int min = static_cast<int>(temp);
    double sec = (temp - min) * 60.0;
    
    return std::make_tuple(hour, min, sec);
}

// Alpaca discovery and connection methods
auto ASCOMTelescope::discoverAlpacaDevices() -> std::vector<std::string> {
    LOG_F(INFO, "Discovering Alpaca devices");
    std::vector<std::string> devices;
    
    // TODO: Implement Alpaca discovery protocol
    // This involves sending UDP broadcasts on port 32227
    // and parsing the JSON responses
    
    // For now, return some common defaults
    devices.push_back("http://localhost:11111/api/v1/telescope/0");
    
    return devices;
}

auto ASCOMTelescope::connectToAlpacaDevice(const std::string &host, int port, int deviceNumber) -> bool {
    LOG_F(INFO, "Connecting to Alpaca device at {}:{} device {}", host, port, deviceNumber);
    
    alpaca_host_ = host;
    alpaca_port_ = port;
    alpaca_device_number_ = deviceNumber;
    
    // Test connection by getting device info
    auto response = sendAlpacaRequest("GET", "connected");
    if (response) {
        is_connected_.store(true);
        updateCapabilities();
        startMonitoring();
        return true;
    }
    
    return false;
}

auto ASCOMTelescope::disconnectFromAlpacaDevice() -> bool {
    LOG_F(INFO, "Disconnecting from Alpaca device");
    
    if (is_connected_.load()) {
        sendAlpacaRequest("PUT", "connected", "Connected=false");
        is_connected_.store(false);
    }
    
    return true;
}

// Helper methods
auto ASCOMTelescope::sendAlpacaRequest(const std::string &method, const std::string &endpoint,
                                      const std::string &params) -> std::optional<std::string> {
    // TODO: Implement HTTP client for Alpaca REST API
    // This would use libcurl or similar HTTP library
    // For now, return placeholder
    
    LOG_F(DEBUG, "Sending Alpaca request: {} {}", method, endpoint);
    return std::nullopt;
}

auto ASCOMTelescope::parseAlpacaResponse(const std::string &response) -> std::optional<std::string> {
    // TODO: Parse JSON response and extract Value field
    return std::nullopt;
}

auto ASCOMTelescope::updateCapabilities() -> bool {
    // Query device capabilities
    return true;
}

auto ASCOMTelescope::startMonitoring() -> void {
    if (!monitor_thread_) {
        stop_monitoring_.store(false);
        monitor_thread_ = std::make_unique<std::thread>(&ASCOMTelescope::monitoringLoop, this);
    }
}

auto ASCOMTelescope::stopMonitoring() -> void {
    if (monitor_thread_) {
        stop_monitoring_.store(true);
        if (monitor_thread_->joinable()) {
            monitor_thread_->join();
        }
        monitor_thread_.reset();
    }
}

auto ASCOMTelescope::monitoringLoop() -> void {
    while (!stop_monitoring_.load()) {
        if (isConnected()) {
            // Update telescope state
            is_slewing_.store(isMoving());
            is_tracking_.store(isTrackingEnabled());
            // Update coordinates
            auto coords = getRADECJNow();
            if (coords) {
                current_radec_ = *coords;
                notifyCoordinateUpdate(current_radec_);
            }
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// Placeholder implementations for remaining methods
auto ASCOMTelescope::setParkOption(ParkOptions option) -> bool { return false; }
auto ASCOMTelescope::getParkPosition() -> std::optional<EquatorialCoordinates> { return std::nullopt; }
auto ASCOMTelescope::setParkPosition(double ra, double dec) -> bool { return false; }
auto ASCOMTelescope::isParked() -> bool { return is_parked_.load(); }
auto ASCOMTelescope::park() -> bool { return false; }
auto ASCOMTelescope::unpark() -> bool { return false; }
auto ASCOMTelescope::canPark() -> bool { return false; }

auto ASCOMTelescope::initializeHome(std::string_view command) -> bool { return false; }
auto ASCOMTelescope::findHome() -> bool { return false; }
auto ASCOMTelescope::setHome() -> bool { return false; }
auto ASCOMTelescope::gotoHome() -> bool { return false; }

auto ASCOMTelescope::getSlewRate() -> std::optional<double> { return std::nullopt; }
auto ASCOMTelescope::setSlewRate(double speed) -> bool { return false; }
auto ASCOMTelescope::getSlewRates() -> std::vector<double> { return {}; }
auto ASCOMTelescope::setSlewRateIndex(int index) -> bool { return false; }

auto ASCOMTelescope::getMoveDirectionEW() -> std::optional<MotionEW> { return std::nullopt; }
auto ASCOMTelescope::setMoveDirectionEW(MotionEW direction) -> bool { return false; }
auto ASCOMTelescope::getMoveDirectionNS() -> std::optional<MotionNS> { return std::nullopt; }
auto ASCOMTelescope::setMoveDirectionNS(MotionNS direction) -> bool { return false; }
auto ASCOMTelescope::startMotion(MotionNS ns_direction, MotionEW ew_direction) -> bool { return false; }
auto ASCOMTelescope::stopMotion(MotionNS ns_direction, MotionEW ew_direction) -> bool { return false; }

auto ASCOMTelescope::guideNS(int direction, int duration) -> bool { return false; }
auto ASCOMTelescope::guideEW(int direction, int duration) -> bool { return false; }
auto ASCOMTelescope::guidePulse(double ra_ms, double dec_ms) -> bool { return false; }

auto ASCOMTelescope::getAZALT() -> std::optional<HorizontalCoordinates> { return std::nullopt; }
auto ASCOMTelescope::setAZALT(double azDegrees, double altDegrees) -> bool { return false; }
auto ASCOMTelescope::slewToAZALT(double azDegrees, double altDegrees) -> bool { return false; }

auto ASCOMTelescope::getLocation() -> std::optional<GeographicLocation> { return std::nullopt; }
auto ASCOMTelescope::setLocation(const GeographicLocation &location) -> bool { return false; }
auto ASCOMTelescope::getUTCTime() -> std::optional<std::chrono::system_clock::time_point> { return std::nullopt; }
auto ASCOMTelescope::setUTCTime(const std::chrono::system_clock::time_point &time) -> bool { return false; }
auto ASCOMTelescope::getLocalTime() -> std::optional<std::chrono::system_clock::time_point> { return std::nullopt; }

auto ASCOMTelescope::getAlignmentMode() -> AlignmentMode { return alignment_mode_; }
auto ASCOMTelescope::setAlignmentMode(AlignmentMode mode) -> bool { alignment_mode_ = mode; return true; }
auto ASCOMTelescope::addAlignmentPoint(const EquatorialCoordinates &measured,
                                      const EquatorialCoordinates &target) -> bool { return false; }
auto ASCOMTelescope::clearAlignment() -> bool { return false; }

// ASCOM-specific method implementations
auto ASCOMTelescope::getASCOMDriverInfo() -> std::optional<std::string> { return driver_info_; }
auto ASCOMTelescope::getASCOMVersion() -> std::optional<std::string> { return driver_version_; }
auto ASCOMTelescope::getASCOMInterfaceVersion() -> std::optional<int> { return interface_version_; }
auto ASCOMTelescope::setASCOMClientID(const std::string &clientId) -> bool { client_id_ = clientId; return true; }
auto ASCOMTelescope::getASCOMClientID() -> std::optional<std::string> { return client_id_; }

// ASCOM capability methods
auto ASCOMTelescope::canPulseGuide() -> bool { return ascom_capabilities_.can_pulse_guide; }
auto ASCOMTelescope::canSetDeclinationRate() -> bool { return ascom_capabilities_.can_set_declination_rate; }
auto ASCOMTelescope::canSetGuideRates() -> bool { return ascom_capabilities_.can_set_guide_rates; }
auto ASCOMTelescope::canSetPark() -> bool { return ascom_capabilities_.can_set_park; }
auto ASCOMTelescope::canSetPierSide() -> bool { return ascom_capabilities_.can_set_pier_side; }
auto ASCOMTelescope::canSetRightAscensionRate() -> bool { return ascom_capabilities_.can_set_right_ascension_rate; }
auto ASCOMTelescope::canSetTracking() -> bool { return ascom_capabilities_.can_set_tracking; }
auto ASCOMTelescope::canSlew() -> bool { return ascom_capabilities_.can_slew; }
auto ASCOMTelescope::canSlewAltAz() -> bool { return ascom_capabilities_.can_slew_alt_az; }
auto ASCOMTelescope::canSlewAltAzAsync() -> bool { return ascom_capabilities_.can_slew_alt_az_async; }
auto ASCOMTelescope::canSlewAsync() -> bool { return ascom_capabilities_.can_slew_async; }
auto ASCOMTelescope::canSync() -> bool { return ascom_capabilities_.can_sync; }
auto ASCOMTelescope::canSyncAltAz() -> bool { return ascom_capabilities_.can_sync_alt_az; }
auto ASCOMTelescope::canUnpark() -> bool { return ascom_capabilities_.can_unpark; }

// Rate methods (placeholder implementations)
auto ASCOMTelescope::getDeclinationRate() -> std::optional<double> { return std::nullopt; }
auto ASCOMTelescope::setDeclinationRate(double rate) -> bool { return false; }
auto ASCOMTelescope::getRightAscensionRate() -> std::optional<double> { return std::nullopt; }
auto ASCOMTelescope::setRightAscensionRate(double rate) -> bool { return false; }
auto ASCOMTelescope::getGuideRateDeclinationRate() -> std::optional<double> { return std::nullopt; }
auto ASCOMTelescope::setGuideRateDeclinationRate(double rate) -> bool { return false; }
auto ASCOMTelescope::getGuideRateRightAscensionRate() -> std::optional<double> { return std::nullopt; }
auto ASCOMTelescope::setGuideRateRightAscensionRate(double rate) -> bool { return false; }

#ifdef _WIN32
// COM-specific methods
auto ASCOMTelescope::connectToCOMDriver(const std::string &progId) -> bool {
    LOG_F(INFO, "Connecting to COM driver: {}", progId);
    
    com_prog_id_ = progId;
    
    CLSID clsid;
    HRESULT hr = CLSIDFromProgID(CComBSTR(progId.c_str()), &clsid);
    if (FAILED(hr)) {
        LOG_F(ERROR, "Failed to get CLSID from ProgID: {}", hr);
        return false;
    }
    
    hr = CoCreateInstance(clsid, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
                         IID_IDispatch, reinterpret_cast<void**>(&com_telescope_));
    if (FAILED(hr)) {
        LOG_F(ERROR, "Failed to create COM instance: {}", hr);
        return false;
    }
    
    // Set Connected = true
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_BOOL;
    value.boolVal = VARIANT_TRUE;
    
    if (setCOMProperty("Connected", value)) {
        is_connected_.store(true);
        updateCapabilities();
        startMonitoring();
        return true;
    }
    
    return false;
}

auto ASCOMTelescope::disconnectFromCOMDriver() -> bool {
    LOG_F(INFO, "Disconnecting from COM driver");
    
    if (com_telescope_) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_BOOL;
        value.boolVal = VARIANT_FALSE;
        setCOMProperty("Connected", value);
        
        com_telescope_->Release();
        com_telescope_ = nullptr;
    }
    
    is_connected_.store(false);
    return true;
}

auto ASCOMTelescope::showASCOMChooser() -> std::optional<std::string> {
    // TODO: Implement ASCOM chooser dialog
    return std::nullopt;
}

auto ASCOMTelescope::invokeCOMMethod(const std::string &method, VARIANT* params, int param_count) -> std::optional<VARIANT> {
    if (!com_telescope_) {
        return std::nullopt;
    }
    
    DISPID dispid;
    CComBSTR method_name(method.c_str());
    HRESULT hr = com_telescope_->GetIDsOfNames(IID_NULL, &method_name, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        LOG_F(ERROR, "Failed to get method ID for {}: {}", method, hr);
        return std::nullopt;
    }
    
    DISPPARAMS dispparams = { params, nullptr, param_count, 0 };
    VARIANT result;
    VariantInit(&result);
    
    hr = com_telescope_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_METHOD,
                               &dispparams, &result, nullptr, nullptr);
    if (FAILED(hr)) {
        LOG_F(ERROR, "Failed to invoke method {}: {}", method, hr);
        return std::nullopt;
    }
    
    return result;
}

auto ASCOMTelescope::getCOMProperty(const std::string &property) -> std::optional<VARIANT> {
    if (!com_telescope_) {
        return std::nullopt;
    }
    
    DISPID dispid;
    CComBSTR property_name(property.c_str());
    HRESULT hr = com_telescope_->GetIDsOfNames(IID_NULL, &property_name, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        LOG_F(ERROR, "Failed to get property ID for {}: {}", property, hr);
        return std::nullopt;
    }
    
    DISPPARAMS dispparams = { nullptr, nullptr, 0, 0 };
    VARIANT result;
    VariantInit(&result);
    
    hr = com_telescope_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYGET,
                               &dispparams, &result, nullptr, nullptr);
    if (FAILED(hr)) {
        LOG_F(ERROR, "Failed to get property {}: {}", property, hr);
        return std::nullopt;
    }
    
    return result;
}

auto ASCOMTelescope::setCOMProperty(const std::string &property, const VARIANT &value) -> bool {
    if (!com_telescope_) {
        return false;
    }
    
    DISPID dispid;
    CComBSTR property_name(property.c_str());
    HRESULT hr = com_telescope_->GetIDsOfNames(IID_NULL, &property_name, 1, LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        LOG_F(ERROR, "Failed to get property ID for {}: {}", property, hr);
        return false;
    }
    
    VARIANT params[] = { value };
    DISPID dispid_put = DISPID_PROPERTYPUT;
    DISPPARAMS dispparams = { params, &dispid_put, 1, 1 };
    
    hr = com_telescope_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT, DISPATCH_PROPERTYPUT,
                               &dispparams, nullptr, nullptr, nullptr);
    if (FAILED(hr)) {
        LOG_F(ERROR, "Failed to set property {}: {}", property, hr);
        return false;
    }
    
    return true;
}
#endif
