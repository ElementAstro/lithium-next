/*
 * modular_dome.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_DEVICE_INDI_DOME_MODULAR_DOME_HPP
#define LITHIUM_DEVICE_INDI_DOME_MODULAR_DOME_HPP

#include "device/template/dome.hpp"
#include <memory>
#include <vector>

namespace lithium::device::indi {

// Forward declarations
class INDIDomeCore;
class PropertyManager;
class MotionController;
class ShutterController;
class ParkingController;
class TelescopeController;
class WeatherManager;
class StatisticsManager;
class ConfigurationManager;
class DomeProfiler;

/**
 * @brief Modular INDI dome implementation providing comprehensive dome control
 *        through specialized components with full AtomDome interface coverage.
 */
class ModularINDIDome : public AtomDome {
public:
    explicit ModularINDIDome(std::string name);
    ~ModularINDIDome() override;

    // Non-copyable, non-movable
    ModularINDIDome(const ModularINDIDome&) = delete;
    ModularINDIDome& operator=(const ModularINDIDome&) = delete;
    ModularINDIDome(ModularINDIDome&&) = delete;
    ModularINDIDome& operator=(ModularINDIDome&&) = delete;

    // Base device interface
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string& deviceName, int timeout = 5000, int maxRetry = 3) -> bool override;
    auto disconnect() -> bool override;
    auto reconnect(int timeout, int maxRetry) -> bool;
    auto scan() -> std::vector<std::string> override;
    [[nodiscard]] auto isConnected() const -> bool override;

    // State queries
    auto isMoving() const -> bool override;
    auto isParked() const -> bool override;

    // Azimuth control
    auto getAzimuth() -> std::optional<double> override;
    auto setAzimuth(double azimuth) -> bool override;
    auto moveToAzimuth(double azimuth) -> bool override;
    auto rotateClockwise() -> bool override;
    auto rotateCounterClockwise() -> bool override;
    auto stopRotation() -> bool override;
    auto abortMotion() -> bool override;
    auto syncAzimuth(double azimuth) -> bool override;

    // Parking
    auto park() -> bool override;
    auto unpark() -> bool override;
    auto getParkPosition() -> std::optional<double> override;
    auto setParkPosition(double azimuth) -> bool override;
    auto canPark() -> bool override;

    // Shutter control
    auto openShutter() -> bool override;
    auto closeShutter() -> bool override;
    auto abortShutter() -> bool override;
    auto getShutterState() -> ShutterState override;
    auto hasShutter() -> bool override;

    // Speed control
    auto getRotationSpeed() -> std::optional<double> override;
    auto setRotationSpeed(double speed) -> bool override;
    auto getMaxSpeed() -> double override;
    auto getMinSpeed() -> double override;

    // Telescope coordination
    auto followTelescope(bool enable) -> bool override;
    auto isFollowingTelescope() -> bool override;
    auto calculateDomeAzimuth(double telescopeAz, double telescopeAlt) -> double override;
    auto setTelescopePosition(double az, double alt) -> bool override;

    // Home position
    auto findHome() -> bool override;
    auto setHome() -> bool override;
    auto gotoHome() -> bool override;
    auto getHomePosition() -> std::optional<double> override;

    // Backlash compensation
    auto getBacklash() -> double override;
    auto setBacklash(double backlash) -> bool override;
    auto enableBacklashCompensation(bool enable) -> bool override;
    auto isBacklashCompensationEnabled() -> bool override;

    // Weather monitoring
    auto canOpenShutter() -> bool override;
    auto isSafeToOperate() -> bool override;
    auto getWeatherStatus() -> std::string override;

    // Statistics
    auto getTotalRotation() -> double override;
    auto resetTotalRotation() -> bool override;
    auto getShutterOperations() -> uint64_t override;
    auto resetShutterOperations() -> bool override;

    // Presets
    auto savePreset(int slot, double azimuth) -> bool override;
    auto loadPreset(int slot) -> bool override;
    auto getPreset(int slot) -> std::optional<double> override;
    auto deletePreset(int slot) -> bool override;

    // Component access for advanced operations
    [[nodiscard]] auto getCore() const -> std::shared_ptr<INDIDomeCore> { return core_; }
    [[nodiscard]] auto getPropertyManager() const -> std::shared_ptr<PropertyManager> { return property_manager_; }
    [[nodiscard]] auto getMotionController() const -> std::shared_ptr<MotionController> { return motion_controller_; }
    [[nodiscard]] auto getShutterController() const -> std::shared_ptr<ShutterController> { return shutter_controller_; }
    [[nodiscard]] auto getParkingController() const -> std::shared_ptr<ParkingController> { return parking_controller_; }
    [[nodiscard]] auto getTelescopeController() const -> std::shared_ptr<TelescopeController> { return telescope_controller_; }
    [[nodiscard]] auto getWeatherManager() const -> std::shared_ptr<WeatherManager> { return weather_manager_; }
    [[nodiscard]] auto getStatisticsManager() const -> std::shared_ptr<StatisticsManager> { return statistics_manager_; }
    [[nodiscard]] auto getConfigurationManager() const -> std::shared_ptr<ConfigurationManager> { return configuration_manager_; }
    [[nodiscard]] auto getProfiler() const -> std::shared_ptr<DomeProfiler> { return profiler_; }

    // Advanced features
    auto enableAdvancedProfiling(bool enable) -> bool;
    auto getPerformanceMetrics() -> std::string;
    auto optimizePerformance() -> bool;
    auto runDiagnostics() -> bool override;

private:
    // Core components
    std::shared_ptr<INDIDomeCore> core_;
    std::shared_ptr<PropertyManager> property_manager_;
    std::shared_ptr<MotionController> motion_controller_;
    std::shared_ptr<ShutterController> shutter_controller_;
    std::shared_ptr<ParkingController> parking_controller_;
    std::shared_ptr<TelescopeController> telescope_controller_;
    std::shared_ptr<WeatherManager> weather_manager_;
    std::shared_ptr<StatisticsManager> statistics_manager_;
    std::shared_ptr<ConfigurationManager> configuration_manager_;
    std::shared_ptr<DomeProfiler> profiler_;

    // Initialization helpers
    auto initializeComponents() -> bool;
    auto registerComponents() -> bool;
    auto setupCallbacks() -> bool;
    auto cleanupComponents() -> bool;

    // Component validation
    [[nodiscard]] auto validateComponents() const -> bool;
    [[nodiscard]] auto areComponentsInitialized() const -> bool;

    // Error handling
    void handleComponentError(const std::string& component, const std::string& error);

    // Logging helpers
    void logInfo(const std::string& message) const;
    void logWarning(const std::string& message) const;
    void logError(const std::string& message) const;
};

} // namespace lithium::device::indi

#endif // LITHIUM_DEVICE_INDI_DOME_MODULAR_DOME_HPP
