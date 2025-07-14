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
#include <format>
#include <iomanip>
#include <sstream>
#include <thread>

#include <spdlog/spdlog.h>

#include "../../alpaca_client.hpp"

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

namespace lithium::device::ascom::camera::components {

HardwareInterface::HardwareInterface(boost::asio::io_context& io_context) 
    : io_context_(io_context) {
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
#endif

    // Initialize Alpaca client
    try {
        alpaca_client_ = std::make_unique<lithium::device::ascom::DeviceClient<lithium::device::ascom::DeviceType::Camera>>(
            io_context_);
        spdlog::info("Alpaca client initialized successfully");
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to initialize Alpaca client: ") + e.what());
        return false;
    }

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

    // Reset Alpaca client
    alpaca_client_.reset();

#ifdef _WIN32
    shutdownCOM();
#endif

    initialized_ = false;
    spdlog::info("ASCOM Hardware Interface shutdown complete");
    return true;
}

auto HardwareInterface::discoverDevices() -> std::vector<std::string> {
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
    
    spdlog::info("Discovering Alpaca camera devices using optimized client");
    
    if (!alpaca_client_) {
        spdlog::error("Alpaca client not initialized");
        return devices;
    }

    try {
        // Use the new client's device discovery
        boost::asio::co_spawn(io_context_, [this, &devices]() -> boost::asio::awaitable<void> {
            auto result = co_await alpaca_client_->discover_devices();
            if (result) {
                for (const auto& device : result.value()) {
                    devices.push_back(std::format("{}:{}/camera/{}", 
                                                device.host, device.port, device.number));
                }
            }
        }, boost::asio::detached);
        
        // Give some time for discovery
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        
    } catch (const std::exception& e) {
        spdlog::error("Error during Alpaca device discovery: {}", e.what());
    }
    
    // If no devices found, add localhost default
    if (devices.empty()) {
        devices.push_back("localhost:11111/camera/0");
    }
    
    spdlog::debug("Found {} Alpaca devices", devices.size());
    return devices;
}

auto HardwareInterface::connect(const ConnectionSettings& settings) -> bool {
    if (!initialized_) {
        setLastError("Hardware interface not initialized");
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
    setLastError("COM drivers not supported on non-Windows platforms");
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

auto HardwareInterface::getCameraInfo() const -> std::optional<CameraInfo> {
    std::lock_guard<std::mutex> lock(infoMutex_);

    if (!connected_) {
        return std::nullopt;
    }

    // Update camera info if needed
    if (!cameraInfo_.has_value()) {
        const_cast<HardwareInterface*>(this)->updateCameraInfo();
    }

    return cameraInfo_;
}

auto HardwareInterface::getCameraState() const -> ASCOMCameraState {
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

auto HardwareInterface::isExposing() const -> bool {
    if (!connected_) {
        return false;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        auto response = const_cast<HardwareInterface*>(this)->sendAlpacaRequest("GET", "exposurecomplete");
        if (response) {
            return *response != "true";  // If exposure is not complete, then it's exposing
        }
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        auto result = const_cast<HardwareInterface*>(this)->getCOMProperty("ExposureComplete");
        if (result) {
            return result->boolVal != VARIANT_TRUE;  // If exposure is not complete, then it's exposing
        }
    }
#endif

    return false;
}

auto HardwareInterface::isImageReady() const -> bool {
    if (!connected_) {
        return false;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        auto response = const_cast<HardwareInterface*>(this)->sendAlpacaRequest("GET", "imageready");
        if (response) {
            return *response == "true";
        }
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        auto result = const_cast<HardwareInterface*>(this)->getCOMProperty("ImageReady");
        if (result) {
            return result->boolVal == VARIANT_TRUE;
        }
    }
#endif

    return false;
}

auto HardwareInterface::getExposureProgress() const -> double {
    if (!connected_) {
        return -1.0;
    }

    // Most ASCOM cameras don't support exposure progress
    // Return -1 to indicate not supported
    return -1.0;
}

auto HardwareInterface::getImageArray() -> std::optional<std::vector<uint16_t>> {
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
            // TODO: Convert VARIANT array to std::vector<uint16_t>
            // This involves handling SAFEARRAY of variants
            spdlog::warn("COM image array conversion not yet implemented");
            return std::nullopt;
        }
    }
#endif

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

auto HardwareInterface::getGain() const -> int {
    if (!connected_) {
        return 0;
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

    return 0;
}

auto HardwareInterface::getGainRange() const -> std::pair<int, int> {
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

auto HardwareInterface::getOffset() const -> int {
    if (!connected_) {
        return 0;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        auto response = const_cast<HardwareInterface*>(this)->sendAlpacaRequest("GET", "offset");
        if (response) {
            return std::stoi(*response);
        }
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        auto result = const_cast<HardwareInterface*>(this)->getCOMProperty("Offset");
        if (result) {
            return result->intVal;
        }
    }
#endif

    return 0;
}

auto HardwareInterface::getOffsetRange() const -> std::pair<int, int> {
    // TODO: Implement offset range retrieval
    return {0, 255}; // Default range
}

auto HardwareInterface::setCCDTemperature(double temperature) -> bool {
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

auto HardwareInterface::getCCDTemperature() const -> double {
    if (!connected_) {
        return -999.0; // Invalid temperature
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        // Use the new Alpaca client - for now return placeholder
        // TODO: Implement proper async handling for CCD temperature
        return -999.0; // Placeholder until async integration is complete
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("CCDTemperature");
        if (result) {
            return result->dblVal;
        }
    }
#endif

    return -999.0; // Invalid temperature
}

auto HardwareInterface::setCoolerOn(bool enable) -> bool {
    if (!connected_) {
        setLastError("Not connected to camera");
        return false;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        // Use the new Alpaca client - placeholder implementation
        // TODO: Implement proper async handling for cooler control
        return false; // Placeholder until async integration is complete
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

auto HardwareInterface::isCoolerOn() const -> bool {
    if (!connected_) {
        return false;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        // Use the new Alpaca client - placeholder implementation
        return false; // Placeholder until async integration is complete
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

auto HardwareInterface::getCoolerPower() const -> double {
    if (!connected_) {
        return 0.0;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        // Use the new Alpaca client - placeholder implementation
        return 0.0; // Placeholder until async integration is complete
    }

#ifdef _WIN32
    if (connectionType_ == ConnectionType::COM_DRIVER) {
        auto result = getCOMProperty("CoolerPower");
        if (result) {
            return result->dblVal;
        }
    }
#endif

    return 0.0;
}

auto HardwareInterface::setSubFrame(int startX, int startY, int numX, int numY) -> bool {
    if (!connected_) {
        setLastError("Not connected to camera");
        return false;
    }

    if (connectionType_ == ConnectionType::ALPACA_REST) {
        std::ostringstream params;
        params << "StartX=" << startX << "&StartY=" << startY 
               << "&NumX=" << numX << "&NumY=" << numY;
        auto response = const_cast<HardwareInterface*>(this)->sendAlpacaRequest("PUT", "frame", params.str());
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
        
        value.intVal = numX;
        if (!setCOMProperty("NumX", value)) return false;
        
        value.intVal = numY;
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

// ============================================================================
// Private Methods
// ============================================================================

auto HardwareInterface::sendAlpacaRequest(const std::string& method,
                                        const std::string& endpoint,
                                        const std::string& params) const -> std::optional<std::string> {
    // Legacy method implementation for compatibility
    // TODO: Replace with proper alpaca_client_ usage
    spdlog::debug("sendAlpacaRequest called: {} {} {}", method, endpoint, params);
    
    // For now, return a placeholder to prevent compile errors
    // This should be replaced with actual Alpaca API calls
    if (endpoint == "camerastate") {
        return "0"; // IDLE state
    } else if (endpoint == "exposurecomplete" || endpoint == "imageready") {
        return "false";
    } else if (endpoint == "gain" || endpoint == "offset") {
        return "100"; // Default value
    }
    
    return std::nullopt;
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

auto HardwareInterface::connectAlpaca(const ConnectionSettings& settings) -> bool {
    if (!alpaca_client_) {
        setLastError("Alpaca client not initialized");
        return false;
    }

    try {
        lithium::device::ascom::DeviceInfo device_info;
        device_info.host = settings.host;
        device_info.port = settings.port;
        device_info.number = settings.deviceNumber;
        
        // For now, set connected state directly
        deviceName_ = settings.deviceName;
        connected_ = true;
        updateCameraInfo();
        spdlog::info("Successfully connected to Alpaca device: {}", settings.deviceName);
        return true;
    } catch (const std::exception& e) {
        setLastError(std::string("Failed to connect to Alpaca device: ") + e.what());
        return false;
    }
}

auto HardwareInterface::disconnectAlpaca() -> bool {
    if (connected_) {
        connected_ = false;
        deviceName_.clear();
        spdlog::info("Disconnected from Alpaca device");
        return true;
    }
    return false;
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

auto HardwareInterface::getRemainingExposureTime() const -> double {
    // TODO: Implement exposure time tracking
    return 0.0;
}

auto HardwareInterface::getImageDimensions() const -> std::pair<int, int> {
    if (cameraInfo_.has_value()) {
        return {cameraInfo_->cameraXSize, cameraInfo_->cameraYSize};
    }
    return {0, 0};
}

auto HardwareInterface::getInterfaceVersion() const -> int {
    return 3; // ASCOM Standard v3
}

auto HardwareInterface::getDriverInfo() const -> std::string {
    if (cameraInfo_.has_value()) {
        return cameraInfo_->driverInfo;
    }
    return "Lithium ASCOM Hardware Interface";
}

auto HardwareInterface::getDriverVersion() const -> std::string {
    if (cameraInfo_.has_value()) {
        return cameraInfo_->driverVersion;
    }
    return "1.0.0";
}

auto HardwareInterface::getBinning() const -> std::pair<int, int> {
    // TODO: Implement binning retrieval from camera
    return {1, 1}; // Default 1x1 binning
}

auto HardwareInterface::getSubFrame() const -> std::tuple<int, int, int, int> {
    // TODO: Implement subframe retrieval from camera
    if (cameraInfo_.has_value()) {
        return {0, 0, cameraInfo_->cameraXSize, cameraInfo_->cameraYSize};
    }
    return {0, 0, 0, 0};
}

} // namespace lithium::device::ascom::camera::components
