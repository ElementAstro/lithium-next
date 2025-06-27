/*
 * hardware_interface.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-24

Description: ASCOM Camera Hardware Interface Component Implementation

This component provides a clean interface to ASCOM Camera APIs,
handling low-level hardware communication, device management,
and both COM and Alpaca protocol integration.

*************************************************/

#include "hardware_interface.hpp"

#include <chrono>
#include <cmath>
#include <iomanip>
#include <sstream>

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

namespace lithium::device::ascom::camera::components {

HardwareInterface::HardwareInterface() {
    spdlog::info("ASCOM Hardware Interface created");
}

HardwareInterface::~HardwareInterface() {
    spdlog::info("ASCOM Hardware Interface destructor called");
    shutdown();
}

auto HardwareInterface::initialize() -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (initialized_) {
        return true;
    }

    spdlog::info("Initializing ASCOM Hardware Interface");

#ifdef _WIN32
    // Initialize COM for Windows
    if (!initializeCOM()) {
        setLastError("Failed to initialize COM subsystem");
        return false;
    }
#else
    // Initialize curl for HTTP requests (Alpaca)
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK) {
        setError("Failed to initialize HTTP client");
        return false;
    }
#endif

    initialized_ = true;
    spdlog::info("ASCOM Hardware Interface initialized successfully");
    return true;
}

auto HardwareInterface::shutdown() -> bool {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!initialized_) {
        return true;
    }

    spdlog::info("Shutting down ASCOM Hardware Interface");

    // Disconnect if connected
    if (connected_) {
        disconnect();
    }

#ifdef _WIN32
    shutdownCOM();
#else
    curl_global_cleanup();
#endif

    initialized_ = false;
    spdlog::info("ASCOM Hardware Interface shutdown complete");
    return true;
}

auto HardwareInterface::enumerateDevices() -> std::vector<std::string> {
    std::vector<std::string> devices;

    // Discover Alpaca devices
    auto alpacaDevices = discoverAlpacaDevices();
    devices.insert(devices.end(), alpacaDevices.begin(), alpacaDevices.end());

#ifdef _WIN32
    // TODO: Scan Windows registry for ASCOM COM drivers
    // This would involve querying HKEY_LOCAL_MACHINE\\SOFTWARE\\ASCOM\\Camera Drivers
    spdlog::debug("Windows COM driver enumeration not yet implemented");
#endif

    spdlog::info("Enumerated {} ASCOM devices", devices.size());
    return devices;
}

auto HardwareInterface::discoverAlpacaDevices() -> std::vector<std::string> {
    std::vector<std::string> devices;
    
    spdlog::info("Discovering Alpaca camera devices");
    
    // TODO: Implement Alpaca discovery protocol
    // This involves sending UDP broadcasts on port 32227
    // and parsing the JSON responses
    
    // For now, return some common defaults
    devices.push_back("http://localhost:11111/api/v1/camera/0");
    
    spdlog::debug("Found {} Alpaca devices", devices.size());
    return devices;
}

auto HardwareInterface::connect(const ConnectionSettings& settings) -> bool {
    if (!initialized_) {
        setError("Hardware interface not initialized");
        return false;
    }

    if (connected_) {
        setLastError("Already connected to a device");
        return false;
    }

    currentSettings_ = settings;

    spdlog::info("Connecting to ASCOM camera: {}", settings.deviceName);

    // Determine connection type based on device name
    if (settings.deviceName.find("://") != std::string::npos) {
        // Looks like an HTTP URL for Alpaca
        connectionType_ = ConnectionType::ALPACA_REST;
        return connectAlpaca(settings);
    }

#ifdef _WIN32
    // Try as COM ProgID
    connectionType_ = ConnectionType::COM_DRIVER;
    return connectToCOMDriver(settings.progID);
#else
    setError("COM drivers not supported on non-Windows platforms");
    return false;
#endif
}

auto HardwareInterface::disconnect() -> bool {
    if (!connected_) {
        return true;
    }

    spdlog::info("Disconnecting from ASCOM camera");

    bool success = false;
    if (connectionType_ == ConnectionType::ALPACA_REST) {
        success = disconnectAlpaca();
    }
#ifdef _WIN32
    else if (connectionType_ == ConnectionType::COM_DRIVER) {
        success = disconnectFromCOMDriver();
    }
#endif

    if (success) {
        connected_ = false;
        connectionType_ = ConnectionType::COM_DRIVER; // Reset to default
        cameraInfo_.reset();
    }

    return success;
}

auto HardwareInterface::getCameraInfo() -> std::optional<CameraInfo> {
    std::lock_guard<std::mutex> lock(infoMutex_);
    
    if (!connected_) {
        return std::nullopt;
    }

    // Update camera info if needed
    if (!cameraInfo_.has_value()) {
        updateCameraInfo();
    }

    return cameraInfo_;
}

auto HardwareInterface::getCameraState() -> ASCOMCameraState {
    if (!connected_) {
        return ASCOMCameraState::ERROR;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "camerastate");
        if (response) {
            // Parse the camera state from JSON response
            // TODO: Implement JSON parsing
            return ASCOMCameraState::IDLE;
        }
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("CameraState");
        if (result) {
            return static_cast<ASCOMCameraState>(result->intVal);
        }
    }
#endif

    return ASCOMCameraState::ERROR;
}

auto HardwareInterface::startExposure(double duration, bool isLight) -> bool {
    if (!connected_) {
        setLastError("Not connected to camera");
        return false;
    }

    spdlog::info("Starting exposure: {} seconds, isLight: {}", duration, isLight);

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        std::ostringstream params;
        params << "Duration=" << std::fixed << std::setprecision(3) << duration
               << "&Light=" << (isLight ? "true" : "false");

        auto response = sendAlpacaRequest("PUT", "startexposure", params.str());
        return response.has_value();
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        VARIANT params[2];
        VariantInit(&params[0]);
        VariantInit(&params[1]);
        params[0].vt = VT_R8;
        params[0].dblVal = duration;
        params[1].vt = VT_BOOL;
        params[1].boolVal = isLight ? VARIANT_TRUE : VARIANT_FALSE;

        auto result = invokeCOMMethod("StartExposure", params, 2);
        return result.has_value();
    }
#endif

    return false;
}

auto HardwareInterface::stopExposure() -> bool {
    if (!connected_) {
        setLastError("Not connected to camera");
        return false;
    }

    spdlog::info("Stopping exposure");

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("PUT", "abortexposure");
        return response.has_value();
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        auto result = invokeCOMMethod("AbortExposure");
        return result.has_value();
    }
#endif

    return false;
}

auto HardwareInterface::isExposureComplete() -> bool {
    if (!connected_) {
        return false;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "exposurecomplete");
        if (response) {
            return *response == "true";
        }
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("ExposureComplete");
        if (result) {
            return result->boolVal == VARIANT_TRUE;
        }
    }
#endif

    return false;
}

auto HardwareInterface::isImageReady() -> bool {
    if (!connected_) {
        return false;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "imageready");
        if (response) {
            return *response == "true";
        }
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("ImageReady");
        if (result) {
            return result->boolVal == VARIANT_TRUE;
        }
    }
#endif

    return false;
}

auto HardwareInterface::getExposureProgress() -> double {
    if (!connected_) {
        return -1.0;
    }

    // Most ASCOM cameras don't support exposure progress
    // Return -1 to indicate not supported
    return -1.0;
}

auto HardwareInterface::getImageArray() -> std::optional<std::vector<uint32_t>> {
    if (!connected_) {
        return std::nullopt;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        // TODO: Implement Alpaca image array retrieval
        // This would involve getting the ImageArray property
        spdlog::warn("Alpaca image array retrieval not yet implemented");
        return std::nullopt;
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("ImageArray");
        if (result) {
            // TODO: Convert VARIANT array to std::vector<uint32_t>
            // This involves handling SAFEARRAY of variants
            spdlog::warn("COM image array conversion not yet implemented");
            return std::nullopt;
        }
    }
#endif

    return std::nullopt;
}

auto HardwareInterface::getImageArrayVariant() -> std::optional<std::vector<uint8_t>> {
    if (!connected_) {
        return std::nullopt;
    }

    // TODO: Implement variant image array retrieval
    spdlog::warn("Variant image array retrieval not yet implemented");
    return std::nullopt;
}

auto HardwareInterface::setGain(int gain) -> bool {
    if (!connected_) {
        setLastError("Not connected to camera");
        return false;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        std::string params = "Gain=" + std::to_string(gain);
        auto response = sendAlpacaRequest("PUT", "gain", params);
        return response.has_value();
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_I4;
        value.intVal = gain;
        return setCOMProperty("Gain", value);
    }
#endif

    return false;
}

auto HardwareInterface::getGain() -> std::optional<int> {
    if (!connected_) {
        return std::nullopt;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "gain");
        if (response) {
            return std::stoi(*response);
        }
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("Gain");
        if (result) {
            return result->intVal;
        }
    }
#endif

    return std::nullopt;
}

auto HardwareInterface::getGainRange() -> std::pair<int, int> {
    // TODO: Implement gain range retrieval
    // This would require querying camera capabilities
    return {0, 1000}; // Default range
}

auto HardwareInterface::setOffset(int offset) -> bool {
    if (!connected_) {
        setLastError("Not connected to camera");
        return false;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        std::string params = "Offset=" + std::to_string(offset);
        auto response = sendAlpacaRequest("PUT", "offset", params);
        return response.has_value();
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_I4;
        value.intVal = offset;
        return setCOMProperty("Offset", value);
    }
#endif

    return false;
}

auto HardwareInterface::getOffset() -> std::optional<int> {
    if (!connected_) {
        return std::nullopt;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "offset");
        if (response) {
            return std::stoi(*response);
        }
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("Offset");
        if (result) {
            return result->intVal;
        }
    }
#endif

    return std::nullopt;
}

auto HardwareInterface::getOffsetRange() -> std::pair<int, int> {
    // TODO: Implement offset range retrieval
    return {0, 255}; // Default range
}

auto HardwareInterface::setTargetTemperature(double temperature) -> bool {
    if (!connected_) {
        setLastError("Not connected to camera");
        return false;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        std::string params = "SetCCDTemperature=" + std::to_string(temperature);
        auto response = sendAlpacaRequest("PUT", "setccdtemperature", params);
        return response.has_value();
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_R8;
        value.dblVal = temperature;
        return setCOMProperty("SetCCDTemperature", value);
    }
#endif

    return false;
}

auto HardwareInterface::getCurrentTemperature() -> std::optional<double> {
    if (!connected_) {
        return std::nullopt;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "ccdtemperature");
        if (response) {
            return std::stod(*response);
        }
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("CCDTemperature");
        if (result) {
            return result->dblVal;
        }
    }
#endif

    return std::nullopt;
}

auto HardwareInterface::setCoolerEnabled(bool enable) -> bool {
    if (!connected_) {
        setLastError("Not connected to camera");
        return false;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        std::string params = "CoolerOn=" + std::string(enable ? "true" : "false");
        auto response = sendAlpacaRequest("PUT", "cooleron", params);
        return response.has_value();
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_BOOL;
        value.boolVal = enable ? VARIANT_TRUE : VARIANT_FALSE;
        return setCOMProperty("CoolerOn", value);
    }
#endif

    return false;
}

auto HardwareInterface::isCoolerEnabled() -> bool {
    if (!connected_) {
        return false;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "cooleron");
        if (response) {
            return *response == "true";
        }
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("CoolerOn");
        if (result) {
            return result->boolVal == VARIANT_TRUE;
        }
    }
#endif

    return false;
}

auto HardwareInterface::getCoolingPower() -> std::optional<double> {
    if (!connected_) {
        return std::nullopt;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        auto response = sendAlpacaRequest("GET", "coolerpower");
        if (response) {
            return std::stod(*response);
        }
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("CoolerPower");
        if (result) {
            return result->dblVal;
        }
    }
#endif

    return std::nullopt;
}

auto HardwareInterface::setFrame(int startX, int startY, int width, int height) -> bool {
    if (!connected_) {
        setLastError("Not connected to camera");
        return false;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        std::ostringstream params;
        params << "StartX=" << startX << "&StartY=" << startY 
               << "&NumX=" << width << "&NumY=" << height;
        auto response = sendAlpacaRequest("PUT", "frame", params.str());
        return response.has_value();
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        // Set individual properties
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_I4;
        
        value.intVal = startX;
        if (!setCOMProperty("StartX", value)) return false;
        
        value.intVal = startY;
        if (!setCOMProperty("StartY", value)) return false;
        
        value.intVal = width;
        if (!setCOMProperty("NumX", value)) return false;
        
        value.intVal = height;
        if (!setCOMProperty("NumY", value)) return false;
        
        return true;
    }
#endif

    return false;
}

auto HardwareInterface::setBinning(int binX, int binY) -> bool {
    if (!connected_) {
        setLastError("Not connected to camera");
        return false;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        std::ostringstream params;
        params << "BinX=" << binX << "&BinY=" << binY;
        auto response = sendAlpacaRequest("PUT", "binning", params.str());
        return response.has_value();
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_I4;
        
        value.intVal = binX;
        if (!setCOMProperty("BinX", value)) return false;
        
        value.intVal = binY;
        if (!setCOMProperty("BinY", value)) return false;
        
        return true;
    }
#endif

    return false;
}

auto HardwareInterface::getLastError() const -> std::string {
    std::lock_guard<std::mutex> lock(errorMutex_);
    return lastError_;
}

// ============================================================================
// Private Methods
// ============================================================================

auto HardwareInterface::setLastError(const std::string& error) const -> void {
    std::lock_guard<std::mutex> lock(errorMutex_);
    lastError_ = error;
    spdlog::error("ASCOM Hardware Interface Error: {}", error);
}

#ifdef _WIN32
auto HardwareInterface::initializeCOM() -> bool {
    if (comInitialized_) {
        return true;
    }

    HRESULT hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    if (FAILED(hr) && hr != RPC_E_CHANGED_MODE) {
        spdlog::error("Failed to initialize COM: {}", hr);
        return false;
    }

    comInitialized_ = true;
    return true;
}

auto HardwareInterface::shutdownCOM() -> void {
    if (comCamera_) {
        comCamera_->Release();
        comCamera_ = nullptr;
    }
    
    if (comInitialized_) {
        CoUninitialize();
        comInitialized_ = false;
    }
}

auto HardwareInterface::connectToCOMDriver(const std::string& progID) -> bool {
    spdlog::info("Connecting to COM camera driver: {}", progID);

    CLSID clsid;
    HRESULT hr = CLSIDFromProgID(CComBSTR(progID.c_str()), &clsid);
    if (FAILED(hr)) {
        setLastError("Failed to get CLSID from ProgID: " + std::to_string(hr));
        return false;
    }

    hr = CoCreateInstance(
        clsid, nullptr, CLSCTX_INPROC_SERVER | CLSCTX_LOCAL_SERVER,
        IID_IDispatch, reinterpret_cast<void**>(&comCamera_));
    if (FAILED(hr)) {
        setLastError("Failed to create COM instance: " + std::to_string(hr));
        return false;
    }

    // Set Connected = true
    VARIANT value;
    VariantInit(&value);
    value.vt = VT_BOOL;
    value.boolVal = VARIANT_TRUE;

    if (setCOMProperty("Connected", value)) {
        connected_ = true;
        updateCameraInfo();
        return true;
    }

    return false;
}

auto HardwareInterface::disconnectFromCOMDriver() -> bool {
    spdlog::info("Disconnecting from COM camera driver");

    if (comCamera_) {
        VARIANT value;
        VariantInit(&value);
        value.vt = VT_BOOL;
        value.boolVal = VARIANT_FALSE;
        setCOMProperty("Connected", value);

        comCamera_->Release();
        comCamera_ = nullptr;
    }

    return true;
}

auto HardwareInterface::invokeCOMMethod(const std::string& method, VARIANT* params,
                                      int paramCount) -> std::optional<VARIANT> {
    if (!comCamera_) {
        return std::nullopt;
    }

    DISPID dispid;
    CComBSTR methodName(method.c_str());
    HRESULT hr = comCamera_->GetIDsOfNames(IID_NULL, &methodName, 1,
                                          LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        spdlog::error("Failed to get method ID for {}: {}", method, hr);
        return std::nullopt;
    }

    DISPPARAMS dispparams = {params, nullptr, paramCount, 0};
    VARIANT result;
    VariantInit(&result);

    hr = comCamera_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                           DISPATCH_METHOD, &dispparams, &result, nullptr,
                           nullptr);
    if (FAILED(hr)) {
        spdlog::error("Failed to invoke method {}: {}", method, hr);
        return std::nullopt;
    }

    return result;
}

auto HardwareInterface::getCOMProperty(const std::string& property)
    -> std::optional<VARIANT> {
    if (!comCamera_) {
        return std::nullopt;
    }

    DISPID dispid;
    CComBSTR propertyName(property.c_str());
    HRESULT hr = comCamera_->GetIDsOfNames(IID_NULL, &propertyName, 1,
                                          LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        spdlog::error("Failed to get property ID for {}: {}", property, hr);
        return std::nullopt;
    }

    DISPPARAMS dispparams = {nullptr, nullptr, 0, 0};
    VARIANT result;
    VariantInit(&result);

    hr = comCamera_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                           DISPATCH_PROPERTYGET, &dispparams, &result,
                           nullptr, nullptr);
    if (FAILED(hr)) {
        spdlog::error("Failed to get property {}: {}", property, hr);
        return std::nullopt;
    }

    return result;
}

auto HardwareInterface::setCOMProperty(const std::string& property,
                                     const VARIANT& value) -> bool {
    if (!comCamera_) {
        return false;
    }

    DISPID dispid;
    CComBSTR propertyName(property.c_str());
    HRESULT hr = comCamera_->GetIDsOfNames(IID_NULL, &propertyName, 1,
                                          LOCALE_USER_DEFAULT, &dispid);
    if (FAILED(hr)) {
        spdlog::error("Failed to get property ID for {}: {}", property, hr);
        return false;
    }

    VARIANT params[] = {value};
    DISPID dispidPut = DISPID_PROPERTYPUT;
    DISPPARAMS dispparams = {params, &dispidPut, 1, 1};

    hr = comCamera_->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                           DISPATCH_PROPERTYPUT, &dispparams, nullptr,
                           nullptr, nullptr);
    if (FAILED(hr)) {
        spdlog::error("Failed to set property {}: {}", property, hr);
        return false;
    }

    return true;
}
#endif

auto HardwareInterface::connectToAlpacaDevice(const std::string& host, int port,
                                            int deviceNumber) -> bool {
    spdlog::info("Connecting to Alpaca camera device at {}:{} device {}", 
                host, port, deviceNumber);

    // Test connection by getting device info
    auto response = sendAlpacaRequest("GET", "connected");
    if (response) {
        connected_ = true;
        updateCameraInfo();
        return true;
    }

    return false;
}

auto HardwareInterface::disconnectFromAlpacaDevice() -> bool {
    spdlog::info("Disconnecting from Alpaca camera device");

    if (connected_) {
        sendAlpacaRequest("PUT", "connected", "Connected=false");
    }

    return true;
}

auto HardwareInterface::sendAlpacaRequest(const std::string& method,
                                        const std::string& endpoint,
                                        const std::string& params) -> std::optional<std::string> {
    // TODO: Implement HTTP client for Alpaca REST API
    // This would use libcurl or similar HTTP library
    spdlog::debug("Sending Alpaca request: {} {} {}", method, endpoint, params);
    return std::nullopt;
}

auto HardwareInterface::parseAlpacaResponse(const std::string& response)
    -> std::optional<std::string> {
    // TODO: Parse JSON response and extract Value field
    return std::nullopt;
}

auto HardwareInterface::updateCameraInfo() -> bool {
    if (!connected_) {
        return false;
    }

    std::lock_guard<std::mutex> lock(infoMutex_);
    
    CameraInfo info;
    info.name = deviceName_;

    // Get camera properties based on connection type
    if (connectionType_ == ConnectionType::ALPACA_REST) {
        // TODO: Get camera dimensions and capabilities from Alpaca
        info.cameraXSize = 1920;
        info.cameraYSize = 1080;
        // ... other properties
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        auto widthResult = getCOMProperty("CameraXSize");
        auto heightResult = getCOMProperty("CameraYSize");

        if (widthResult && heightResult) {
            info.cameraXSize = widthResult->intVal;
            info.cameraYSize = heightResult->intVal;
        }
        
        // Get other camera properties...
        auto canAbortResult = getCOMProperty("CanAbortExposure");
        if (canAbortResult) {
            info.canAbortExposure = canAbortResult->boolVal == VARIANT_TRUE;
        }
        
        // ... get more properties as needed
    }
#endif

    cameraInfo_ = info;
    return true;
}

} // namespace lithium::device::ascom::camera::components
