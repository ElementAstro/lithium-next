/*
 * property_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "property_manager.hpp"
#include "core/indi_dome_core.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include <thread>

namespace lithium::device::indi {

PropertyManager::PropertyManager(std::shared_ptr<INDIDomeCore> core)
    : DomeComponentBase(std::move(core), "PropertyManager") {
}

auto PropertyManager::initialize() -> bool {
    if (isInitialized()) {
        logWarning("Already initialized");
        return true;
    }

    auto core = getCore();
    if (!core) {
        logError("Core is null, cannot initialize");
        return false;
    }

    try {
        logInfo("Initializing property manager");
        setInitialized(true);
        return true;
    } catch (const std::exception& ex) {
        logError("Failed to initialize: " + std::string(ex.what()));
        return false;
    }
}

auto PropertyManager::cleanup() -> bool {
    if (!isInitialized()) {
        return true;
    }

    try {
        std::lock_guard<std::recursive_mutex> lock(properties_mutex_);
        cached_properties_.clear();
        setInitialized(false);
        logInfo("Property manager cleaned up");
        return true;
    } catch (const std::exception& ex) {
        logError("Failed to cleanup: " + std::string(ex.what()));
        return false;
    }
}

void PropertyManager::handlePropertyUpdate(const INDI::Property& property) {
    if (!isOurProperty(property)) {
        return;
    }

    cacheProperty(property);
    logInfo("Updated property: " + std::string(property.getName()));
}

// Property access methods
auto PropertyManager::getNumberProperty(const std::string& name) const -> std::optional<INDI::PropertyNumber> {
    auto prop = getProperty(name);
    if (!prop || prop->getType() != INDI_NUMBER) {
        return std::nullopt;
    }
    return *prop;  // Return the property directly, not a call to getNumber()
}

auto PropertyManager::getSwitchProperty(const std::string& name) const -> std::optional<INDI::PropertySwitch> {
    auto prop = getProperty(name);
    if (!prop || prop->getType() != INDI_SWITCH) {
        return std::nullopt;
    }
    return *prop;  // Return the property directly, not a call to getSwitch()
}

auto PropertyManager::getTextProperty(const std::string& name) const -> std::optional<INDI::PropertyText> {
    auto prop = getProperty(name);
    if (!prop || prop->getType() != INDI_TEXT) {
        return std::nullopt;
    }
    return *prop;  // Return the property directly, not a call to getText()
}

auto PropertyManager::getBLOBProperty(const std::string& name) const -> std::optional<INDI::PropertyBlob> {
    auto prop = getProperty(name);
    if (!prop || prop->getType() != INDI_BLOB) {
        return std::nullopt;
    }
    return *prop;  // Return the property directly, not a call to getBLOB()
}

auto PropertyManager::getLightProperty(const std::string& name) const -> std::optional<INDI::PropertyLight> {
    auto prop = getProperty(name);
    if (!prop || prop->getType() != INDI_LIGHT) {
        return std::nullopt;
    }
    return *prop;  // Return the property directly, not a call to getLight()
}

// Typed property value getters
auto PropertyManager::getNumberValue(const std::string& propertyName, const std::string& elementName) const -> std::optional<double> {
    auto prop = getNumberProperty(propertyName);
    if (!prop || !validateNumberProperty(*prop, elementName)) {
        return std::nullopt;
    }

    auto element = prop->findWidgetByName(elementName.c_str());
    if (!element) {
        return std::nullopt;
    }

    return element->getValue();
}

auto PropertyManager::getSwitchState(const std::string& propertyName, const std::string& elementName) const -> std::optional<ISState> {
    auto prop = getSwitchProperty(propertyName);
    if (!prop || !validateSwitchProperty(*prop, elementName)) {
        return std::nullopt;
    }

    auto element = prop->findWidgetByName(elementName.c_str());
    if (!element) {
        return std::nullopt;
    }

    return element->getState();
}

auto PropertyManager::getTextValue(const std::string& propertyName, const std::string& elementName) const -> std::optional<std::string> {
    auto prop = getTextProperty(propertyName);
    if (!prop || !validateTextProperty(*prop, elementName)) {
        return std::nullopt;
    }

    auto element = prop->findWidgetByName(elementName.c_str());
    if (!element) {
        return std::nullopt;
    }

    return std::string(element->getText());
}

auto PropertyManager::getLightState(const std::string& propertyName, const std::string& elementName) const -> std::optional<IPState> {
    auto prop = getLightProperty(propertyName);
    if (!prop) {
        return std::nullopt;
    }

    auto element = prop->findWidgetByName(elementName.c_str());
    if (!element) {
        return std::nullopt;
    }

    return element->getState();
}

// Property setters
auto PropertyManager::setNumberValue(const std::string& propertyName, const std::string& elementName, double value) -> bool {
    auto prop = getNumberProperty(propertyName);
    if (!prop || !validateNumberProperty(*prop, elementName)) {
        logError("Invalid number property: " + propertyName + "." + elementName);
        return false;
    }

    auto element = prop->findWidgetByName(elementName.c_str());
    if (!element) {
        logError("Element not found: " + elementName);
        return false;
    }

    element->setValue(value);
    
    auto core = getCore();
    if (!core) {
        logError("Core is null");
        return false;
    }

    try {
        core->sendNewProperty(*prop);
        return true;
    } catch (const std::exception& ex) {
        logError("Failed to send property: " + std::string(ex.what()));
        return false;
    }
}

auto PropertyManager::setSwitchState(const std::string& propertyName, const std::string& elementName, ISState state) -> bool {
    auto prop = getSwitchProperty(propertyName);
    if (!prop || !validateSwitchProperty(*prop, elementName)) {
        logError("Invalid switch property: " + propertyName + "." + elementName);
        return false;
    }

    prop->reset();
    auto element = prop->findWidgetByName(elementName.c_str());
    if (!element) {
        logError("Element not found: " + elementName);
        return false;
    }

    element->setState(state);
    
    auto core = getCore();
    if (!core) {
        logError("Core is null");
        return false;
    }

    try {
        core->sendNewProperty(*prop);
        return true;
    } catch (const std::exception& ex) {
        logError("Failed to send property: " + std::string(ex.what()));
        return false;
    }
}

auto PropertyManager::setTextValue(const std::string& propertyName, const std::string& elementName, const std::string& value) -> bool {
    auto prop = getTextProperty(propertyName);
    if (!prop || !validateTextProperty(*prop, elementName)) {
        logError("Invalid text property: " + propertyName + "." + elementName);
        return false;
    }

    auto element = prop->findWidgetByName(elementName.c_str());
    if (!element) {
        logError("Element not found: " + elementName);
        return false;
    }

    element->setText(value.c_str());
    
    auto core = getCore();
    if (!core) {
        logError("Core is null");
        return false;
    }

    try {
        core->sendNewProperty(*prop);
        return true;
    } catch (const std::exception& ex) {
        logError("Failed to send property: " + std::string(ex.what()));
        return false;
    }
}

// Dome-specific property accessors
auto PropertyManager::getDomeAzimuthProperty() const -> std::optional<INDI::PropertyNumber> {
    return getNumberProperty("ABS_DOME_POSITION");
}

auto PropertyManager::getDomeMotionProperty() const -> std::optional<INDI::PropertySwitch> {
    return getSwitchProperty("DOME_MOTION");
}

auto PropertyManager::getDomeShutterProperty() const -> std::optional<INDI::PropertySwitch> {
    return getSwitchProperty("DOME_SHUTTER");
}

auto PropertyManager::getDomeParkProperty() const -> std::optional<INDI::PropertySwitch> {
    return getSwitchProperty("DOME_PARK");
}

auto PropertyManager::getDomeSpeedProperty() const -> std::optional<INDI::PropertyNumber> {
    return getNumberProperty("DOME_SPEED");
}

auto PropertyManager::getDomeAbortProperty() const -> std::optional<INDI::PropertySwitch> {
    return getSwitchProperty("DOME_ABORT_MOTION");
}

auto PropertyManager::getDomeHomeProperty() const -> std::optional<INDI::PropertySwitch> {
    return getSwitchProperty("DOME_HOME");
}

auto PropertyManager::getDomeParametersProperty() const -> std::optional<INDI::PropertyNumber> {
    return getNumberProperty("DOME_PARAMS");
}

auto PropertyManager::getConnectionProperty() const -> std::optional<INDI::PropertySwitch> {
    return getSwitchProperty("CONNECTION");
}

// Dome value getters
auto PropertyManager::getCurrentAzimuth() const -> std::optional<double> {
    return getNumberValue("ABS_DOME_POSITION", "DOME_ABSOLUTE_POSITION");
}

auto PropertyManager::getTargetAzimuth() const -> std::optional<double> {
    return getNumberValue("ABS_DOME_POSITION", "DOME_ABSOLUTE_POSITION");
}

auto PropertyManager::getCurrentSpeed() const -> std::optional<double> {
    return getNumberValue("DOME_SPEED", "DOME_SPEED_VALUE");
}

auto PropertyManager::getTargetSpeed() const -> std::optional<double> {
    return getNumberValue("DOME_SPEED", "DOME_SPEED_VALUE");
}

auto PropertyManager::getParkPosition() const -> std::optional<double> {
    return getNumberValue("DOME_PARK_POSITION", "PARK_POSITION");
}

auto PropertyManager::getHomePosition() const -> std::optional<double> {
    return getNumberValue("DOME_HOME_POSITION", "HOME_POSITION");
}

auto PropertyManager::getBacklash() const -> std::optional<double> {
    return getNumberValue("DOME_BACKLASH", "DOME_BACKLASH_VALUE");
}

// Dome state queries
auto PropertyManager::isConnected() const -> bool {
    auto state = getSwitchState("CONNECTION", "CONNECT");
    return state && *state == ISS_ON;
}

auto PropertyManager::isMoving() const -> bool {
    auto cw_state = getSwitchState("DOME_MOTION", "DOME_CW");
    auto ccw_state = getSwitchState("DOME_MOTION", "DOME_CCW");
    
    return (cw_state && *cw_state == ISS_ON) || (ccw_state && *ccw_state == ISS_ON);
}

auto PropertyManager::isParked() const -> bool {
    auto state = getSwitchState("DOME_PARK", "PARK");
    return state && *state == ISS_ON;
}

auto PropertyManager::isShutterOpen() const -> bool {
    auto state = getSwitchState("DOME_SHUTTER", "SHUTTER_OPEN");
    return state && *state == ISS_ON;
}

auto PropertyManager::isShutterClosed() const -> bool {
    auto state = getSwitchState("DOME_SHUTTER", "SHUTTER_CLOSE");
    return state && *state == ISS_ON;
}

auto PropertyManager::canPark() const -> bool {
    return getDomeParkProperty().has_value();
}

auto PropertyManager::canSync() const -> bool {
    return getSwitchProperty("DOME_SYNC").has_value();
}

auto PropertyManager::canAbort() const -> bool {
    return getDomeAbortProperty().has_value();
}

auto PropertyManager::hasShutter() const -> bool {
    return getDomeShutterProperty().has_value();
}

auto PropertyManager::hasHome() const -> bool {
    return getDomeHomeProperty().has_value();
}

auto PropertyManager::hasBacklash() const -> bool {
    return getNumberProperty("DOME_BACKLASH").has_value();
}

// Property waiting utilities
auto PropertyManager::waitForProperty(const std::string& propertyName, int timeoutMs) const -> bool {
    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(timeoutMs);
    
    while (std::chrono::steady_clock::now() - start < timeout) {
        if (getProperty(propertyName)) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return false;
}

auto PropertyManager::waitForPropertyState(const std::string& propertyName, IPState state, int timeoutMs) const -> bool {
    auto start = std::chrono::steady_clock::now();
    auto timeout = std::chrono::milliseconds(timeoutMs);
    
    while (std::chrono::steady_clock::now() - start < timeout) {
        auto prop = getProperty(propertyName);
        if (prop && prop->getState() == state) {
            return true;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    return false;
}

// Property sending with error handling
auto PropertyManager::sendNewSwitch(const std::string& propertyName, const std::string& elementName, ISState state) -> bool {
    return setSwitchState(propertyName, elementName, state);
}

auto PropertyManager::sendNewNumber(const std::string& propertyName, const std::string& elementName, double value) -> bool {
    return setNumberValue(propertyName, elementName, value);
}

auto PropertyManager::sendNewText(const std::string& propertyName, const std::string& elementName, const std::string& value) -> bool {
    return setTextValue(propertyName, elementName, value);
}

// Dome-specific convenience methods
auto PropertyManager::connectDevice() -> bool {
    return setSwitchState("CONNECTION", "CONNECT", ISS_ON);
}

auto PropertyManager::disconnectDevice() -> bool {
    return setSwitchState("CONNECTION", "DISCONNECT", ISS_ON);
}

auto PropertyManager::moveToAzimuth(double azimuth) -> bool {
    if (!isValidAzimuth(azimuth)) {
        logError("Invalid azimuth value: " + std::to_string(azimuth));
        return false;
    }
    return setNumberValue("ABS_DOME_POSITION", "DOME_ABSOLUTE_POSITION", azimuth);
}

auto PropertyManager::startRotation(bool clockwise) -> bool {
    if (clockwise) {
        return setSwitchState("DOME_MOTION", "DOME_CW", ISS_ON);
    } else {
        return setSwitchState("DOME_MOTION", "DOME_CCW", ISS_ON);
    }
}

auto PropertyManager::stopRotation() -> bool {
    return setSwitchState("DOME_MOTION", "DOME_STOP", ISS_ON);
}

auto PropertyManager::abortMotion() -> bool {
    return setSwitchState("DOME_ABORT_MOTION", "ABORT", ISS_ON);
}

auto PropertyManager::parkDome() -> bool {
    return setSwitchState("DOME_PARK", "PARK", ISS_ON);
}

auto PropertyManager::unparkDome() -> bool {
    return setSwitchState("DOME_PARK", "UNPARK", ISS_ON);
}

auto PropertyManager::openShutter() -> bool {
    return setSwitchState("DOME_SHUTTER", "SHUTTER_OPEN", ISS_ON);
}

auto PropertyManager::closeShutter() -> bool {
    return setSwitchState("DOME_SHUTTER", "SHUTTER_CLOSE", ISS_ON);
}

auto PropertyManager::abortShutter() -> bool {
    return setSwitchState("DOME_SHUTTER", "SHUTTER_ABORT", ISS_ON);
}

auto PropertyManager::gotoHome() -> bool {
    return setSwitchState("DOME_HOME", "HOME_GO", ISS_ON);
}

auto PropertyManager::findHome() -> bool {
    return setSwitchState("DOME_HOME", "HOME_FIND", ISS_ON);
}

auto PropertyManager::syncAzimuth(double azimuth) -> bool {
    if (!isValidAzimuth(azimuth)) {
        logError("Invalid azimuth value: " + std::to_string(azimuth));
        return false;
    }
    return setNumberValue("DOME_SYNC", "DOME_SYNC_VALUE", azimuth);
}

auto PropertyManager::setSpeed(double speed) -> bool {
    if (!isValidSpeed(speed)) {
        logError("Invalid speed value: " + std::to_string(speed));
        return false;
    }
    return setNumberValue("DOME_SPEED", "DOME_SPEED_VALUE", speed);
}

// Property listing
auto PropertyManager::getAllProperties() const -> std::vector<std::string> {
    std::lock_guard<std::recursive_mutex> lock(properties_mutex_);
    std::vector<std::string> names;
    for (const auto& [name, prop] : cached_properties_) {
        names.push_back(name);
    }
    return names;
}

auto PropertyManager::getPropertyNames() const -> std::vector<std::string> {
    return getAllProperties();
}

auto PropertyManager::getPropertyCount() const -> size_t {
    std::lock_guard<std::recursive_mutex> lock(properties_mutex_);
    return cached_properties_.size();
}

// Debug and diagnostics
void PropertyManager::dumpProperties() const {
    std::lock_guard<std::recursive_mutex> lock(properties_mutex_);
    logInfo("Property dump (" + std::to_string(cached_properties_.size()) + " properties):");
    for (const auto& [name, prop] : cached_properties_) {
        logInfo("  " + name + " (" + std::to_string(prop.getType()) + ")");
    }
}

void PropertyManager::dumpProperty(const std::string& name) const {
    auto prop = getProperty(name);
    if (!prop) {
        logWarning("Property not found: " + name);
        return;
    }
    
    logInfo("Property: " + name);
    logInfo("  Type: " + std::to_string(prop->getType()));
    logInfo("  State: " + std::to_string(prop->getState()));
    logInfo("  Device: " + std::string(prop->getDeviceName()));
    logInfo("  Group: " + std::string(prop->getGroupName()));
    logInfo("  Label: " + std::string(prop->getLabel()));
}

auto PropertyManager::getPropertyInfo(const std::string& name) const -> std::string {
    auto prop = getProperty(name);
    if (!prop) {
        return "Property not found: " + name;
    }
    
    return "Property: " + name + " (Type: " + std::to_string(prop->getType()) + 
           ", State: " + std::to_string(prop->getState()) + ")";
}

// Private methods
auto PropertyManager::getDevice() const -> INDI::BaseDevice {
    auto core = getCore();
    if (!core) {
        return INDI::BaseDevice();
    }
    return core->getDevice();
}

auto PropertyManager::getProperty(const std::string& name) const -> std::optional<INDI::Property> {
    std::lock_guard<std::recursive_mutex> lock(properties_mutex_);
    
    auto it = cached_properties_.find(name);
    if (it != cached_properties_.end()) {
        return it->second;
    }
    
    // Try to get from device if not cached
    auto device = getDevice();
    if (device.isValid()) {
        auto prop = device.getProperty(name.c_str());
        if (prop.isValid()) {
            // Cache it for future use
            const_cast<PropertyManager*>(this)->cacheProperty(prop);
            return prop;
        }
    }
    
    return std::nullopt;
}

void PropertyManager::cacheProperty(const INDI::Property& property) {
    if (!property.isValid()) {
        return;
    }
    
    std::lock_guard<std::recursive_mutex> lock(properties_mutex_);
    cached_properties_[property.getName()] = property;
}

void PropertyManager::removeCachedProperty(const std::string& name) {
    std::lock_guard<std::recursive_mutex> lock(properties_mutex_);
    cached_properties_.erase(name);
}

// Validation helpers
auto PropertyManager::validatePropertyAccess(const std::string& propertyName, const std::string& elementName) const -> bool {
    if (propertyName.empty() || elementName.empty()) {
        logError("Empty property or element name");
        return false;
    }
    return true;
}

auto PropertyManager::validateNumberProperty(const INDI::PropertyNumber& prop, const std::string& elementName) const -> bool {
    if (!prop.isValid()) {
        return false;
    }
    
    auto element = prop.findWidgetByName(elementName.c_str());
    return element != nullptr;
}

auto PropertyManager::validateSwitchProperty(const INDI::PropertySwitch& prop, const std::string& elementName) const -> bool {
    if (!prop.isValid()) {
        return false;
    }
    
    auto element = prop.findWidgetByName(elementName.c_str());
    return element != nullptr;
}

auto PropertyManager::validateTextProperty(const INDI::PropertyText& prop, const std::string& elementName) const -> bool {
    if (!prop.isValid()) {
        return false;
    }
    
    auto element = prop.findWidgetByName(elementName.c_str());
    return element != nullptr;
}

auto PropertyManager::getDomeProperty(const std::string& name) const -> std::optional<INDI::Property> {
    return getProperty(name);
}

auto PropertyManager::isValidAzimuth(double azimuth) const -> bool {
    return azimuth >= 0.0 && azimuth < 360.0;
}

auto PropertyManager::isValidSpeed(double speed) const -> bool {
    return speed >= 0.0 && speed <= 100.0; // Assuming percentage-based speed
}

} // namespace lithium::device::indi
