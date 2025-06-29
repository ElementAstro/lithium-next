/*
 * dome.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: ASCOM Dome Implementation

*************************************************/

#include "dome.hpp"

#include <chrono>
#include <cmath>

#ifdef _WIN32
#include <objbase.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <curl/curl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include <spdlog/spdlog.h>

ASCOMDome::ASCOMDome(std::string name) : AtomDome(std::move(name)) {
    spdlog::info("ASCOMDome constructor called with name: {}", getName());
}

ASCOMDome::~ASCOMDome() {
    spdlog::info("ASCOMDome destructor called");
    disconnect();

#ifdef _WIN32
    if (com_dome_) {
        com_dome_->Release();
        com_dome_ = nullptr;
    }
    CoUninitialize();
#endif
}

auto ASCOMDome::initialize() -> bool {
    spdlog::info("Initializing ASCOM Dome");

#ifdef _WIN32
    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        spdlog::error("Failed to initialize COM: {}", hr);
        return false;
    }
#else
    curl_global_init(CURL_GLOBAL_DEFAULT);
#endif

    return true;
}

auto ASCOMDome::destroy() -> bool {
    spdlog::info("Destroying ASCOM Dome");

    stopMonitoring();
    disconnect();

#ifndef _WIN32
    curl_global_cleanup();
#endif

    return true;
}

auto ASCOMDome::connect(const std::string &deviceName, int timeout,
                        int maxRetry) -> bool {
    spdlog::info("Connecting to ASCOM dome device: {}", deviceName);

    device_name_ = deviceName;

    // Determine connection type
    if (deviceName.find("://") != std::string::npos) {
        // Alpaca REST API
        size_t start = deviceName.find("://") + 3;
        size_t colon = deviceName.find(":", start);
        size_t slash = deviceName.find("/", start);

        if (colon != std::string::npos) {
            alpaca_host_ = deviceName.substr(start, colon - start);
            if (slash != std::string::npos) {
                alpaca_port_ =
                    std::stoi(deviceName.substr(colon + 1, slash - colon - 1));
            } else {
                alpaca_port_ = std::stoi(deviceName.substr(colon + 1));
            }
        }

        connection_type_ = ConnectionType::ALPACA_REST;
        return connectToAlpacaDevice(alpaca_host_, alpaca_port_,
                                     alpaca_device_number_);
    }

#ifdef _WIN32
    // Try as COM ProgID
    connection_type_ = ConnectionType::COM_DRIVER;
    return connectToCOMDriver(deviceName);
#else
    spdlog::error("COM drivers not supported on non-Windows platforms");
    return false;
#endif
}

auto ASCOMDome::disconnect() -> bool {
    spdlog::info("Disconnecting ASCOM Dome");

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

auto ASCOMDome::scan() -> std::vector<std::string> {
    spdlog::info("Scanning for ASCOM dome devices");

    std::vector<std::string> devices;

    // Discover Alpaca devices
    auto alpaca_devices = discoverAlpacaDevices();
    devices.insert(devices.end(), alpaca_devices.begin(), alpaca_devices.end());

    return devices;
}

auto ASCOMDome::isConnected() const -> bool { return is_connected_.load(); }

auto ASCOMDome::isMoving() const -> bool { return is_moving_.load(); }

auto ASCOMDome::isParked() const -> bool { return is_parked_.load(); }

// Azimuth control methods
auto ASCOMDome::getAzimuth() -> std::optional<double> {
    if (!isConnected()) {
        return std::nullopt;
    }

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "azimuth");
        if (response) {
            return std::stod(*response);
        }
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("Azimuth");
        if (result) {
            return result->dblVal;
        }
    }
#endif

    return std::nullopt;
}

auto ASCOMDome::setAzimuth(double azimuth) -> bool {
    return moveToAzimuth(azimuth);
}

auto ASCOMDome::moveToAzimuth(double azimuth) -> bool {
    if (!isConnected() || is_moving_.load()) {
        return false;
    }

    // Normalize azimuth to 0-360 range
    while (azimuth < 0.0)
        azimuth += 360.0;
    while (azimuth >= 360.0)
        azimuth -= 360.0;

    spdlog::info("Moving dome to azimuth: {:.2f}°", azimuth);

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        std::string params = "Azimuth=" + std::to_string(azimuth);
        auto response = sendAlpacaRequest("PUT", "slewtoazimuth", params);
        if (response) {
            is_moving_.store(true);
            current_azimuth_.store(azimuth);
            return true;
        }
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        VARIANT param;
        VariantInit(&param);
        param.vt = VT_R8;
        param.dblVal = azimuth;

        auto result = invokeCOMMethod("SlewToAzimuth", &param, 1);
        if (result) {
            is_moving_.store(true);
            current_azimuth_.store(azimuth);
            return true;
        }
    }
#endif

    return false;
}

auto ASCOMDome::rotateClockwise() -> bool {
    if (!isConnected() || is_moving_.load()) {
        return false;
    }

    spdlog::info("Rotating dome clockwise");

    // Get current azimuth and move 10 degrees clockwise
    auto currentAz = getAzimuth();
    if (currentAz) {
        double newAz = *currentAz + 10.0;
        return moveToAzimuth(newAz);
    }

    return false;
}

auto ASCOMDome::rotateCounterClockwise() -> bool {
    if (!isConnected() || is_moving_.load()) {
        return false;
    }

    spdlog::info("Rotating dome counter-clockwise");

    // Get current azimuth and move 10 degrees counter-clockwise
    auto currentAz = getAzimuth();
    if (currentAz) {
        double newAz = *currentAz - 10.0;
        return moveToAzimuth(newAz);
    }

    return false;
}

auto ASCOMDome::stopRotation() -> bool { return abortMotion(); }

auto ASCOMDome::abortMotion() -> bool {
    if (!isConnected()) {
        return false;
    }

    spdlog::info("Aborting dome motion");

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("PUT", "abortslew");
        if (response) {
            is_moving_.store(false);
            return true;
        }
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = invokeCOMMethod("AbortSlew");
        if (result) {
            is_moving_.store(false);
            return true;
        }
    }
#endif

    return false;
}

auto ASCOMDome::syncAzimuth(double azimuth) -> bool {
    if (!isConnected()) {
        return false;
    }

    spdlog::info("Syncing dome azimuth to: {:.2f}°", azimuth);

    // ASCOM domes typically don't support sync
    // Just update our internal state
    current_azimuth_.store(azimuth);
    return true;
}

// Parking methods
auto ASCOMDome::park() -> bool {
    if (!isConnected() || is_parked_.load()) {
        return false;
    }

    spdlog::info("Parking dome");

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("PUT", "park");
        if (response) {
            is_moving_.store(true);
            return true;
        }
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = invokeCOMMethod("Park");
        if (result) {
            is_moving_.store(true);
            return true;
        }
    }
#endif

    return false;
}

auto ASCOMDome::unpark() -> bool {
    if (!isConnected() || !is_parked_.load()) {
        return false;
    }

    spdlog::info("Unparking dome");

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("PUT", "unpark");
        if (response) {
            is_parked_.store(false);
            return true;
        }
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = invokeCOMMethod("Unpark");
        if (result) {
            is_parked_.store(false);
            return true;
        }
    }
#endif

    return false;
}

auto ASCOMDome::getParkPosition() -> std::optional<double> {
    // ASCOM domes typically have a fixed park position
    return 0.0;  // North
}

auto ASCOMDome::setParkPosition(double azimuth) -> bool {
    // Most ASCOM domes don't allow setting park position
    spdlog::info("Set park position to: {:.2f}° (may not be supported)",
                 azimuth);
    return false;
}

auto ASCOMDome::canPark() -> bool { return ascom_capabilities_.can_park; }

// Shutter control methods
auto ASCOMDome::openShutter() -> bool {
    if (!isConnected()) {
        return false;
    }

    spdlog::info("Opening dome shutter");

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("PUT", "openshutter");
        if (response) {
            return true;
        }
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = invokeCOMMethod("OpenShutter");
        if (result) {
            return true;
        }
    }
#endif

    return false;
}

auto ASCOMDome::closeShutter() -> bool {
    if (!isConnected()) {
        return false;
    }

    spdlog::info("Closing dome shutter");

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("PUT", "closeshutter");
        if (response) {
            return true;
        }
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = invokeCOMMethod("CloseShutter");
        if (result) {
            return true;
        }
    }
#endif

    return false;
}

auto ASCOMDome::abortShutter() -> bool {
    if (!isConnected()) {
        return false;
    }

    spdlog::info("Aborting shutter motion");

    // Most ASCOM domes don't support abort shutter
    return false;
}

auto ASCOMDome::getShutterState() -> ShutterState {
    if (!isConnected()) {
        return ShutterState::UNKNOWN;
    }

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "shutterstatus");
        if (response) {
            int status = std::stoi(*response);
            switch (status) {
                case 0:
                    return ShutterState::OPEN;
                case 1:
                    return ShutterState::CLOSED;
                case 2:
                    return ShutterState::OPENING;
                case 3:
                    return ShutterState::CLOSING;
                default:
                    return ShutterState::ERROR;
            }
        }
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("ShutterStatus");
        if (result) {
            int status = result->intVal;
            switch (status) {
                case 0:
                    return ShutterState::OPEN;
                case 1:
                    return ShutterState::CLOSED;
                case 2:
                    return ShutterState::OPENING;
                case 3:
                    return ShutterState::CLOSING;
                default:
                    return ShutterState::ERROR;
            }
        }
    }
#endif

    return ShutterState::UNKNOWN;
}

auto ASCOMDome::hasShutter() -> bool {
    return ascom_capabilities_.can_set_shutter;
}

// Speed control methods
auto ASCOMDome::getRotationSpeed() -> std::optional<double> {
    // ASCOM domes typically don't expose speed control
    return std::nullopt;
}

auto ASCOMDome::setRotationSpeed(double speed) -> bool {
    // ASCOM domes typically don't support speed control
    spdlog::info("Set rotation speed to: {:.2f} (may not be supported)", speed);
    return false;
}

auto ASCOMDome::getMaxSpeed() -> double {
    return 1.0;  // Arbitrary unit
}

auto ASCOMDome::getMinSpeed() -> double {
    return 0.1;  // Arbitrary unit
}

// Telescope coordination methods
auto ASCOMDome::followTelescope(bool enable) -> bool {
    if (!isConnected()) {
        return false;
    }

    is_slaved_.store(enable);
    spdlog::info("{} telescope following", enable ? "Enabling" : "Disabling");

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        std::string params = "Slaved=" + std::string(enable ? "true" : "false");
        auto response = sendAlpacaRequest("PUT", "slaved", params);
        return response.has_value();
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_BOOL;
        value.boolVal = enable ? VARIANT_TRUE : VARIANT_FALSE;
        return setCOMProperty("Slaved", value);
    }
#endif

    return false;
}

auto ASCOMDome::isFollowingTelescope() -> bool { return is_slaved_.load(); }

auto ASCOMDome::calculateDomeAzimuth(double telescopeAz, double telescopeAlt)
    -> double {
    // Simple calculation - in practice this would be more complex
    // accounting for telescope offset from dome center
    return telescopeAz;
}

auto ASCOMDome::setTelescopePosition(double az, double alt) -> bool {
    if (!isConnected() || !is_slaved_.load()) {
        return false;
    }

    // Calculate required dome azimuth
    double domeAz = calculateDomeAzimuth(az, alt);

    // Move dome if necessary
    auto currentAz = getAzimuth();
    if (currentAz && std::abs(*currentAz - domeAz) > 1.0) {
        return moveToAzimuth(domeAz);
    }

    return true;
}

// Home position methods
auto ASCOMDome::findHome() -> bool {
    if (!isConnected()) {
        return false;
    }

    spdlog::info("Finding dome home position");

    if (connection_type_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("PUT", "findhome");
        if (response) {
            is_moving_.store(true);
            return true;
        }
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto result = invokeCOMMethod("FindHome");
        if (result) {
            is_moving_.store(true);
            return true;
        }
    }
#endif

    return false;
}

auto ASCOMDome::setHome() -> bool {
    if (!isConnected()) {
        return false;
    }

    spdlog::info("Setting current position as home");

    // ASCOM domes typically don't support setting home
    return false;
}

auto ASCOMDome::gotoHome() -> bool {
    auto homePos = getHomePosition();
    if (homePos) {
        return moveToAzimuth(*homePos);
    }
    return false;
}

auto ASCOMDome::getHomePosition() -> std::optional<double> {
    // ASCOM domes typically have a fixed home position
    return 0.0;  // North
}

// Additional stub implementations for the remaining virtual methods...
auto ASCOMDome::getBacklash() -> double { return 0.0; }
auto ASCOMDome::setBacklash(double backlash) -> bool { return false; }
auto ASCOMDome::enableBacklashCompensation(bool enable) -> bool {
    return false;
}
auto ASCOMDome::isBacklashCompensationEnabled() -> bool { return false; }
auto ASCOMDome::canOpenShutter() -> bool { return true; }
auto ASCOMDome::isSafeToOperate() -> bool { return true; }
auto ASCOMDome::getWeatherStatus() -> std::string { return "Unknown"; }
auto ASCOMDome::getTotalRotation() -> double { return 0.0; }
auto ASCOMDome::resetTotalRotation() -> bool { return false; }
auto ASCOMDome::getShutterOperations() -> uint64_t { return 0; }
auto ASCOMDome::resetShutterOperations() -> bool { return false; }
auto ASCOMDome::savePreset(int slot, double azimuth) -> bool { return false; }
auto ASCOMDome::loadPreset(int slot) -> bool { return false; }
auto ASCOMDome::getPreset(int slot) -> std::optional<double> {
    return std::nullopt;
}
auto ASCOMDome::deletePreset(int slot) -> bool { return false; }

// Alpaca discovery and connection methods
auto ASCOMDome::discoverAlpacaDevices() -> std::vector<std::string> {
    spdlog::info("Discovering Alpaca dome devices");
    std::vector<std::string> devices;

    // TODO: Implement Alpaca discovery protocol
    devices.push_back("http://localhost:11111/api/v1/dome/0");

    return devices;
}

auto ASCOMDome::connectToAlpacaDevice(const std::string &host, int port,
                                      int deviceNumber) -> bool {
    spdlog::info("Connecting to Alpaca dome device at {}:{} device {}", host,
                 port, deviceNumber);

    alpaca_host_ = host;
    alpaca_port_ = port;
    alpaca_device_number_ = deviceNumber;

    // Test connection
    auto response = sendAlpacaRequest("GET", "connected");
    if (response) {
        is_connected_.store(true);
        updateDomeCapabilities();
        startMonitoring();
        return true;
    }

    return false;
}

auto ASCOMDome::disconnectFromAlpacaDevice() -> bool {
    spdlog::info("Disconnecting from Alpaca dome device");

    if (is_connected_.load()) {
        sendAlpacaRequest("PUT", "connected", "Connected=false");
        is_connected_.store(false);
    }

    return true;
}

// Helper methods
auto ASCOMDome::sendAlpacaRequest(const std::string &method,
                                  const std::string &endpoint,
                                  const std::string &params)
    -> std::optional<std::string> {
    // TODO: Implement HTTP client for Alpaca REST API
    spdlog::debug("Sending Alpaca request: {} {}", method, endpoint);
    return std::nullopt;
}

auto ASCOMDome::parseAlpacaResponse(const std::string &response)
    -> std::optional<std::string> {
    // TODO: Parse JSON response
    return std::nullopt;
}

auto ASCOMDome::updateDomeCapabilities() -> bool {
    if (!isConnected()) {
        return false;
    }

    // Get dome capabilities
    if (connection_type_ == ConnectionType::ALPACA_REST) {
        // TODO: Query actual capabilities
        ascom_capabilities_.can_find_home = true;
        ascom_capabilities_.can_park = true;
        ascom_capabilities_.can_set_azimuth = true;
        ascom_capabilities_.can_set_shutter = true;
        ascom_capabilities_.can_slave = true;
        ascom_capabilities_.can_sync_azimuth = false;
    }

#ifdef _WIN32
    if (connection_type_ == ConnectionType::COM_DRIVER) {
        auto canFindHome = getCOMProperty("CanFindHome");
        auto canPark = getCOMProperty("CanPark");
        auto canSetAzimuth = getCOMProperty("CanSetAzimuth");
        auto canSetShutter = getCOMProperty("CanSetShutter");
        auto canSlave = getCOMProperty("CanSlave");
        auto canSyncAzimuth = getCOMProperty("CanSyncAzimuth");

        if (canFindHome)
            ascom_capabilities_.can_find_home =
                (canFindHome->boolVal == VARIANT_TRUE);
        if (canPark)
            ascom_capabilities_.can_park = (canPark->boolVal == VARIANT_TRUE);
        if (canSetAzimuth)
            ascom_capabilities_.can_set_azimuth =
                (canSetAzimuth->boolVal == VARIANT_TRUE);
        if (canSetShutter)
            ascom_capabilities_.can_set_shutter =
                (canSetShutter->boolVal == VARIANT_TRUE);
        if (canSlave)
            ascom_capabilities_.can_slave = (canSlave->boolVal == VARIANT_TRUE);
        if (canSyncAzimuth)
            ascom_capabilities_.can_sync_azimuth =
                (canSyncAzimuth->boolVal == VARIANT_TRUE);
    }
#endif

    return true;
}

auto ASCOMDome::startMonitoring() -> void {
    if (!monitor_thread_) {
        stop_monitoring_.store(false);
        monitor_thread_ =
            std::make_unique<std::thread>(&ASCOMDome::monitoringLoop, this);
    }
}

auto ASCOMDome::stopMonitoring() -> void {
    if (monitor_thread_) {
        stop_monitoring_.store(true);
        if (monitor_thread_->joinable()) {
            monitor_thread_->join();
        }
        monitor_thread_.reset();
    }
}

auto ASCOMDome::monitoringLoop() -> void {
    while (!stop_monitoring_.load()) {
        if (isConnected()) {
            // Update dome state
            auto azimuth = getAzimuth();
            if (azimuth) {
                current_azimuth_.store(*azimuth);
            }

            // Check movement status
            if (is_moving_.load()) {
                bool moving = false;

                if (connection_type_ == ConnectionType::ALPACA_REST) {
                    auto response = sendAlpacaRequest("GET", "slewing");
                    if (response && *response == "false") {
                        moving = false;
                    }
                }

#ifdef _WIN32
                if (connection_type_ == ConnectionType::COM_DRIVER) {
                    auto result = getCOMProperty("Slewing");
                    if (result && result->boolVal == VARIANT_FALSE) {
                        moving = false;
                    }
                }
#endif

                if (!moving) {
                    is_moving_.store(false);
                    notifyMoveComplete(true, "Dome movement completed");
                }
            }

            // Check park status
            if (connection_type_ == ConnectionType::ALPACA_REST) {
                auto response = sendAlpacaRequest("GET", "athome");
                if (response) {
                    bool atHome = (*response == "true");
                    is_parked_.store(atHome);
                }
            }

#ifdef _WIN32
            if (connection_type_ == ConnectionType::COM_DRIVER) {
                auto result = getCOMProperty("AtHome");
                if (result) {
                    is_parked_.store(result->boolVal == VARIANT_TRUE);
                }
            }
#endif
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

#ifdef _WIN32
auto ASCOMDome::connectToCOMDriver(const std::string &progId) -> bool {
    spdlog::info("Connecting to COM dome driver: {}", progId);

    com_prog_id_ = progId;

    CLSID clsid;
    HRESULT hr = CLSIDFromProgID(CComBSTR(progId.c_str()), &clsid);
    if (FAILED(hr)) {
        spdlog::error("Failed to get CLSID from ProgID: {}", hr);
        return false;
    }

    hr = CoCreateInstance(clsid, nullptr,
                          CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
                          IID_IDispatch, reinterpret_cast<void **>(&com_dome_));
    if (FAILED(hr)) {
        spdlog::error("Failed to create COM instance: {}", hr);
        return false;
    }

    // Set Connected = true
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_BOOL;
    value.boolVal = VARIANT_TRUE;

    if (setCOMProperty("Connected", value)) {
        is_connected_.store(true);
        updateDomeCapabilities();
        startMonitoring();
        return true;
    }

    return false;
}

auto ASCOMDome::disconnectFromCOMDriver() -> bool {
    spdlog::info("Disconnecting from COM dome driver");

    if (com_dome_) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_BOOL;
        value.boolVal = VARIANT_FALSE;
        setCOMProperty("Connected", value);

        com_dome_->Release();
        com_dome_ = nullptr;
    }

    is_connected_.store(false);
    return true;
}

// COM helper methods (similar to other implementations)
auto ASCOMDome::invokeCOMMethod(const std::string &method, VARIANT *params,
                                int param_count) -> std::optional<VARIANT> {
    if (!com_dome_) {
        return std::nullopt;
    }

    DISPID dispid;
    CComBSTR method_name(method.c_str());
    HRESULT hr = com_dome_->GetIDsOfNames(IID_NULL, &method_name, 1,
                                          LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        spdlog::error("Failed to get method ID for {}: {}", method, hr);
        return std::nullopt;
    }

    DISPPARAMS dispparams = {params, nullptr, param_count, 0};
    VARIANT result;
    VariantInit(&result);

    hr = com_dome_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                           DISPATCH_METHOD, &dispparams, &result, nullptr,
                           nullptr);
    if (FAILED(hr)) {
        spdlog::error("Failed to invoke method {}: {}", method, hr);
        return std::nullopt;
    }

    return result;
}

auto ASCOMDome::getCOMProperty(const std::string &property)
    -> std::optional<VARIANT> {
    if (!com_dome_) {
        return std::nullopt;
    }

    DISPID dispid;
    CComBSTR property_name(property.c_str());
    HRESULT hr = com_dome_->GetIDsOfNames(IID_NULL, &property_name, 1,
                                          LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        spdlog::error("Failed to get property ID for {}: {}", property, hr);
        return std::nullopt;
    }

    DISPPARAMS dispparams = {nullptr, nullptr, 0, 0};
    VARIANT result;
    VariantInit(&result);

    hr = com_dome_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                           DISPATCH_PROPERTYGET, &dispparams, &result, nullptr,
                           nullptr);
    if (FAILED(hr)) {
        spdlog::error("Failed to get property {}: {}", property, hr);
        return std::nullopt;
    }

    return result;
}

auto ASCOMDome::setCOMProperty(const std::string &property,
                               const VARIANT &value) -> bool {
    if (!com_dome_) {
        return false;
    }

    DISPID dispid;
    CComBSTR property_name(property.c_str());
    HRESULT hr = com_dome_->GetIDsOfNames(IID_NULL, &property_name, 1,
                                          LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        spdlog::error("Failed to get property ID for {}: {}", property, hr);
        return false;
    }

    VARIANT params[] = {value};
    DISPID dispid_put = DISPID_PROPERTYPUT;
    DISPPARAMS dispparams = {params, &dispid_put, 1, 1};

    hr = com_dome_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                           DISPATCH_PROPERTYPUT, &dispparams, nullptr, nullptr,
                           nullptr);
    if (FAILED(hr)) {
        spdlog::error("Failed to set property {}: {}", property, hr);
        return false;
    }

    return true;
}

auto ASCOMDome::showASCOMChooser() -> std::optional<std::string> {
    // TODO: Implement ASCOM Chooser dialog
    return std::nullopt;
}
#endif
