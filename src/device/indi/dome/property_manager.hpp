/*
 * property_manager.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_DEVICE_INDI_DOME_PROPERTY_MANAGER_HPP
#define LITHIUM_DEVICE_INDI_DOME_PROPERTY_MANAGER_HPP

#include "component_base.hpp"

#include <libindi/indiapi.h>
#include <libindi/indiproperty.h>
#include <libindi/basedevice.h>

#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace lithium::device::indi {

/**
 * @brief Manages INDI properties for dome devices with robust error handling
 *        and type-safe property access patterns.
 */
class PropertyManager : public DomeComponentBase {
public:
    explicit PropertyManager(std::shared_ptr<INDIDomeCore> core);
    ~PropertyManager() override = default;

    // Component interface
    auto initialize() -> bool override;
    auto cleanup() -> bool override;
    void handlePropertyUpdate(const INDI::Property& property) override;

    // Property access methods with robust error handling
    [[nodiscard]] auto getNumberProperty(const std::string& name) const -> std::optional<INDI::PropertyNumber>;
    [[nodiscard]] auto getSwitchProperty(const std::string& name) const -> std::optional<INDI::PropertySwitch>;
    [[nodiscard]] auto getTextProperty(const std::string& name) const -> std::optional<INDI::PropertyText>;
    [[nodiscard]] auto getBLOBProperty(const std::string& name) const -> std::optional<INDI::PropertyBlob>;
    [[nodiscard]] auto getLightProperty(const std::string& name) const -> std::optional<INDI::PropertyLight>;

    // Typed property value getters
    [[nodiscard]] auto getNumberValue(const std::string& propertyName, const std::string& elementName) const -> std::optional<double>;
    [[nodiscard]] auto getSwitchState(const std::string& propertyName, const std::string& elementName) const -> std::optional<ISState>;
    [[nodiscard]] auto getTextValue(const std::string& propertyName, const std::string& elementName) const -> std::optional<std::string>;
    [[nodiscard]] auto getLightState(const std::string& propertyName, const std::string& elementName) const -> std::optional<IPState>;

    // Property setters with validation
    auto setNumberValue(const std::string& propertyName, const std::string& elementName, double value) -> bool;
    auto setSwitchState(const std::string& propertyName, const std::string& elementName, ISState state) -> bool;
    auto setTextValue(const std::string& propertyName, const std::string& elementName, const std::string& value) -> bool;

    // Dome-specific property accessors
    [[nodiscard]] auto getDomeAzimuthProperty() const -> std::optional<INDI::PropertyNumber>;
    [[nodiscard]] auto getDomeMotionProperty() const -> std::optional<INDI::PropertySwitch>;
    [[nodiscard]] auto getDomeShutterProperty() const -> std::optional<INDI::PropertySwitch>;
    [[nodiscard]] auto getDomeParkProperty() const -> std::optional<INDI::PropertySwitch>;
    [[nodiscard]] auto getDomeSpeedProperty() const -> std::optional<INDI::PropertyNumber>;
    [[nodiscard]] auto getDomeAbortProperty() const -> std::optional<INDI::PropertySwitch>;
    [[nodiscard]] auto getDomeHomeProperty() const -> std::optional<INDI::PropertySwitch>;
    [[nodiscard]] auto getDomeParametersProperty() const -> std::optional<INDI::PropertyNumber>;
    [[nodiscard]] auto getConnectionProperty() const -> std::optional<INDI::PropertySwitch>;

    // Dome value getters
    [[nodiscard]] auto getCurrentAzimuth() const -> std::optional<double>;
    [[nodiscard]] auto getTargetAzimuth() const -> std::optional<double>;
    [[nodiscard]] auto getCurrentSpeed() const -> std::optional<double>;
    [[nodiscard]] auto getTargetSpeed() const -> std::optional<double>;
    [[nodiscard]] auto getParkPosition() const -> std::optional<double>;
    [[nodiscard]] auto getHomePosition() const -> std::optional<double>;
    [[nodiscard]] auto getBacklash() const -> std::optional<double>;

    // Dome state queries
    [[nodiscard]] auto isConnected() const -> bool;
    [[nodiscard]] auto isMoving() const -> bool;
    [[nodiscard]] auto isParked() const -> bool;
    [[nodiscard]] auto isShutterOpen() const -> bool;
    [[nodiscard]] auto isShutterClosed() const -> bool;
    [[nodiscard]] auto canPark() const -> bool;
    [[nodiscard]] auto canSync() const -> bool;
    [[nodiscard]] auto canAbort() const -> bool;
    [[nodiscard]] auto hasShutter() const -> bool;
    [[nodiscard]] auto hasHome() const -> bool;
    [[nodiscard]] auto hasBacklash() const -> bool;

    // Property waiting utilities
    auto waitForProperty(const std::string& propertyName, int timeoutMs = 5000) const -> bool;
    auto waitForPropertyState(const std::string& propertyName, IPState state, int timeoutMs = 5000) const -> bool;

    // Property sending with error handling
    auto sendNewSwitch(const std::string& propertyName, const std::string& elementName, ISState state) -> bool;
    auto sendNewNumber(const std::string& propertyName, const std::string& elementName, double value) -> bool;
    auto sendNewText(const std::string& propertyName, const std::string& elementName, const std::string& value) -> bool;

    // Dome-specific convenience methods
    auto connectDevice() -> bool;
    auto disconnectDevice() -> bool;
    auto moveToAzimuth(double azimuth) -> bool;
    auto startRotation(bool clockwise) -> bool;
    auto stopRotation() -> bool;
    auto abortMotion() -> bool;
    auto parkDome() -> bool;
    auto unparkDome() -> bool;
    auto openShutter() -> bool;
    auto closeShutter() -> bool;
    auto abortShutter() -> bool;
    auto gotoHome() -> bool;
    auto findHome() -> bool;
    auto syncAzimuth(double azimuth) -> bool;
    auto setSpeed(double speed) -> bool;

    // Property listing
    [[nodiscard]] auto getAllProperties() const -> std::vector<std::string>;
    [[nodiscard]] auto getPropertyNames() const -> std::vector<std::string>;
    [[nodiscard]] auto getPropertyCount() const -> size_t;

    // Debug and diagnostics
    void dumpProperties() const;
    void dumpProperty(const std::string& name) const;
    [[nodiscard]] auto getPropertyInfo(const std::string& name) const -> std::string;

private:
    mutable std::recursive_mutex properties_mutex_;
    std::unordered_map<std::string, INDI::Property> cached_properties_;

    // Internal helpers
    [[nodiscard]] auto getDevice() const -> INDI::BaseDevice;
    [[nodiscard]] auto getProperty(const std::string& name) const -> std::optional<INDI::Property>;
    void cacheProperty(const INDI::Property& property);
    void removeCachedProperty(const std::string& name);

    // Validation helpers
    [[nodiscard]] auto validatePropertyAccess(const std::string& propertyName, const std::string& elementName) const -> bool;
    [[nodiscard]] auto validateNumberProperty(const INDI::PropertyNumber& prop, const std::string& elementName) const -> bool;
    [[nodiscard]] auto validateSwitchProperty(const INDI::PropertySwitch& prop, const std::string& elementName) const -> bool;
    [[nodiscard]] auto validateTextProperty(const INDI::PropertyText& prop, const std::string& elementName) const -> bool;

    // Dome-specific helpers
    [[nodiscard]] auto getDomeProperty(const std::string& name) const -> std::optional<INDI::Property>;
    [[nodiscard]] auto isValidAzimuth(double azimuth) const -> bool;
    [[nodiscard]] auto isValidSpeed(double speed) const -> bool;
};

} // namespace lithium::device::indi

#endif // LITHIUM_DEVICE_INDI_DOME_PROPERTY_MANAGER_HPP
