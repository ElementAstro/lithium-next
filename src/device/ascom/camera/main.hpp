/*
 * main.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Camera Modular Integration Header

This file provides the main integration points for the modular ASCOM camera
implementation, including entry points, factory methods, and public API.

*************************************************/

#pragma once

#include <memory>
#include <string>
#include <functional>
#include <map>
#include <vector>
#include <mutex>
#include <optional>

#include "controller.hpp"

// Forward declarations
namespace lithium::device::ascom::camera::components {
    class HardwareInterface;
    enum class ConnectionType;
}
#include "device/template/camera_frame.hpp"

namespace lithium::device::ascom::camera {

/**
 * @brief Main ASCOM Camera Integration Class
 *
 * This class provides the primary integration interface for the modular
 * ASCOM camera system. It encapsulates the controller and provides
 * simplified access to camera functionality.
 */
class ASCOMCameraMain {
public:
    // Configuration structure for camera initialization
    struct CameraConfig {
        std::string deviceName = "Default ASCOM Camera";
        std::string progId;                                    // COM driver ProgID
        std::string host = "localhost";                        // Alpaca host
        int port = 11111;                                     // Alpaca port
        int deviceNumber = 0;                                 // Alpaca device number
        std::string clientId = "Lithium-Next";               // Client ID
        int connectionType = 0;                               // 0=COM, 1=ALPACA_REST

        // Optional callbacks
        std::function<void(const std::string&)> logCallback;
        std::function<void(std::shared_ptr<AtomCameraFrame>)> frameCallback;
        std::function<void(const std::string&, double)> progressCallback;
    };

    // Camera state enumeration
    enum class CameraState {
        DISCONNECTED,
        CONNECTING,
        CONNECTED,
        EXPOSING,
        READING,
        IDLE,
        ERROR
    };

public:
    ASCOMCameraMain();
    ~ASCOMCameraMain();

    // Non-copyable and non-movable
    ASCOMCameraMain(const ASCOMCameraMain&) = delete;
    ASCOMCameraMain& operator=(const ASCOMCameraMain&) = delete;
    ASCOMCameraMain(ASCOMCameraMain&&) = delete;
    ASCOMCameraMain& operator=(ASCOMCameraMain&&) = delete;

    // =========================================================================
    // Initialization and Connection
    // =========================================================================

    /**
     * @brief Initialize the camera system with configuration
     * @param config Camera configuration
     * @return true if initialization successful
     */
    bool initialize(const CameraConfig& config);

    /**
     * @brief Connect to the ASCOM camera
     * @return true if connection successful
     */
    bool connect();

    /**
     * @brief Disconnect from the camera
     * @return true if disconnection successful
     */
    bool disconnect();

    /**
     * @brief Check if camera is connected
     * @return true if connected
     */
    bool isConnected() const;

    /**
     * @brief Get current camera state
     * @return Current state
     */
    CameraState getState() const;

    /**
     * @brief Get state as string
     * @return State description
     */
    std::string getStateString() const;

    // =========================================================================
    // Basic Camera Operations
    // =========================================================================

    /**
     * @brief Start an exposure
     * @param duration Exposure duration in seconds
     * @param isDark Whether this is a dark frame
     * @return true if exposure started
     */
    bool startExposure(double duration, bool isDark = false);

    /**
     * @brief Abort current exposure
     * @return true if exposure aborted
     */
    bool abortExposure();

    /**
     * @brief Check if camera is exposing
     * @return true if exposing
     */
    bool isExposing() const;

    /**
     * @brief Get the last captured image
     * @return Image frame or nullptr if none available
     */
    std::shared_ptr<AtomCameraFrame> getLastImage();

    /**
     * @brief Download current image
     * @return Image frame or nullptr if failed
     */
    std::shared_ptr<AtomCameraFrame> downloadImage();

    // =========================================================================
    // Camera Properties
    // =========================================================================

    /**
     * @brief Get camera name
     * @return Camera name
     */
    std::string getCameraName() const;

    /**
     * @brief Get camera description
     * @return Camera description
     */
    std::string getDescription() const;

    /**
     * @brief Get driver info
     * @return Driver information
     */
    std::string getDriverInfo() const;

    /**
     * @brief Get driver version
     * @return Driver version
     */
    std::string getDriverVersion() const;

    /**
     * @brief Get camera X size (pixels)
     * @return X size in pixels
     */
    int getCameraXSize() const;

    /**
     * @brief Get camera Y size (pixels)
     * @return Y size in pixels
     */
    int getCameraYSize() const;

    /**
     * @brief Get pixel size X (micrometers)
     * @return Pixel size X
     */
    double getPixelSizeX() const;

    /**
     * @brief Get pixel size Y (micrometers)
     * @return Pixel size Y
     */
    double getPixelSizeY() const;

    // =========================================================================
    // Temperature Control
    // =========================================================================

    /**
     * @brief Set target CCD temperature
     * @param temperature Target temperature in Celsius
     * @return true if temperature set successfully
     */
    bool setCCDTemperature(double temperature);

    /**
     * @brief Get current CCD temperature
     * @return Current temperature in Celsius
     */
    double getCCDTemperature() const;

    /**
     * @brief Check if cooling is available
     * @return true if camera has cooling
     */
    bool hasCooling() const;

    /**
     * @brief Check if cooling is enabled
     * @return true if cooling is on
     */
    bool isCoolingEnabled() const;

    /**
     * @brief Enable or disable cooling
     * @param enable true to enable cooling
     * @return true if successful
     */
    bool setCoolingEnabled(bool enable);

    // =========================================================================
    // Video and Live Mode
    // =========================================================================

    /**
     * @brief Start live video mode
     * @return true if started successfully
     */
    bool startLiveMode();

    /**
     * @brief Stop live video mode
     * @return true if stopped successfully
     */
    bool stopLiveMode();

    /**
     * @brief Check if live mode is active
     * @return true if live mode running
     */
    bool isLiveModeActive() const;

    /**
     * @brief Get latest live frame
     * @return Latest frame or nullptr
     */
    std::shared_ptr<AtomCameraFrame> getLiveFrame();

    // =========================================================================
    // Advanced Features
    // =========================================================================

    /**
     * @brief Set ROI (Region of Interest)
     * @param startX Start X coordinate
     * @param startY Start Y coordinate
     * @param width ROI width
     * @param height ROI height
     * @return true if ROI set successfully
     */
    bool setROI(int startX, int startY, int width, int height);

    /**
     * @brief Reset ROI to full frame
     * @return true if reset successful
     */
    bool resetROI();

    /**
     * @brief Set binning
     * @param binning Binning factor (1, 2, 3, 4...)
     * @return true if binning set successfully
     */
    bool setBinning(int binning);

    /**
     * @brief Get current binning
     * @return Current binning factor
     */
    int getBinning() const;

    /**
     * @brief Set camera gain
     * @param gain Gain value
     * @return true if gain set successfully
     */
    bool setGain(int gain);

    /**
     * @brief Get camera gain
     * @return Current gain value
     */
    int getGain() const;

    // =========================================================================
    // Statistics and Monitoring
    // =========================================================================

    /**
     * @brief Get camera statistics
     * @return Statistics map
     */
    std::map<std::string, double> getStatistics() const;

    /**
     * @brief Get last error message
     * @return Error message or empty string
     */
    std::string getLastError() const;

    /**
     * @brief Clear last error
     */
    void clearLastError();

    // =========================================================================
    // Access to Controller
    // =========================================================================

    /**
     * @brief Get the underlying controller
     * @return Shared pointer to controller
     */
    std::shared_ptr<ASCOMCameraController> getController() const;

private:
    // Private implementation data
    std::shared_ptr<ASCOMCameraController> controller_;
    CameraConfig config_;
    mutable std::mutex stateMutex_;
    CameraState state_;
    std::string lastError_;

    // Helper methods
    void setState(CameraState newState);
    void setError(const std::string& error);
    CameraState convertControllerState() const;
};

// =========================================================================
// Factory Functions
// =========================================================================

/**
 * @brief Create a new ASCOM camera instance
 * @param config Camera configuration
 * @return Shared pointer to camera instance or nullptr on failure
 */
std::shared_ptr<ASCOMCameraMain> createASCOMCamera(const ASCOMCameraMain::CameraConfig& config);

/**
 * @brief Create ASCOM camera with default configuration
 * @param deviceName Device name or ProgID
 * @return Shared pointer to camera instance or nullptr on failure
 */
std::shared_ptr<ASCOMCameraMain> createASCOMCamera(const std::string& deviceName);

/**
 * @brief Discover available ASCOM cameras
 * @return Vector of available camera names/ProgIDs
 */
std::vector<std::string> discoverASCOMCameras();

/**
 * @brief Camera capabilities structure
 */
struct CameraCapabilities {
    int maxWidth = 0;
    int maxHeight = 0;
    double pixelSizeX = 0.0;
    double pixelSizeY = 0.0;
    int maxBinning = 1;
    bool hasCooler = false;
    bool hasShutter = true;
    bool canAbortExposure = true;
    bool canStopExposure = true;
    bool canGetCoolerPower = false;
    bool canSetCCDTemperature = false;
    bool hasGainControl = false;
    bool hasOffsetControl = false;
    double minExposure = 0.001;
    double maxExposure = 3600.0;
    double electronsPerADU = 1.0;
    double fullWellCapacity = 0.0;
    int maxADU = 65535;
};

/**
 * @brief Get ASCOM camera capabilities
 * @param deviceName Device name or ProgID
 * @return Camera capabilities structure
 */
std::optional<CameraCapabilities>
getASCOMCameraCapabilities(const std::string& deviceName);

} // namespace lithium::device::ascom::camera
