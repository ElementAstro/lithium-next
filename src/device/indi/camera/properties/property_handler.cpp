#include "property_handler.hpp"
#include "../core/indi_camera_core.hpp"

#include <spdlog/spdlog.h>
#include <algorithm>

namespace lithium::device::indi::camera {

PropertyHandler::PropertyHandler(INDICameraCore* core) 
    : ComponentBase(core) {
    spdlog::debug("Creating property handler");
}

auto PropertyHandler::initialize() -> bool {
    spdlog::debug("Initializing property handler");
    
    // Clear existing registrations
    propertyHandlers_.clear();
    propertyWatchers_.clear();
    availableProperties_.clear();
    
    return true;
}

auto PropertyHandler::destroy() -> bool {
    spdlog::debug("Destroying property handler");
    
    // Clear all registrations
    propertyHandlers_.clear();
    propertyWatchers_.clear();
    availableProperties_.clear();
    
    return true;
}

auto PropertyHandler::getComponentName() const -> std::string {
    return "PropertyHandler";
}

auto PropertyHandler::handleProperty(INDI::Property property) -> bool {
    if (!validateProperty(property)) {
        return false;
    }
    
    std::string propertyName = property.getName();
    
    // Check if we have a specific watcher for this property
    auto watcherIt = propertyWatchers_.find(propertyName);
    if (watcherIt != propertyWatchers_.end()) {
        watcherIt->second(property);
    }
    
    // Distribute to registered component handlers
    distributePropertyToComponents(property);
    
    return true;
}

auto PropertyHandler::registerPropertyHandler(const std::string& propertyName, 
                                            ComponentBase* component) -> void {
    if (!component) {
        spdlog::error("Cannot register null component for property: {}", propertyName);
        return;
    }
    
    auto& handlers = propertyHandlers_[propertyName];
    
    // Check if component is already registered
    auto it = std::find(handlers.begin(), handlers.end(), component);
    if (it == handlers.end()) {
        handlers.push_back(component);
        spdlog::debug("Registered component {} for property {}", 
                     component->getComponentName(), propertyName);
    }
}

auto PropertyHandler::unregisterPropertyHandler(const std::string& propertyName, 
                                               ComponentBase* component) -> void {
    auto it = propertyHandlers_.find(propertyName);
    if (it != propertyHandlers_.end()) {
        auto& handlers = it->second;
        handlers.erase(
            std::remove(handlers.begin(), handlers.end(), component),
            handlers.end()
        );
        
        // Remove entry if no handlers left
        if (handlers.empty()) {
            propertyHandlers_.erase(it);
        }
        
        spdlog::debug("Unregistered component {} from property {}", 
                     component ? component->getComponentName() : "null", propertyName);
    }
}

auto PropertyHandler::setPropertyNumber(const std::string& propertyName, double value) -> bool {
    if (!getCore()->isConnected()) {
        spdlog::error("Device not connected");
        return false;
    }
    
    try {
        auto device = getCore()->getDevice();
        INDI::PropertyNumber property = device.getProperty(propertyName);
        if (!property.isValid()) {
            spdlog::error("Property {} not found", propertyName);
            return false;
        }
        
        if (property.size() == 0) {
            spdlog::error("Property {} has no elements", propertyName);
            return false;
        }
        
        property[0].setValue(value);
        getCore()->sendNewProperty(property);
        
        spdlog::debug("Set property {} to {}", propertyName, value);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set property {}: {}", propertyName, e.what());
        return false;
    }
}

auto PropertyHandler::setPropertySwitch(const std::string& propertyName, 
                                       int index, bool state) -> bool {
    if (!getCore()->isConnected()) {
        spdlog::error("Device not connected");
        return false;
    }
    
    try {
        auto device = getCore()->getDevice();
        INDI::PropertySwitch property = device.getProperty(propertyName);
        if (!property.isValid()) {
            spdlog::error("Property {} not found", propertyName);
            return false;
        }
        
        if (index < 0 || index >= property.size()) {
            spdlog::error("Property {} index {} out of range [0, {})", 
                         propertyName, index, property.size());
            return false;
        }
        
        property[index].setState(state ? ISS_ON : ISS_OFF);
        getCore()->sendNewProperty(property);
        
        spdlog::debug("Set property {}[{}] to {}", propertyName, index, state);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set property {}: {}", propertyName, e.what());
        return false;
    }
}

auto PropertyHandler::setPropertyText(const std::string& propertyName, 
                                     const std::string& value) -> bool {
    if (!getCore()->isConnected()) {
        spdlog::error("Device not connected");
        return false;
    }
    
    try {
        auto device = getCore()->getDevice();
        INDI::PropertyText property = device.getProperty(propertyName);
        if (!property.isValid()) {
            spdlog::error("Property {} not found", propertyName);
            return false;
        }
        
        if (property.size() == 0) {
            spdlog::error("Property {} has no elements", propertyName);
            return false;
        }
        
        property[0].setText(value.c_str());
        getCore()->sendNewProperty(property);
        
        spdlog::debug("Set property {} to '{}'", propertyName, value);
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Failed to set property {}: {}", propertyName, e.what());
        return false;
    }
}

auto PropertyHandler::watchProperty(const std::string& propertyName, 
                                   std::function<void(INDI::Property)> callback) -> void {
    propertyWatchers_[propertyName] = std::move(callback);
    spdlog::debug("Watching property: {}", propertyName);
}

auto PropertyHandler::unwatchProperty(const std::string& propertyName) -> void {
    propertyWatchers_.erase(propertyName);
    spdlog::debug("Stopped watching property: {}", propertyName);
}

auto PropertyHandler::getPropertyList() const -> std::vector<std::string> {
    return availableProperties_;
}

auto PropertyHandler::isPropertyAvailable(const std::string& propertyName) const -> bool {
    return std::find(availableProperties_.begin(), availableProperties_.end(), propertyName) 
           != availableProperties_.end();
}

// Private methods
void PropertyHandler::updateAvailableProperties() {
    availableProperties_.clear();
    
    if (!getCore()->isConnected()) {
        return;
    }
    
    try {
        auto device = getCore()->getDevice();
        // Note: INDI doesn't provide a direct way to enumerate all properties
        // This would need to be populated as properties are discovered
        
        // Common INDI camera properties
        std::vector<std::string> commonProperties = {
            "CONNECTION", "CCD_EXPOSURE", "CCD_TEMPERATURE", "CCD_COOLER",
            "CCD_COOLER_POWER", "CCD_GAIN", "CCD_OFFSET", "CCD_FRAME",
            "CCD_BINNING", "CCD_INFO", "CCD_FRAME_TYPE", "CCD_SHUTTER",
            "CCD_FAN", "CCD_VIDEO_STREAM", "CCD1"
        };
        
        for (const auto& propName : commonProperties) {
            INDI::Property prop = device.getProperty(propName);
            if (prop.isValid()) {
                availableProperties_.push_back(propName);
            }
        }
        
    } catch (const std::exception& e) {
        spdlog::error("Failed to update available properties: {}", e.what());
    }
}

void PropertyHandler::distributePropertyToComponents(INDI::Property property) {
    std::string propertyName = property.getName();
    
    auto it = propertyHandlers_.find(propertyName);
    if (it != propertyHandlers_.end()) {
        for (auto* component : it->second) {
            if (component) {
                try {
                    component->handleProperty(property);
                } catch (const std::exception& e) {
                    spdlog::error("Error in component {} handling property {}: {}", 
                                 component->getComponentName(), propertyName, e.what());
                }
            }
        }
    }
}

auto PropertyHandler::validateProperty(INDI::Property property) -> bool {
    if (!property.isValid()) {
        spdlog::debug("Invalid property received");
        return false;
    }
    
    if (property.getDeviceName() != getCore()->getDeviceName()) {
        // Property is for a different device
        return false;
    }
    
    return true;
}

} // namespace lithium::device::indi::camera
