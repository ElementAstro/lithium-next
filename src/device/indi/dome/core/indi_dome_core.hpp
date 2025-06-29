/*
 * indi_dome_core.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_DEVICE_INDI_DOME_CORE_HPP
#define LITHIUM_DEVICE_INDI_DOME_CORE_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>

#include <string>
#include <thread>

#include "device/template/dome.hpp"

namespace lithium::device::indi {

// Forward declarations
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
 * @brief Core INDI dome implementation providing centralized state management
 *        and component coordination for modular dome control.
 */
class INDIDomeCore : public INDI::BaseClient {
public:
    explicit INDIDomeCore(std::string name);
    ~INDIDomeCore() override;

    // Non-copyable, non-movable
    INDIDomeCore(const INDIDomeCore&) = delete;
    INDIDomeCore& operator=(const INDIDomeCore&) = delete;
    INDIDomeCore(INDIDomeCore&&) = delete;
    INDIDomeCore& operator=(INDIDomeCore&&) = delete;

    // Core lifecycle
    auto initialize() -> bool;
    auto destroy() -> bool;
    auto connect(const std::string& deviceName, int timeout = 5000, int maxRetry = 3) -> bool;
    auto disconnect() -> bool;
    auto reconnect(int timeout = 5000, int maxRetry = 3) -> bool;

    // State queries
    [[nodiscard]] auto isConnected() const -> bool { return is_connected_.load(); }
    [[nodiscard]] auto isInitialized() const -> bool { return is_initialized_.load(); }
    [[nodiscard]] auto getDeviceName() const -> std::string;
    [[nodiscard]] auto getDevice() -> INDI::BaseDevice;

    // Component registration
    void registerPropertyManager(std::shared_ptr<PropertyManager> manager);
    void registerMotionController(std::shared_ptr<MotionController> controller);
    void registerShutterController(std::shared_ptr<ShutterController> controller);
    void registerParkingController(std::shared_ptr<ParkingController> controller);
    void registerTelescopeController(std::shared_ptr<TelescopeController> controller);
    void registerWeatherManager(std::shared_ptr<WeatherManager> manager);
    void registerStatisticsManager(std::shared_ptr<StatisticsManager> manager);
    void registerConfigurationManager(std::shared_ptr<ConfigurationManager> manager);
    void registerProfiler(std::shared_ptr<DomeProfiler> profiler);

    // Event callbacks - called by components to notify state changes
    using AzimuthCallback = std::function<void(double azimuth)>;
    using ShutterCallback = std::function<void(ShutterState state)>;
    using ParkCallback = std::function<void(bool parked)>;
    using MoveCompleteCallback = std::function<void(bool success, const std::string& message)>;
    using WeatherCallback = std::function<void(bool safe, const std::string& status)>;
    using ConnectionCallback = std::function<void(bool connected)>;

    void setAzimuthCallback(AzimuthCallback callback) { azimuth_callback_ = std::move(callback); }
    void setShutterCallback(ShutterCallback callback) { shutter_callback_ = std::move(callback); }
    void setParkCallback(ParkCallback callback) { park_callback_ = std::move(callback); }
    void setMoveCompleteCallback(MoveCompleteCallback callback) { move_complete_callback_ = std::move(callback); }
    void setWeatherCallback(WeatherCallback callback) { weather_callback_ = std::move(callback); }
    void setConnectionCallback(ConnectionCallback callback) { connection_callback_ = std::move(callback); }

    // Event notification methods - called by components
    void notifyAzimuthChange(double azimuth);
    void notifyShutterChange(ShutterState state);
    void notifyParkChange(bool parked);
    void notifyMoveComplete(bool success, const std::string& message = "");
    void notifyWeatherChange(bool safe, const std::string& status);
    void notifyConnectionChange(bool connected);

    // Device scanning support
    auto scanForDevices() -> std::vector<std::string>;
    auto getAvailableDevices() -> std::vector<std::string>;

    // Thread-safe state access
    [[nodiscard]] auto getCurrentAzimuth() const -> double { return current_azimuth_.load(); }
    [[nodiscard]] auto getTargetAzimuth() const -> double { return target_azimuth_.load(); }
    [[nodiscard]] auto isMoving() const -> bool { return is_moving_.load(); }
    [[nodiscard]] auto isParked() const -> bool { return is_parked_.load(); }
    [[nodiscard]] auto getShutterState() const -> ShutterState;
    [[nodiscard]] auto isSafeToOperate() const -> bool { return is_safe_to_operate_.load(); }

    // State setters (for component use)
    void setCurrentAzimuth(double azimuth) { current_azimuth_.store(azimuth); }
    void setTargetAzimuth(double azimuth) { target_azimuth_.store(azimuth); }
    void setMoving(bool moving) { is_moving_.store(moving); }
    void setParked(bool parked) { is_parked_.store(parked); }
    void setShutterState(ShutterState state);
    void setSafeToOperate(bool safe) { is_safe_to_operate_.store(safe); }

protected:
    // INDI BaseClient overrides
    void newDevice(INDI::BaseDevice baseDevice) override;
    void removeDevice(INDI::BaseDevice baseDevice) override;
    void newProperty(INDI::Property property) override;
    void updateProperty(INDI::Property property) override;
    void removeProperty(INDI::Property property) override;
    void newMessage(INDI::BaseDevice baseDevice, int messageID) override;
    void serverConnected() override;
    void serverDisconnected(int exit_code) override;

private:
    // Core state
    std::string device_name_;
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> is_initialized_{false};
    std::atomic<bool> server_connected_{false};

    // Device reference
    INDI::BaseDevice base_device_;

    // Thread safety
    mutable std::recursive_mutex state_mutex_;
    mutable std::recursive_mutex device_mutex_;

    // Monitoring thread
    std::thread monitoring_thread_;
    std::atomic<bool> monitoring_running_{false};

    // Component references
    std::weak_ptr<PropertyManager> property_manager_;
    std::weak_ptr<MotionController> motion_controller_;
    std::weak_ptr<ShutterController> shutter_controller_;
    std::weak_ptr<ParkingController> parking_controller_;
    std::weak_ptr<TelescopeController> telescope_controller_;
    std::weak_ptr<WeatherManager> weather_manager_;
    std::weak_ptr<StatisticsManager> statistics_manager_;
    std::weak_ptr<ConfigurationManager> configuration_manager_;
    std::weak_ptr<DomeProfiler> profiler_;

    // Cached state (atomic for thread-safe access)
    std::atomic<double> current_azimuth_{0.0};
    std::atomic<double> target_azimuth_{0.0};
    std::atomic<bool> is_moving_{false};
    std::atomic<bool> is_parked_{false};
    std::atomic<int> shutter_state_{static_cast<int>(ShutterState::UNKNOWN)};
    std::atomic<bool> is_safe_to_operate_{true};

    // Event callbacks
    AzimuthCallback azimuth_callback_;
    ShutterCallback shutter_callback_;
    ParkCallback park_callback_;
    MoveCompleteCallback move_complete_callback_;
    WeatherCallback weather_callback_;
    ConnectionCallback connection_callback_;

    // Internal methods
    void monitoringThreadFunction();
    auto waitForConnection(int timeout) -> bool;
    auto waitForDevice(int timeout) -> bool;
    void updateComponentsFromProperty(const INDI::Property& property);
    void distributePropertyToComponents(const INDI::Property& property);

    // Logging helpers
    void logInfo(const std::string& message) const;
    void logWarning(const std::string& message) const;
    void logError(const std::string& message) const;
};

} // namespace lithium::device::indi

#endif // LITHIUM_DEVICE_INDI_DOME_CORE_HPP
