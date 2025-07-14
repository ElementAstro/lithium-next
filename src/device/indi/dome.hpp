/*
 * dome.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-6-1

Description: INDI Dome Client Implementation

*************************************************/

#ifndef LITHIUM_CLIENT_INDI_DOME_HPP
#define LITHIUM_CLIENT_INDI_DOME_HPP

#include <libindi/baseclient.h>
#include <libindi/basedevice.h>

#include <atomic>
#include <mutex>
#include <optional>
#include <thread>

#include "device/template/dome.hpp"

// Forward declarations and type definitions
struct WeatherCondition {
    bool safe{true};
    double temperature{20.0};
    double humidity{50.0};
    double windSpeed{0.0};
    bool rainDetected{false};
};

struct WeatherLimits {
    double maxWindSpeed{15.0};    // m/s
    double minTemperature{-10.0}; // °C
    double maxTemperature{50.0};  // °C
    double maxHumidity{85.0};     // %
    bool rainProtection{true};
};

class INDIDome : public INDI::BaseClient, public AtomDome {
public:
    explicit INDIDome(std::string name);
    ~INDIDome() override = default;

    // Non-copyable, non-movable due to atomic members
    INDIDome(const INDIDome& other) = delete;
    INDIDome& operator=(const INDIDome& other) = delete;
    INDIDome(INDIDome&& other) = delete;
    INDIDome& operator=(INDIDome&& other) = delete;

    // Base device interface
    auto initialize() -> bool override;
    auto destroy() -> bool override;
    auto connect(const std::string &deviceName, int timeout, int maxRetry) -> bool override;
    auto disconnect() -> bool override;
    auto reconnect(int timeout, int maxRetry) -> bool;
    auto scan() -> std::vector<std::string> override;
    [[nodiscard]] auto isConnected() const -> bool override;

    virtual auto watchAdditionalProperty() -> bool;

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

protected:
    // INDI BaseClient virtual methods
    void newDevice(INDI::BaseDevice baseDevice) override;
    void removeDevice(INDI::BaseDevice baseDevice) override;
    void newProperty(INDI::Property property) override;
    void updateProperty(INDI::Property property) override;
    void removeProperty(INDI::Property property) override;
    void newMessage(INDI::BaseDevice baseDevice, int messageID) override;
    void serverConnected() override;
    void serverDisconnected(int exit_code) override;

private:
    // Internal state
    std::string device_name_;
    std::atomic<bool> is_connected_{false};
    std::atomic<bool> is_initialized_{false};
    std::atomic<bool> server_connected_{false};

    // Device reference
    INDI::BaseDevice base_device_;

    // Thread safety
    mutable std::recursive_mutex state_mutex_;
    mutable std::recursive_mutex device_mutex_;

    // Monitoring thread for continuous updates
    std::thread monitoring_thread_;
    std::atomic<bool> monitoring_thread_running_{false};

    // Current state caching
    std::atomic<double> current_azimuth_{0.0};
    std::atomic<double> target_azimuth_{0.0};
    std::atomic<double> rotation_speed_{0.0};
    std::atomic<bool> is_moving_{false};
    std::atomic<bool> is_parked_{false};
    std::atomic<int> shutter_state_{static_cast<int>(ShutterState::UNKNOWN)};

    // Weather safety
    std::atomic<bool> is_safe_to_operate_{true};
    std::string weather_status_{"Unknown"};

    // Weather monitoring
    bool weather_monitoring_enabled_{false};
    bool weather_safe_{true};
    WeatherLimits weather_limits_;
    bool auto_close_on_unsafe_weather_{true};

    // Home position
    double home_position_{-1.0}; // -1 means not set

    // Telescope coordination
    double current_telescope_az_{0.0};
    double current_telescope_alt_{0.0};

    // Backlash compensation
    double backlash_compensation_{0.0};
    bool backlash_enabled_{false};

    // Dome parameters
    DomeParameters dome_parameters_;

    // Statistics
    double total_rotation_{0.0};
    uint64_t shutter_operations_{0};

    // Internal methods
    void monitoringThreadFunction();
    auto waitForConnection(int timeout) -> bool;
    auto waitForProperty(const std::string& propertyName, int timeout) -> bool;
    void updateFromDevice();
    void handleDomeProperty(const INDI::Property& property);
    void updateAzimuthFromProperty(const INDI::PropertyNumber& property);
    void updateShutterFromProperty(const INDI::PropertySwitch& property);
    void updateParkingFromProperty(const INDI::PropertySwitch& property);
    void updateSpeedFromProperty(const INDI::PropertyNumber& property);

    // Helper methods
    void checkWeatherStatus();
    void updateDomeParameters();
    double normalizeAzimuth(double azimuth) override;

    // Property helpers
    auto getDomeAzimuthProperty() -> INDI::PropertyNumber;
    auto getDomeSpeedProperty() -> INDI::PropertyNumber;
    auto getDomeMotionProperty() -> INDI::PropertySwitch;
    auto getDomeParkProperty() -> INDI::PropertySwitch;
    auto getDomeShutterProperty() -> INDI::PropertySwitch;
    auto getDomeAbortProperty() -> INDI::PropertySwitch;
    auto getConnectionProperty() -> INDI::PropertySwitch;

    // Utility methods
    void logInfo(const std::string& message);
    void logWarning(const std::string& message);
    void logError(const std::string& message);

    // State conversion helpers
    auto convertShutterState(ISState state) -> ShutterState;
    auto convertToISState(bool value) -> ISState;
};

#endif  // LITHIUM_CLIENT_INDI_DOME_HPP
