/*
 * camera_factory.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: Enhanced Camera Factory for creating camera instances

*************************************************/

#pragma once

#include "../template/camera.hpp"
#include <spdlog/spdlog.h>

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>

namespace lithium::device {

/**
 * @brief Camera types supported by the factory
 */
enum class CameraDriverType {
    INDI,
    QHY,
    ASI,
    ATIK,
    SBIG,
    FLI,
    PLAYERONE,
    ASCOM,
    SIMULATOR,
    AUTO_DETECT
};

/**
 * @brief Camera information structure
 */
struct CameraInfo {
    std::string name;
    std::string manufacturer;
    std::string model;
    std::string driver;
    CameraDriverType type;
    bool isAvailable;
    std::string description;
};

/**
 * @brief Factory class for creating camera instances
 * 
 * This factory supports multiple camera driver types including INDI, QHY, ASI,
 * and ASCOM, providing a unified interface for camera creation and management.
 */
class CameraFactory {
public:
    using CreateCameraFunction = std::function<std::shared_ptr<AtomCamera>(const std::string&)>;

    /**
     * @brief Get the singleton instance of the camera factory
     */
    static CameraFactory& getInstance();

    /**
     * @brief Register a camera creation function for a specific driver type
     * @param type Camera driver type
     * @param createFunc Function to create camera instances
     */
    void registerCameraDriver(CameraDriverType type, CreateCameraFunction createFunc);

    /**
     * @brief Create a camera instance
     * @param type Driver type to use
     * @param name Camera name/identifier
     * @return Shared pointer to camera instance, nullptr on failure
     */
    std::shared_ptr<AtomCamera> createCamera(CameraDriverType type, const std::string& name);

    /**
     * @brief Create a camera instance with automatic driver detection
     * @param name Camera name/identifier
     * @return Shared pointer to camera instance, nullptr on failure
     */
    std::shared_ptr<AtomCamera> createCamera(const std::string& name);

    /**
     * @brief Scan for available cameras across all registered drivers
     * @return Vector of camera information structures
     */
    std::vector<CameraInfo> scanForCameras();

    /**
     * @brief Scan for cameras using a specific driver type
     * @param type Driver type to scan with
     * @return Vector of camera information structures
     */
    std::vector<CameraInfo> scanForCameras(CameraDriverType type);

    /**
     * @brief Get list of supported driver types
     * @return Vector of supported camera driver types
     */
    std::vector<CameraDriverType> getSupportedDriverTypes() const;

    /**
     * @brief Check if a driver type is supported
     * @param type Driver type to check
     * @return True if supported, false otherwise
     */
    bool isDriverSupported(CameraDriverType type) const;

    /**
     * @brief Convert driver type to string
     * @param type Driver type
     * @return String representation of driver type
     */
    static std::string driverTypeToString(CameraDriverType type);

    /**
     * @brief Convert string to driver type
     * @param typeStr String representation
     * @return Driver type, AUTO_DETECT if not recognized
     */
    static CameraDriverType stringToDriverType(const std::string& typeStr);

    /**
     * @brief Get detailed information about a camera
     * @param name Camera name/identifier
     * @param type Specific driver type, or AUTO_DETECT to search all
     * @return Camera information, empty if not found
     */
    CameraInfo getCameraInfo(const std::string& name, CameraDriverType type = CameraDriverType::AUTO_DETECT);

private:
    CameraFactory() = default;
    ~CameraFactory() = default;
    
    // Disable copy and move
    CameraFactory(const CameraFactory&) = delete;
    CameraFactory& operator=(const CameraFactory&) = delete;
    CameraFactory(CameraFactory&&) = delete;
    CameraFactory& operator=(CameraFactory&&) = delete;

    // Initialize default drivers
    void initializeDefaultDrivers();

    // Helper methods
    std::vector<CameraInfo> scanINDICameras();
    std::vector<CameraInfo> scanQHYCameras();
    std::vector<CameraInfo> scanASICameras();
    std::vector<CameraInfo> scanAtikCameras();
    std::vector<CameraInfo> scanSBIGCameras();
    std::vector<CameraInfo> scanFLICameras();
    std::vector<CameraInfo> scanPlayerOneCameras();
    std::vector<CameraInfo> scanASCOMCameras();
    std::vector<CameraInfo> scanSimulatorCameras();

    // Driver registry
    std::unordered_map<CameraDriverType, CreateCameraFunction> drivers_;
    
    // Cached camera information
    mutable std::vector<CameraInfo> cached_cameras_;
    mutable std::chrono::steady_clock::time_point last_scan_time_;
    static constexpr auto CACHE_DURATION = std::chrono::seconds(30);
    
    // Initialization flag
    bool initialized_ = false;
};

/**
 * @brief Convenience function to create a camera with automatic driver detection
 * @param name Camera name/identifier
 * @return Shared pointer to camera instance
 */
inline std::shared_ptr<AtomCamera> createCamera(const std::string& name) {
    return CameraFactory::getInstance().createCamera(name);
}

/**
 * @brief Convenience function to create a camera with specific driver type
 * @param type Driver type
 * @param name Camera name/identifier
 * @return Shared pointer to camera instance
 */
inline std::shared_ptr<AtomCamera> createCamera(CameraDriverType type, const std::string& name) {
    return CameraFactory::getInstance().createCamera(type, name);
}

/**
 * @brief Convenience function to scan for all available cameras
 * @return Vector of camera information structures
 */
inline std::vector<CameraInfo> scanCameras() {
    return CameraFactory::getInstance().scanForCameras();
}

} // namespace lithium::device
