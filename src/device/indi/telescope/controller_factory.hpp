/*
 * controller_factory.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-23

Description: INDI Telescope Controller Factory

This factory provides convenient methods for creating and configuring
INDI telescope controllers with various component configurations.

*************************************************/

#pragma once

#include <memory>
#include <string>
#include <map>
#include <functional>

#include "telescope_controller.hpp"

namespace lithium::device::indi::telescope {

/**
 * @brief Configuration options for telescope controller creation
 */
struct TelescopeControllerConfig {
    std::string name = "INDITelescope";
    bool enableGuiding = true;
    bool enableTracking = true;
    bool enableParking = true;
    bool enableAlignment = true;
    bool enableAdvancedFeatures = true;
    
    // Component-specific configurations
    struct {
        int connectionTimeout = 30000;      // milliseconds
        int propertyTimeout = 5000;         // milliseconds
        bool enablePropertyCaching = true;
        bool enableAutoReconnect = true;
    } hardware;
    
    struct {
        double maxSlewSpeed = 5.0;          // degrees/sec
        double minSlewSpeed = 0.1;          // degrees/sec
        bool enableMotionLimits = true;
        bool enableSlewProgressTracking = true;
    } motion;
    
    struct {
        bool enableAutoTracking = true;
        double defaultTrackingRate = 15.041067;  // arcsec/sec (sidereal)
        bool enableTrackingStatistics = true;
        bool enablePEC = false;
    } tracking;
    
    struct {
        bool enableAutoPark = false;
        bool enableParkingConfirmation = true;
        double maxParkTime = 300.0;         // seconds
        bool saveParkPositions = true;
    } parking;
    
    struct {
        bool enableAutoAlignment = false;
        bool enableLocationSync = true;
        bool enableTimeSync = true;
        double coordinateUpdateRate = 1.0;  // Hz
    } coordinates;
    
    struct {
        double maxPulseDuration = 10000.0;  // milliseconds
        double minPulseDuration = 10.0;     // milliseconds
        bool enableGuideCalibration = true;
        bool enableGuideStatistics = true;
    } guiding;
};

/**
 * @brief Factory for creating INDI telescope controllers
 */
class ControllerFactory {
public:
    /**
     * @brief Create a standard telescope controller
     * @param name Telescope name
     * @return Unique pointer to telescope controller
     */
    static std::unique_ptr<INDITelescopeController> createStandardController(
        const std::string& name = "INDITelescope");

    /**
     * @brief Create a modular telescope controller with full configuration
     * @param config Configuration options
     * @return Unique pointer to telescope controller
     */
    static std::unique_ptr<INDITelescopeController> createModularController(
        const TelescopeControllerConfig& config = {});

    /**
     * @brief Create a minimal telescope controller (basic functionality only)
     * @param name Telescope name
     * @return Unique pointer to telescope controller
     */
    static std::unique_ptr<INDITelescopeController> createMinimalController(
        const std::string& name = "INDITelescope");

    /**
     * @brief Create a guiding-optimized telescope controller
     * @param name Telescope name
     * @return Unique pointer to telescope controller
     */
    static std::unique_ptr<INDITelescopeController> createGuidingController(
        const std::string& name = "INDITelescope");

    /**
     * @brief Create a telescope controller from configuration file
     * @param configFile Path to configuration file
     * @return Unique pointer to telescope controller
     */
    static std::unique_ptr<INDITelescopeController> createFromConfig(
        const std::string& configFile);

    /**
     * @brief Create a telescope controller with custom component factory
     * @param name Telescope name
     * @param componentFactory Custom component factory function
     * @return Unique pointer to telescope controller
     */
    static std::unique_ptr<INDITelescopeController> createCustomController(
        const std::string& name,
        std::function<void(INDITelescopeController&)> componentFactory);

    /**
     * @brief Get default configuration
     * @return Default telescope controller configuration
     */
    static TelescopeControllerConfig getDefaultConfig();

    /**
     * @brief Get minimal configuration
     * @return Minimal telescope controller configuration
     */
    static TelescopeControllerConfig getMinimalConfig();

    /**
     * @brief Get guiding-optimized configuration
     * @return Guiding-optimized telescope controller configuration
     */
    static TelescopeControllerConfig getGuidingConfig();

    /**
     * @brief Validate configuration
     * @param config Configuration to validate
     * @return true if configuration is valid, false otherwise
     */
    static bool validateConfig(const TelescopeControllerConfig& config);

    /**
     * @brief Load configuration from file
     * @param configFile Path to configuration file
     * @return Configuration loaded from file
     */
    static TelescopeControllerConfig loadConfigFromFile(const std::string& configFile);

    /**
     * @brief Save configuration to file
     * @param config Configuration to save
     * @param configFile Path to configuration file
     * @return true if save successful, false otherwise
     */
    static bool saveConfigToFile(const TelescopeControllerConfig& config, 
                                const std::string& configFile);

    /**
     * @brief Register telescope controller type
     * @param typeName Type name for the controller
     * @param factory Factory function for creating controllers of this type
     */
    static void registerControllerType(
        const std::string& typeName,
        std::function<std::unique_ptr<INDITelescopeController>(const TelescopeControllerConfig&)> factory);

    /**
     * @brief Create telescope controller by type name
     * @param typeName Registered type name
     * @param config Configuration for the controller
     * @return Unique pointer to telescope controller
     */
    static std::unique_ptr<INDITelescopeController> createByType(
        const std::string& typeName,
        const TelescopeControllerConfig& config = {});

    /**
     * @brief Get list of registered controller types
     * @return Vector of registered type names
     */
    static std::vector<std::string> getRegisteredTypes();

private:
    // Registry for custom controller types
    static std::map<std::string, std::function<std::unique_ptr<INDITelescopeController>(const TelescopeControllerConfig&)>> controllerRegistry_;

    // Internal helper methods
    static void applyHardwareConfig(INDITelescopeController& controller, 
                                   const TelescopeControllerConfig& config);
    static void applyMotionConfig(INDITelescopeController& controller, 
                                 const TelescopeControllerConfig& config);
    static void applyTrackingConfig(INDITelescopeController& controller, 
                                   const TelescopeControllerConfig& config);
    static void applyParkingConfig(INDITelescopeController& controller, 
                                  const TelescopeControllerConfig& config);
    static void applyCoordinateConfig(INDITelescopeController& controller, 
                                     const TelescopeControllerConfig& config);
    static void applyGuidingConfig(INDITelescopeController& controller, 
                                  const TelescopeControllerConfig& config);

    // Configuration validation helpers
    static bool validateHardwareConfig(const TelescopeControllerConfig& config);
    static bool validateMotionConfig(const TelescopeControllerConfig& config);
    static bool validateTrackingConfig(const TelescopeControllerConfig& config);
    static bool validateParkingConfig(const TelescopeControllerConfig& config);
    static bool validateCoordinateConfig(const TelescopeControllerConfig& config);
    static bool validateGuidingConfig(const TelescopeControllerConfig& config);
};

} // namespace lithium::device::indi::telescope
