/*
 * controller.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-12-01

Description: ASCOM Dome Modular Controller

*************************************************/

#pragma once

#include <atomic>
#include <memory>
#include <optional>
#include <string>
#include <vector>
#include <chrono>

#include "device/template/dome.hpp"
#include "components/hardware_interface.hpp"
#include "components/azimuth_manager.hpp"
#include "components/shutter_manager.hpp"
#include "components/parking_manager.hpp"
#include "components/telescope_coordinator.hpp"
#include "components/weather_monitor.hpp"
#include "components/home_manager.hpp"
#include "components/configuration_manager.hpp"
#include "components/monitoring_system.hpp"
#include "components/alpaca_client.hpp"

#ifdef _WIN32
#include "components/com_helper.hpp"
#endif

namespace lithium::ascom::dome {

/**
 * @brief Modular ASCOM Dome Controller
 *
 * This class serves as the main orchestrator for the ASCOM dome system,
 * coordinating between various specialized components to provide a complete
 * dome control interface following the AtomDome interface.
 */
class ASCOMDomeController : public AtomDome {
public:
    explicit ASCOMDomeController(std::string name);
    ~ASCOMDomeController() override;

    // === Basic Device Operations ===
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string &deviceName, int timeout, int maxRetry) -> bool override;
    auto disconnect() -> bool override;
    auto scan() -> std::vector<std::string> override;
    auto isConnected() const -> bool override;

    // === Dome State ===
    auto isMoving() const -> bool override;
    auto isParked() const -> bool override;

    // === Azimuth Control ===
    auto getAzimuth() -> std::optional<double> override;
    auto setAzimuth(double azimuth) -> bool override;
    auto moveToAzimuth(double azimuth) -> bool override;
    auto rotateClockwise() -> bool override;
    auto rotateCounterClockwise() -> bool override;
    auto stopRotation() -> bool override;
    auto abortMotion() -> bool override;
    auto syncAzimuth(double azimuth) -> bool override;

    // === Parking ===
    auto park() -> bool override;
    auto unpark() -> bool override;
    auto getParkPosition() -> std::optional<double> override;
    auto setParkPosition(double azimuth) -> bool override;
    auto canPark() -> bool override;

    // === Shutter Control ===
    auto openShutter() -> bool override;
    auto closeShutter() -> bool override;
    auto abortShutter() -> bool override;
    auto getShutterState() -> ShutterState override;
    auto hasShutter() -> bool override;

    // === Speed Control ===
    auto getRotationSpeed() -> std::optional<double> override;
    auto setRotationSpeed(double speed) -> bool override;
    auto getMaxSpeed() -> double override;
    auto getMinSpeed() -> double override;

    // === Telescope Coordination ===
    auto followTelescope(bool enable) -> bool override;
    auto isFollowingTelescope() -> bool override;
    auto calculateDomeAzimuth(double telescopeAz, double telescopeAlt) -> double override;
    auto setTelescopePosition(double az, double alt) -> bool override;

    // === Home Position ===
    auto findHome() -> bool override;
    auto setHome() -> bool override;
    auto gotoHome() -> bool override;
    auto getHomePosition() -> std::optional<double> override;

    // === Backlash Compensation ===
    auto getBacklash() -> double override;
    auto setBacklash(double backlash) -> bool override;
    auto enableBacklashCompensation(bool enable) -> bool override;
    auto isBacklashCompensationEnabled() -> bool override;

    // === Weather Monitoring ===
    auto canOpenShutter() -> bool override;
    auto isSafeToOperate() -> bool override;
    auto getWeatherStatus() -> std::string override;

    // === Statistics ===
    auto getTotalRotation() -> double override;
    auto resetTotalRotation() -> bool override;
    auto getShutterOperations() -> uint64_t override;
    auto resetShutterOperations() -> bool override;

    // === Presets ===
    auto savePreset(int slot, double azimuth) -> bool override;
    auto loadPreset(int slot) -> bool override;
    auto getPreset(int slot) -> std::optional<double> override;
    auto deletePreset(int slot) -> bool override;

    // === ASCOM-Specific Methods ===
    auto getASCOMDriverInfo() -> std::optional<std::string>;
    auto getASCOMVersion() -> std::optional<std::string>;
    auto getASCOMInterfaceVersion() -> std::optional<int>;
    auto setASCOMClientID(const std::string &clientId) -> bool;
    auto getASCOMClientID() -> std::optional<std::string>;

    // === ASCOM Capabilities ===
    auto canFindHome() -> bool;
    auto canSetAzimuth() -> bool;
    auto canSetPark() -> bool;
    auto canSetShutter() -> bool;
    auto canSlave() -> bool;
    auto canSyncAzimuth() -> bool;

    // === Alpaca Discovery ===
    auto discoverAlpacaDevices() -> std::vector<std::string>;
    auto connectToAlpacaDevice(const std::string &host, int port, int deviceNumber) -> bool;
    auto disconnectFromAlpacaDevice() -> bool;

    // === COM Driver Connection (Windows only) ===
#ifdef _WIN32
    auto connectToCOMDriver(const std::string &progId) -> bool;
    auto disconnectFromCOMDriver() -> bool;
    auto showASCOMChooser() -> std::optional<std::string>;
#endif

    // === Component Access (for testing/advanced usage) ===
    auto getHardwareInterface() -> std::shared_ptr<components::HardwareInterface> { return hardware_interface_; }
    auto getAzimuthManager() -> std::shared_ptr<components::AzimuthManager> { return azimuth_manager_; }
    auto getShutterManager() -> std::shared_ptr<components::ShutterManager> { return shutter_manager_; }
    auto getParkingManager() -> std::shared_ptr<components::ParkingManager> { return parking_manager_; }
    auto getTelescopeCoordinator() -> std::shared_ptr<components::TelescopeCoordinator> { return telescope_coordinator_; }
    auto getWeatherMonitor() -> std::shared_ptr<components::WeatherMonitor> { return weather_monitor_; }
    auto getHomeManager() -> std::shared_ptr<components::HomeManager> { return home_manager_; }
    auto getConfigurationManager() -> std::shared_ptr<components::ConfigurationManager> { return configuration_manager_; }
    auto getMonitoringSystem() -> std::shared_ptr<components::MonitoringSystem> { return monitoring_system_; }

private:
    // === Component Instances ===
    std::shared_ptr<components::HardwareInterface> hardware_interface_;
    std::shared_ptr<components::AzimuthManager> azimuth_manager_;
    std::shared_ptr<components::ShutterManager> shutter_manager_;
    std::shared_ptr<components::ParkingManager> parking_manager_;
    std::shared_ptr<components::TelescopeCoordinator> telescope_coordinator_;
    std::shared_ptr<components::WeatherMonitor> weather_monitor_;
    std::shared_ptr<components::HomeManager> home_manager_;
    std::shared_ptr<components::ConfigurationManager> configuration_manager_;
    std::shared_ptr<components::MonitoringSystem> monitoring_system_;

    // === Connection-specific components ===
    std::shared_ptr<components::AlpacaClient> alpaca_client_;
#ifdef _WIN32
    std::shared_ptr<components::COMHelper> com_helper_;
#endif

    // === Connection Management ===
    enum class ConnectionType {
        COM_DRIVER,
        ALPACA_REST
    } connection_type_{ConnectionType::ALPACA_REST};

    // === State Variables ===
    std::atomic<bool> is_initialized_{false};
    std::atomic<bool> is_connected_{false};
    std::string device_name_;
    std::string client_id_{"Lithium-Next"};

    // === Statistics ===
    std::atomic<double> total_rotation_{0.0};

    // === Presets ===
    std::array<std::optional<double>, 10> presets_;

    // === Component initialization and cleanup ===
    auto initializeComponents() -> bool;
    auto destroyComponents() -> bool;
    auto validateComponentState() const -> bool;
    auto setupComponentCallbacks() -> void;
    auto applyConfiguration() -> void;

    // === Error handling ===
    auto handleComponentError(const std::string& component, const std::string& operation,
                             const std::exception& error) -> void;

    // === Configuration synchronization ===
    auto syncComponentConfigurations() -> bool;
};

} // namespace lithium::ascom::dome
