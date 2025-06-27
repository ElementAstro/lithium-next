/*
 * hardware_interface.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Camera Hardware Interface Component

This component provides a clean interface to ASCOM Camera APIs,
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

#ifdef _WIN32
// clang-format off
#include <windows.h>
#include <comdef.h>
#include <comutil.h>
// clang-format on
#endif

namespace lithium::device::ascom::camera::components {

/**
 * @brief Connection type enumeration
 */
enum class ConnectionType {
    COM_DRIVER,    // Windows COM/ASCOM driver
    ALPACA_REST    // ASCOM Alpaca REST protocol
};

/**
 * @brief ASCOM Camera states
 */
enum class ASCOMCameraState {
    IDLE = 0,
    WAITING = 1,
    EXPOSING = 2,
    READING = 3,
    DOWNLOAD = 4,
    ERROR = 5
};

/**
 * @brief ASCOM Sensor types
 */
enum class ASCOMSensorType {
    MONOCHROME = 0,
    COLOR = 1,
    RGGB = 2,
    CMYG = 3,
    CMYG2 = 4,
    LRGB = 5
};

/**
 * @brief Hardware Interface for ASCOM Camera communication
 *
 * This component encapsulates all direct interaction with ASCOM Camera APIs,
 * providing a clean C++ interface for hardware operations while managing
 * both COM driver and Alpaca REST communication, device enumeration,
 * connection management, and low-level parameter control.
 */
class HardwareInterface {
public:
    struct CameraInfo {
        std::string name;
        std::string serialNumber;
        std::string driverInfo;
        std::string driverVersion;
        int cameraXSize = 0;
        int cameraYSize = 0;
        double pixelSizeX = 0.0;
        double pixelSizeY = 0.0;
        int maxBinX = 1;
        int maxBinY = 1;
        int bayerOffsetX = 0;
        int bayerOffsetY = 0;
        bool canAbortExposure = false;
        bool canAsymmetricBin = false;
        bool canFastReadout = false;
        bool canStopExposure = false;
        bool canSubFrame = false;
        bool hasShutter = false;
        ASCOMSensorType sensorType = ASCOMSensorType::MONOCHROME;
        double electronsPerADU = 1.0;
        double fullWellCapacity = 0.0;
        int maxADU = 65535;
        bool hasCooler = false;
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
    HardwareInterface();
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
     * @brief Discover available ASCOM camera devices
     * @return Vector of device names/identifiers
     */
    std::vector<std::string> discoverDevices();

    /**
     * @brief Connect to a camera device
     * @param settings Connection settings
     * @return true if connection successful
     */
    bool connect(const ConnectionSettings& settings);

    /**
     * @brief Disconnect from current camera
     * @return true if disconnection successful
     */
    bool disconnect();

    /**
     * @brief Check if connected to a camera
     * @return true if connected
     */
    bool isConnected() const { return connected_; }

    /**
     * @brief Get connection type
     * @return Current connection type
     */
    ConnectionType getConnectionType() const { return connectionType_; }

    // =========================================================================
    // Camera Information and Properties
    // =========================================================================

    /**
     * @brief Get camera information
     * @return Optional camera info structure
     */
    std::optional<CameraInfo> getCameraInfo() const;

    /**
     * @brief Get camera state
     * @return Current camera state
     */
    ASCOMCameraState getCameraState() const;

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
    // Exposure Control
    // =========================================================================

    /**
     * @brief Start an exposure
     * @param duration Exposure duration in seconds
     * @param light True for light frame, false for dark frame
     * @return true if exposure started successfully
     */
    bool startExposure(double duration, bool light = true);

    /**
     * @brief Stop current exposure
     * @return true if exposure stopped successfully
     */
    bool stopExposure();

    /**
     * @brief Check if camera is exposing
     * @return true if exposing
     */
    bool isExposing() const;

    /**
     * @brief Check if image is ready for download
     * @return true if image ready
     */
    bool isImageReady() const;

    /**
     * @brief Get exposure progress (0.0 to 1.0)
     * @return Progress value
     */
    double getExposureProgress() const;

    /**
     * @brief Get remaining exposure time
     * @return Remaining time in seconds
     */
    double getRemainingExposureTime() const;

    // =========================================================================
    // Image Retrieval
    // =========================================================================

    /**
     * @brief Get image array from camera
     * @return Optional vector of image data
     */
    std::optional<std::vector<uint16_t>> getImageArray();

    /**
     * @brief Get image dimensions
     * @return Pair of width, height
     */
    std::pair<int, int> getImageDimensions() const;

    // =========================================================================
    // Camera Settings
    // =========================================================================

    /**
     * @brief Set CCD temperature
     * @param temperature Target temperature in Celsius
     * @return true if set successfully
     */
    bool setCCDTemperature(double temperature);

    /**
     * @brief Get CCD temperature
     * @return Current temperature in Celsius
     */
    double getCCDTemperature() const;

    /**
     * @brief Enable/disable cooler
     * @param enable True to enable cooler
     * @return true if set successfully
     */
    bool setCoolerOn(bool enable);

    /**
     * @brief Check if cooler is on
     * @return true if cooler is enabled
     */
    bool isCoolerOn() const;

    /**
     * @brief Get cooler power
     * @return Cooler power percentage (0-100)
     */
    double getCoolerPower() const;

    /**
     * @brief Set camera gain
     * @param gain Gain value
     * @return true if set successfully
     */
    bool setGain(int gain);

    /**
     * @brief Get camera gain
     * @return Current gain value
     */
    int getGain() const;

    /**
     * @brief Get gain range
     * @return Pair of min, max gain values
     */
    std::pair<int, int> getGainRange() const;

    /**
     * @brief Set camera offset
     * @param offset Offset value
     * @return true if set successfully
     */
    bool setOffset(int offset);

    /**
     * @brief Get camera offset
     * @return Current offset value
     */
    int getOffset() const;

    /**
     * @brief Get offset range
     * @return Pair of min, max offset values
     */
    std::pair<int, int> getOffsetRange() const;

    // =========================================================================
    // Frame Settings
    // =========================================================================

    /**
     * @brief Set binning
     * @param binX Horizontal binning
     * @param binY Vertical binning
     * @return true if set successfully
     */
    bool setBinning(int binX, int binY);

    /**
     * @brief Get current binning
     * @return Pair of horizontal, vertical binning
     */
    std::pair<int, int> getBinning() const;

    /**
     * @brief Set subframe/ROI
     * @param startX Starting X coordinate
     * @param startY Starting Y coordinate
     * @param numX Width of subframe
     * @param numY Height of subframe
     * @return true if set successfully
     */
    bool setSubFrame(int startX, int startY, int numX, int numY);

    /**
     * @brief Get current subframe settings
     * @return Tuple of startX, startY, numX, numY
     */
    std::tuple<int, int, int, int> getSubFrame() const;

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
    
    // Connection details
    ConnectionType connectionType_{ConnectionType::ALPACA_REST};
    ConnectionSettings currentSettings_;
    
    // Camera information cache
    mutable std::optional<CameraInfo> cameraInfo_;
    mutable std::chrono::steady_clock::time_point lastInfoUpdate_;
    
    // Error handling
    mutable std::string lastError_;

#ifdef _WIN32
    // COM interface
    IDispatch* comCamera_ = nullptr;
    
    // COM helper methods
    auto invokeCOMMethod(const std::string& method, VARIANT* params = nullptr, int paramCount = 0) -> std::optional<VARIANT>;
    auto getCOMProperty(const std::string& property) -> std::optional<VARIANT>;
    auto setCOMProperty(const std::string& property, const VARIANT& value) -> bool;
#endif

    // Alpaca helper methods
    auto sendAlpacaRequest(const std::string& method, const std::string& endpoint, const std::string& params = "") const -> std::optional<std::string>;
    auto parseAlpacaResponse(const std::string& response) -> std::optional<std::string>;
    auto buildAlpacaUrl(const std::string& endpoint) const -> std::string;
    
    // Connection type specific methods
    auto connectCOM(const ConnectionSettings& settings) -> bool;
    auto connectAlpaca(const ConnectionSettings& settings) -> bool;
    auto disconnectCOM() -> bool;
    auto disconnectAlpaca() -> bool;
    
    // Alpaca discovery
    auto discoverAlpacaDevices() -> std::vector<std::string>;
    
    // Information caching
    auto updateCameraInfo() const -> bool;
    auto shouldUpdateInfo() const -> bool;
    
    // Error handling helpers
    void setError(const std::string& error) const { lastError_ = error; }
};

} // namespace lithium::device::ascom::camera::components
