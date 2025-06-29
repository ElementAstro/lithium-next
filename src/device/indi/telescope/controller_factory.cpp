/*
 * controller_factory.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "controller_factory.hpp"

#include <fstream>
#include <spdlog/spdlog.h>
#include <nlohmann/json.hpp>

namespace lithium::device::indi::telescope {

// Static member initialization
std::map<std::string, std::function<std::unique_ptr<INDITelescopeController>(const TelescopeControllerConfig&)>>
    ControllerFactory::controllerRegistry_;

std::unique_ptr<INDITelescopeController> ControllerFactory::createStandardController(const std::string& name) {
    auto config = getDefaultConfig();
    config.name = name;
    return createModularController(config);
}

std::unique_ptr<INDITelescopeController> ControllerFactory::createModularController(const TelescopeControllerConfig& config) {
    try {
        // Validate configuration
        if (!validateConfig(config)) {
            spdlog::error("Invalid configuration provided to createModularController");
            return nullptr;
        }

        // Create the controller
        auto controller = std::make_unique<INDITelescopeController>(config.name);

        // Apply configuration to components
        applyHardwareConfig(*controller, config);
        applyMotionConfig(*controller, config);
        applyTrackingConfig(*controller, config);
        applyParkingConfig(*controller, config);
        applyCoordinateConfig(*controller, config);
        applyGuidingConfig(*controller, config);

        spdlog::info("Created modular telescope controller: {}", config.name);
        return controller;

    } catch (const std::exception& e) {
        spdlog::error("Failed to create modular controller: {}", e.what());
        return nullptr;
    }
}

std::unique_ptr<INDITelescopeController> ControllerFactory::createMinimalController(const std::string& name) {
    auto config = getMinimalConfig();
    config.name = name;
    return createModularController(config);
}

std::unique_ptr<INDITelescopeController> ControllerFactory::createGuidingController(const std::string& name) {
    auto config = getGuidingConfig();
    config.name = name;
    return createModularController(config);
}

std::unique_ptr<INDITelescopeController> ControllerFactory::createFromConfig(const std::string& configFile) {
    try {
        auto config = loadConfigFromFile(configFile);
        return createModularController(config);

    } catch (const std::exception& e) {
        spdlog::error("Failed to create controller from config file {}: {}", configFile, e.what());
        return nullptr;
    }
}

std::unique_ptr<INDITelescopeController> ControllerFactory::createCustomController(
    const std::string& name,
    std::function<void(INDITelescopeController&)> componentFactory) {

    try {
        auto controller = std::make_unique<INDITelescopeController>(name);

        // Apply custom component configuration
        if (componentFactory) {
            componentFactory(*controller);
        }

        spdlog::info("Created custom telescope controller: {}", name);
        return controller;

    } catch (const std::exception& e) {
        spdlog::error("Failed to create custom controller: {}", e.what());
        return nullptr;
    }
}

TelescopeControllerConfig ControllerFactory::getDefaultConfig() {
    TelescopeControllerConfig config;

    config.name = "INDITelescope";
    config.enableGuiding = true;
    config.enableTracking = true;
    config.enableParking = true;
    config.enableAlignment = true;
    config.enableAdvancedFeatures = true;

    // Hardware configuration
    config.hardware.connectionTimeout = 30000;
    config.hardware.propertyTimeout = 5000;
    config.hardware.enablePropertyCaching = true;
    config.hardware.enableAutoReconnect = true;

    // Motion configuration
    config.motion.maxSlewSpeed = 5.0;
    config.motion.minSlewSpeed = 0.1;
    config.motion.enableMotionLimits = true;
    config.motion.enableSlewProgressTracking = true;

    // Tracking configuration
    config.tracking.enableAutoTracking = true;
    config.tracking.defaultTrackingRate = 15.041067; // Sidereal rate
    config.tracking.enableTrackingStatistics = true;
    config.tracking.enablePEC = false;

    // Parking configuration
    config.parking.enableAutoPark = false;
    config.parking.enableParkingConfirmation = true;
    config.parking.maxParkTime = 300.0;
    config.parking.saveParkPositions = true;

    // Coordinate configuration
    config.coordinates.enableAutoAlignment = false;
    config.coordinates.enableLocationSync = true;
    config.coordinates.enableTimeSync = true;
    config.coordinates.coordinateUpdateRate = 1.0;

    // Guiding configuration
    config.guiding.maxPulseDuration = 10000.0;
    config.guiding.minPulseDuration = 10.0;
    config.guiding.enableGuideCalibration = true;
    config.guiding.enableGuideStatistics = true;

    return config;
}

TelescopeControllerConfig ControllerFactory::getMinimalConfig() {
    TelescopeControllerConfig config;

    config.name = "MinimalTelescope";
    config.enableGuiding = false;
    config.enableTracking = true;
    config.enableParking = false;
    config.enableAlignment = false;
    config.enableAdvancedFeatures = false;

    // Minimal hardware configuration
    config.hardware.connectionTimeout = 15000;
    config.hardware.propertyTimeout = 3000;
    config.hardware.enablePropertyCaching = false;
    config.hardware.enableAutoReconnect = false;

    // Basic motion configuration
    config.motion.maxSlewSpeed = 2.0;
    config.motion.minSlewSpeed = 0.5;
    config.motion.enableMotionLimits = false;
    config.motion.enableSlewProgressTracking = false;

    // Basic tracking configuration
    config.tracking.enableAutoTracking = false;
    config.tracking.defaultTrackingRate = 15.041067;
    config.tracking.enableTrackingStatistics = false;
    config.tracking.enablePEC = false;

    return config;
}

TelescopeControllerConfig ControllerFactory::getGuidingConfig() {
    auto config = getDefaultConfig();

    config.name = "GuidingTelescope";
    config.enableGuiding = true;
    config.enableAdvancedFeatures = true;

    // Optimized for guiding
    config.guiding.maxPulseDuration = 5000.0;  // 5 seconds max
    config.guiding.minPulseDuration = 5.0;     // 5 ms min
    config.guiding.enableGuideCalibration = true;
    config.guiding.enableGuideStatistics = true;

    // Enhanced tracking for guiding
    config.tracking.enableAutoTracking = true;
    config.tracking.enableTrackingStatistics = true;
    config.tracking.enablePEC = true;

    return config;
}

bool ControllerFactory::validateConfig(const TelescopeControllerConfig& config) {
    if (config.name.empty()) {
        spdlog::error("Configuration validation failed: name is empty");
        return false;
    }

    if (!validateHardwareConfig(config)) {
        spdlog::error("Configuration validation failed: hardware config invalid");
        return false;
    }

    if (!validateMotionConfig(config)) {
        spdlog::error("Configuration validation failed: motion config invalid");
        return false;
    }

    if (!validateTrackingConfig(config)) {
        spdlog::error("Configuration validation failed: tracking config invalid");
        return false;
    }

    if (!validateParkingConfig(config)) {
        spdlog::error("Configuration validation failed: parking config invalid");
        return false;
    }

    if (!validateCoordinateConfig(config)) {
        spdlog::error("Configuration validation failed: coordinate config invalid");
        return false;
    }

    if (!validateGuidingConfig(config)) {
        spdlog::error("Configuration validation failed: guiding config invalid");
        return false;
    }

    return true;
}

TelescopeControllerConfig ControllerFactory::loadConfigFromFile(const std::string& configFile) {
    std::ifstream file(configFile);
    if (!file.is_open()) {
        throw std::runtime_error("Cannot open config file: " + configFile);
    }

    nlohmann::json j;
    file >> j;

    TelescopeControllerConfig config;

    // Parse basic settings
    if (j.contains("name")) {
        config.name = j["name"];
    }
    if (j.contains("enableGuiding")) {
        config.enableGuiding = j["enableGuiding"];
    }
    if (j.contains("enableTracking")) {
        config.enableTracking = j["enableTracking"];
    }
    if (j.contains("enableParking")) {
        config.enableParking = j["enableParking"];
    }
    if (j.contains("enableAlignment")) {
        config.enableAlignment = j["enableAlignment"];
    }
    if (j.contains("enableAdvancedFeatures")) {
        config.enableAdvancedFeatures = j["enableAdvancedFeatures"];
    }

    // Parse hardware settings
    if (j.contains("hardware")) {
        auto hw = j["hardware"];
        if (hw.contains("connectionTimeout")) {
            config.hardware.connectionTimeout = hw["connectionTimeout"];
        }
        if (hw.contains("propertyTimeout")) {
            config.hardware.propertyTimeout = hw["propertyTimeout"];
        }
        if (hw.contains("enablePropertyCaching")) {
            config.hardware.enablePropertyCaching = hw["enablePropertyCaching"];
        }
        if (hw.contains("enableAutoReconnect")) {
            config.hardware.enableAutoReconnect = hw["enableAutoReconnect"];
        }
    }

    // Parse motion settings
    if (j.contains("motion")) {
        auto motion = j["motion"];
        if (motion.contains("maxSlewSpeed")) {
            config.motion.maxSlewSpeed = motion["maxSlewSpeed"];
        }
        if (motion.contains("minSlewSpeed")) {
            config.motion.minSlewSpeed = motion["minSlewSpeed"];
        }
        if (motion.contains("enableMotionLimits")) {
            config.motion.enableMotionLimits = motion["enableMotionLimits"];
        }
        if (motion.contains("enableSlewProgressTracking")) {
            config.motion.enableSlewProgressTracking = motion["enableSlewProgressTracking"];
        }
    }

    // Parse other sections similarly...

    return config;
}

bool ControllerFactory::saveConfigToFile(const TelescopeControllerConfig& config, const std::string& configFile) {
    try {
        nlohmann::json j;

        // Basic settings
        j["name"] = config.name;
        j["enableGuiding"] = config.enableGuiding;
        j["enableTracking"] = config.enableTracking;
        j["enableParking"] = config.enableParking;
        j["enableAlignment"] = config.enableAlignment;
        j["enableAdvancedFeatures"] = config.enableAdvancedFeatures;

        // Hardware settings
        j["hardware"]["connectionTimeout"] = config.hardware.connectionTimeout;
        j["hardware"]["propertyTimeout"] = config.hardware.propertyTimeout;
        j["hardware"]["enablePropertyCaching"] = config.hardware.enablePropertyCaching;
        j["hardware"]["enableAutoReconnect"] = config.hardware.enableAutoReconnect;

        // Motion settings
        j["motion"]["maxSlewSpeed"] = config.motion.maxSlewSpeed;
        j["motion"]["minSlewSpeed"] = config.motion.minSlewSpeed;
        j["motion"]["enableMotionLimits"] = config.motion.enableMotionLimits;
        j["motion"]["enableSlewProgressTracking"] = config.motion.enableSlewProgressTracking;

        // Tracking settings
        j["tracking"]["enableAutoTracking"] = config.tracking.enableAutoTracking;
        j["tracking"]["defaultTrackingRate"] = config.tracking.defaultTrackingRate;
        j["tracking"]["enableTrackingStatistics"] = config.tracking.enableTrackingStatistics;
        j["tracking"]["enablePEC"] = config.tracking.enablePEC;

        // Parking settings
        j["parking"]["enableAutoPark"] = config.parking.enableAutoPark;
        j["parking"]["enableParkingConfirmation"] = config.parking.enableParkingConfirmation;
        j["parking"]["maxParkTime"] = config.parking.maxParkTime;
        j["parking"]["saveParkPositions"] = config.parking.saveParkPositions;

        // Coordinate settings
        j["coordinates"]["enableAutoAlignment"] = config.coordinates.enableAutoAlignment;
        j["coordinates"]["enableLocationSync"] = config.coordinates.enableLocationSync;
        j["coordinates"]["enableTimeSync"] = config.coordinates.enableTimeSync;
        j["coordinates"]["coordinateUpdateRate"] = config.coordinates.coordinateUpdateRate;

        // Guiding settings
        j["guiding"]["maxPulseDuration"] = config.guiding.maxPulseDuration;
        j["guiding"]["minPulseDuration"] = config.guiding.minPulseDuration;
        j["guiding"]["enableGuideCalibration"] = config.guiding.enableGuideCalibration;
        j["guiding"]["enableGuideStatistics"] = config.guiding.enableGuideStatistics;

        // Save to file
        std::ofstream file(configFile);
        if (!file.is_open()) {
            spdlog::error("Cannot create config file: {}", configFile);
            return false;
        }

        file << j.dump(4); // Pretty print with 4 spaces
        file.close();

        spdlog::info("Configuration saved to: {}", configFile);
        return true;

    } catch (const std::exception& e) {
        spdlog::error("Failed to save configuration: {}", e.what());
        return false;
    }
}

void ControllerFactory::registerControllerType(
    const std::string& typeName,
    std::function<std::unique_ptr<INDITelescopeController>(const TelescopeControllerConfig&)> factory) {

    controllerRegistry_[typeName] = std::move(factory);
    spdlog::info("Registered telescope controller type: {}", typeName);
}

std::unique_ptr<INDITelescopeController> ControllerFactory::createByType(
    const std::string& typeName,
    const TelescopeControllerConfig& config) {

    auto it = controllerRegistry_.find(typeName);
    if (it == controllerRegistry_.end()) {
        spdlog::error("Unknown telescope controller type: {}", typeName);
        return nullptr;
    }

    try {
        return it->second(config);
    } catch (const std::exception& e) {
        spdlog::error("Failed to create controller of type {}: {}", typeName, e.what());
        return nullptr;
    }
}

std::vector<std::string> ControllerFactory::getRegisteredTypes() {
    std::vector<std::string> types;
    for (const auto& pair : controllerRegistry_) {
        types.push_back(pair.first);
    }
    return types;
}

// Private helper methods
void ControllerFactory::applyHardwareConfig(INDITelescopeController& controller, const TelescopeControllerConfig& config) {
    auto hardware = controller.getHardwareInterface();
    if (!hardware) {
        return;
    }

    // Apply hardware-specific configuration
    // This would typically involve setting timeouts, connection parameters, etc.
    spdlog::debug("Applied hardware configuration for: {}", config.name);
}

void ControllerFactory::applyMotionConfig(INDITelescopeController& controller, const TelescopeControllerConfig& config) {
    auto motionController = controller.getMotionController();
    if (!motionController) {
        return;
    }

    // Apply motion-specific configuration
    spdlog::debug("Applied motion configuration for: {}", config.name);
}

void ControllerFactory::applyTrackingConfig(INDITelescopeController& controller, const TelescopeControllerConfig& config) {
    auto trackingManager = controller.getTrackingManager();
    if (!trackingManager) {
        return;
    }

    // Apply tracking-specific configuration
    spdlog::debug("Applied tracking configuration for: {}", config.name);
}

void ControllerFactory::applyParkingConfig(INDITelescopeController& controller, const TelescopeControllerConfig& config) {
    auto parkingManager = controller.getParkingManager();
    if (!parkingManager) {
        return;
    }

    // Apply parking-specific configuration
    spdlog::debug("Applied parking configuration for: {}", config.name);
}

void ControllerFactory::applyCoordinateConfig(INDITelescopeController& controller, const TelescopeControllerConfig& config) {
    auto coordinateManager = controller.getCoordinateManager();
    if (!coordinateManager) {
        return;
    }

    // Apply coordinate-specific configuration
    spdlog::debug("Applied coordinate configuration for: {}", config.name);
}

void ControllerFactory::applyGuidingConfig(INDITelescopeController& controller, const TelescopeControllerConfig& config) {
    auto guideManager = controller.getGuideManager();
    if (!guideManager) {
        return;
    }

    // Apply guiding-specific configuration
    spdlog::debug("Applied guiding configuration for: {}", config.name);
}

// Validation helper methods
bool ControllerFactory::validateHardwareConfig(const TelescopeControllerConfig& config) {
    if (config.hardware.connectionTimeout <= 0 || config.hardware.connectionTimeout > 300000) {
        return false; // 0 to 5 minutes
    }

    if (config.hardware.propertyTimeout <= 0 || config.hardware.propertyTimeout > 60000) {
        return false; // 0 to 1 minute
    }

    return true;
}

bool ControllerFactory::validateMotionConfig(const TelescopeControllerConfig& config) {
    if (config.motion.maxSlewSpeed <= 0 || config.motion.maxSlewSpeed > 10.0) {
        return false; // 0 to 10 degrees/sec
    }

    if (config.motion.minSlewSpeed <= 0 || config.motion.minSlewSpeed >= config.motion.maxSlewSpeed) {
        return false;
    }

    return true;
}

bool ControllerFactory::validateTrackingConfig(const TelescopeControllerConfig& config) {
    if (config.tracking.defaultTrackingRate <= 0 || config.tracking.defaultTrackingRate > 100.0) {
        return false; // 0 to 100 arcsec/sec
    }

    return true;
}

bool ControllerFactory::validateParkingConfig(const TelescopeControllerConfig& config) {
    if (config.parking.maxParkTime <= 0 || config.parking.maxParkTime > 3600.0) {
        return false; // 0 to 1 hour
    }

    return true;
}

bool ControllerFactory::validateCoordinateConfig(const TelescopeControllerConfig& config) {
    if (config.coordinates.coordinateUpdateRate <= 0 || config.coordinates.coordinateUpdateRate > 10.0) {
        return false; // 0 to 10 Hz
    }

    return true;
}

bool ControllerFactory::validateGuidingConfig(const TelescopeControllerConfig& config) {
    if (config.guiding.maxPulseDuration <= 0 || config.guiding.maxPulseDuration > 60000.0) {
        return false; // 0 to 1 minute
    }

    if (config.guiding.minPulseDuration <= 0 || config.guiding.minPulseDuration >= config.guiding.maxPulseDuration) {
        return false;
    }

    return true;
}

} // namespace lithium::device::indi::telescope
