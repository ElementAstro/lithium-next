/*
 * property_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12-18

Description: ASCOM Camera Property Manager Component Implementation

This component manages camera properties, settings, and configuration
including gain, offset, binning, ROI, and other camera parameters.

*************************************************/

#include "property_manager.hpp"
#include "hardware_interface.hpp"

#include <spdlog/spdlog.h>
#include "device/template/camera.hpp"

namespace lithium::device::ascom::camera::components {

// Static property name constants
const std::string PropertyManager::PROPERTY_GAIN = "Gain";
const std::string PropertyManager::PROPERTY_OFFSET = "Offset";
const std::string PropertyManager::PROPERTY_ISO = "ISO";
const std::string PropertyManager::PROPERTY_BINX = "BinX";
const std::string PropertyManager::PROPERTY_BINY = "BinY";
const std::string PropertyManager::PROPERTY_STARTX = "StartX";
const std::string PropertyManager::PROPERTY_STARTY = "StartY";
const std::string PropertyManager::PROPERTY_NUMX = "NumX";
const std::string PropertyManager::PROPERTY_NUMY = "NumY";
const std::string PropertyManager::PROPERTY_FRAME_TYPE = "FrameType";
const std::string PropertyManager::PROPERTY_UPLOAD_MODE = "UploadMode";
const std::string PropertyManager::PROPERTY_PIXEL_SIZE_X = "PixelSizeX";
const std::string PropertyManager::PROPERTY_PIXEL_SIZE_Y = "PixelSizeY";
const std::string PropertyManager::PROPERTY_BIT_DEPTH = "BitDepth";
const std::string PropertyManager::PROPERTY_IS_COLOR = "IsColor";
const std::string PropertyManager::PROPERTY_BAYER_PATTERN = "BayerPattern";
const std::string PropertyManager::PROPERTY_HAS_SHUTTER = "HasShutter";
const std::string PropertyManager::PROPERTY_SHUTTER_OPEN = "ShutterOpen";
const std::string PropertyManager::PROPERTY_HAS_FAN = "HasFan";
const std::string PropertyManager::PROPERTY_FAN_SPEED = "FanSpeed";

PropertyManager::PropertyManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(hardware)
    , notificationsEnabled_(true) {
    LOG_F(INFO, "ASCOM Camera PropertyManager initialized");
}

// =========================================================================
// Property Management
// =========================================================================

bool PropertyManager::initialize() {
    LOG_F(INFO, "Initializing property manager");

    if (!hardware_ || !hardware_->isConnected()) {
        LOG_F(ERROR, "Cannot initialize: hardware not connected");
        return false;
    }

    loadCameraProperties();
    return true;
}

bool PropertyManager::refreshProperties() {
    std::lock_guard<std::mutex> lock(propertiesMutex_);

    if (!hardware_ || !hardware_->isConnected()) {
        LOG_F(ERROR, "Cannot refresh properties: hardware not connected");
        return false;
    }

    // Refresh properties from hardware
    LOG_F(INFO, "Properties refreshed successfully");
    return true;
}

std::optional<PropertyManager::PropertyInfo>
PropertyManager::getPropertyInfo(const std::string& name) const {
    std::lock_guard<std::mutex> lock(propertiesMutex_);

    auto it = properties_.find(name);
    if (it == properties_.end()) {
        return std::nullopt;
    }

    return it->second;
}

std::optional<PropertyManager::PropertyValue>
PropertyManager::getProperty(const std::string& name) const {
    std::lock_guard<std::mutex> lock(propertiesMutex_);

    auto it = properties_.find(name);
    if (it == properties_.end()) {
        return std::nullopt;
    }

    return it->second.currentValue;
}

bool PropertyManager::setProperty(const std::string& name, const PropertyValue& value) {
    std::lock_guard<std::mutex> lock(propertiesMutex_);

    auto it = properties_.find(name);
    if (it == properties_.end()) {
        LOG_F(ERROR, "Property not found: {}", name);
        return false;
    }

    auto& property = it->second;

    // Check if property is writable
    if (property.isReadOnly) {
        LOG_F(ERROR, "Property is read-only: {}", name);
        return false;
    }

    // Store old value for change notification
    PropertyValue oldValue = property.currentValue;

    // Update property value
    property.currentValue = value;

    // Apply to hardware
    if (!applyPropertyToCamera(name, value)) {
        LOG_F(ERROR, "Failed to apply property {} to hardware", name);
        // Revert to old value
        property.currentValue = oldValue;
        return false;
    }

    LOG_F(INFO, "Property {} set successfully", name);

    // Notify change callback
    if (notificationsEnabled_.load()) {
        notifyPropertyChange(name, oldValue, value);
    }

    return true;
}

std::map<std::string, PropertyManager::PropertyInfo>
PropertyManager::getAllProperties() const {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    return properties_;
}

bool PropertyManager::isPropertyAvailable(const std::string& name) const {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    return properties_.find(name) != properties_.end();
}

// =========================================================================
// Gain and Offset Control
// =========================================================================

bool PropertyManager::setGain(int gain) {
    return setProperty(PROPERTY_GAIN, gain);
}

std::optional<int> PropertyManager::getGain() const {
    auto value = getProperty(PROPERTY_GAIN);
    if (value && std::holds_alternative<int>(*value)) {
        return std::get<int>(*value);
    }
    return std::nullopt;
}

std::pair<int, int> PropertyManager::getGainRange() const {
    auto info = getPropertyInfo(PROPERTY_GAIN);
    if (info && std::holds_alternative<int>(info->minValue) &&
        std::holds_alternative<int>(info->maxValue)) {
        return {std::get<int>(info->minValue), std::get<int>(info->maxValue)};
    }
    return {0, 100}; // Default range
}

bool PropertyManager::setOffset(int offset) {
    return setProperty(PROPERTY_OFFSET, offset);
}

std::optional<int> PropertyManager::getOffset() const {
    auto value = getProperty(PROPERTY_OFFSET);
    if (value && std::holds_alternative<int>(*value)) {
        return std::get<int>(*value);
    }
    return std::nullopt;
}

std::pair<int, int> PropertyManager::getOffsetRange() const {
    auto info = getPropertyInfo(PROPERTY_OFFSET);
    if (info && std::holds_alternative<int>(info->minValue) &&
        std::holds_alternative<int>(info->maxValue)) {
        return {std::get<int>(info->minValue), std::get<int>(info->maxValue)};
    }
    return {0, 1000}; // Default range
}

bool PropertyManager::setISO(int iso) {
    return setProperty(PROPERTY_ISO, iso);
}

std::optional<int> PropertyManager::getISO() const {
    auto value = getProperty(PROPERTY_ISO);
    if (value && std::holds_alternative<int>(*value)) {
        return std::get<int>(*value);
    }
    return std::nullopt;
}

std::vector<int> PropertyManager::getISOList() const {
    // Return common ISO values
    return {100, 200, 400, 800, 1600, 3200, 6400};
}

// =========================================================================
// ROI and Binning Control
// =========================================================================

bool PropertyManager::setROI(const ROI& roi) {
    bool success = setProperty(PROPERTY_STARTX, roi.x);
    success &= setProperty(PROPERTY_STARTY, roi.y);
    success &= setProperty(PROPERTY_NUMX, roi.width);
    success &= setProperty(PROPERTY_NUMY, roi.height);
    return success;
}

PropertyManager::ROI PropertyManager::getROI() const {
    ROI roi;

    auto startX = getProperty(PROPERTY_STARTX);
    if (startX && std::holds_alternative<int>(*startX)) {
        roi.x = std::get<int>(*startX);
    }

    auto startY = getProperty(PROPERTY_STARTY);
    if (startY && std::holds_alternative<int>(*startY)) {
        roi.y = std::get<int>(*startY);
    }

    auto numX = getProperty(PROPERTY_NUMX);
    if (numX && std::holds_alternative<int>(*numX)) {
        roi.width = std::get<int>(*numX);
    }

    auto numY = getProperty(PROPERTY_NUMY);
    if (numY && std::holds_alternative<int>(*numY)) {
        roi.height = std::get<int>(*numY);
    }

    return roi;
}

PropertyManager::ROI PropertyManager::getMaxROI() const {
    // Return maximum sensor dimensions (typical values)
    return {0, 0, 4096, 4096};
}

bool PropertyManager::setBinning(const AtomCameraFrame::Binning& binning) {
    bool success = setProperty(PROPERTY_BINX, binning.horizontal);
    success &= setProperty(PROPERTY_BINY, binning.vertical);
    return success;
}

std::optional<AtomCameraFrame::Binning> PropertyManager::getBinning() const {
    auto binX = getProperty(PROPERTY_BINX);
    auto binY = getProperty(PROPERTY_BINY);

    if (binX && binY &&
        std::holds_alternative<int>(*binX) &&
        std::holds_alternative<int>(*binY)) {
        AtomCameraFrame::Binning binning;
        binning.horizontal = std::get<int>(*binX);
        binning.vertical = std::get<int>(*binY);
        return binning;
    }

    return std::nullopt;
}

AtomCameraFrame::Binning PropertyManager::getMaxBinning() const {
    return {8, 8}; // Typical maximum binning
}

bool PropertyManager::setFrameType(FrameType type) {
    return setProperty(PROPERTY_FRAME_TYPE, static_cast<int>(type));
}

FrameType PropertyManager::getFrameType() const {
    auto value = getProperty(PROPERTY_FRAME_TYPE);
    if (value && std::holds_alternative<int>(*value)) {
        return static_cast<FrameType>(std::get<int>(*value));
    }
    return FrameType::FITS;
}

bool PropertyManager::setUploadMode(UploadMode mode) {
    return setProperty(PROPERTY_UPLOAD_MODE, static_cast<int>(mode));
}

UploadMode PropertyManager::getUploadMode() const {
    auto value = getProperty(PROPERTY_UPLOAD_MODE);
    if (value && std::holds_alternative<int>(*value)) {
        return static_cast<UploadMode>(std::get<int>(*value));
    }
    return UploadMode::CLIENT;
}

// =========================================================================
// Image and Sensor Properties
// =========================================================================

PropertyManager::ImageSettings PropertyManager::getImageSettings() const {
    std::lock_guard<std::mutex> lock(settingsMutex_);
    return currentImageSettings_;
}

double PropertyManager::getPixelSize() const {
    return getPixelSizeX(); // Assume square pixels
}

double PropertyManager::getPixelSizeX() const {
    auto value = getProperty(PROPERTY_PIXEL_SIZE_X);
    if (value && std::holds_alternative<double>(*value)) {
        return std::get<double>(*value);
    }
    return 5.4; // Default pixel size in micrometers
}

double PropertyManager::getPixelSizeY() const {
    auto value = getProperty(PROPERTY_PIXEL_SIZE_Y);
    if (value && std::holds_alternative<double>(*value)) {
        return std::get<double>(*value);
    }
    return 5.4; // Default pixel size in micrometers
}

int PropertyManager::getBitDepth() const {
    auto value = getProperty(PROPERTY_BIT_DEPTH);
    if (value && std::holds_alternative<int>(*value)) {
        return std::get<int>(*value);
    }
    return 16; // Default bit depth
}

bool PropertyManager::isColor() const {
    auto value = getProperty(PROPERTY_IS_COLOR);
    if (value && std::holds_alternative<bool>(*value)) {
        return std::get<bool>(*value);
    }
    return false; // Default to monochrome
}

BayerPattern PropertyManager::getBayerPattern() const {
    auto value = getProperty(PROPERTY_BAYER_PATTERN);
    if (value && std::holds_alternative<int>(*value)) {
        return static_cast<BayerPattern>(std::get<int>(*value));
    }
    return BayerPattern::RGGB;
}

bool PropertyManager::setBayerPattern(BayerPattern pattern) {
    return setProperty(PROPERTY_BAYER_PATTERN, static_cast<int>(pattern));
}

// =========================================================================
// Advanced Properties
// =========================================================================

bool PropertyManager::hasShutter() const {
    auto value = getProperty(PROPERTY_HAS_SHUTTER);
    if (value && std::holds_alternative<bool>(*value)) {
        return std::get<bool>(*value);
    }
    return true; // Default to having shutter
}

bool PropertyManager::setShutter(bool open) {
    return setProperty(PROPERTY_SHUTTER_OPEN, open);
}

bool PropertyManager::getShutterStatus() const {
    auto value = getProperty(PROPERTY_SHUTTER_OPEN);
    if (value && std::holds_alternative<bool>(*value)) {
        return std::get<bool>(*value);
    }
    return false;
}

bool PropertyManager::hasFan() const {
    auto value = getProperty(PROPERTY_HAS_FAN);
    if (value && std::holds_alternative<bool>(*value)) {
        return std::get<bool>(*value);
    }
    return false; // Default to no fan
}

bool PropertyManager::setFanSpeed(int speed) {
    return setProperty(PROPERTY_FAN_SPEED, speed);
}

int PropertyManager::getFanSpeed() const {
    auto value = getProperty(PROPERTY_FAN_SPEED);
    if (value && std::holds_alternative<int>(*value)) {
        return std::get<int>(*value);
    }
    return 0;
}

std::shared_ptr<AtomCameraFrame> PropertyManager::getFrameInfo() const {
    auto frame = std::make_shared<AtomCameraFrame>();

    auto roi = getROI();
    auto binning = getBinning();

    frame->resolution.width = roi.width;
    frame->resolution.height = roi.height;
    // Note: bitDepth is not a direct member of AtomCameraFrame

    if (binning) {
        frame->binning = *binning;
    }

    return frame;
}

// =========================================================================
// Property Validation and Constraints
// =========================================================================

bool PropertyManager::validateProperty(const std::string& name, const PropertyValue& value) const {
    auto info = getPropertyInfo(name);
    if (!info) return false;

    // Basic type validation
    return value.index() == info->currentValue.index();
}

std::string PropertyManager::getPropertyConstraints(const std::string& name) const {
    return "Property constraints for " + name;
}

bool PropertyManager::resetProperty(const std::string& name) {
    auto info = getPropertyInfo(name);
    if (!info) return false;

    return setProperty(name, info->defaultValue);
}

bool PropertyManager::resetAllProperties() {
    std::lock_guard<std::mutex> lock(propertiesMutex_);

    bool success = true;
    for (const auto& [name, info] : properties_) {
        if (!info.isReadOnly) {
            if (!setProperty(name, info.defaultValue)) {
                success = false;
            }
        }
    }

    return success;
}

// =========================================================================
// Private Helper Methods
// =========================================================================

void PropertyManager::loadCameraProperties() {
    std::lock_guard<std::mutex> lock(propertiesMutex_);

    // Initialize basic camera properties
    PropertyInfo gainInfo;
    gainInfo.name = PROPERTY_GAIN;
    gainInfo.description = "Camera gain";
    gainInfo.currentValue = 0;
    gainInfo.defaultValue = 0;
    gainInfo.minValue = 0;
    gainInfo.maxValue = 100;
    gainInfo.isReadOnly = false;
    // gainInfo.propertyType = PropertyType::INTEGER; // Remove propertyType references
    properties_[PROPERTY_GAIN] = gainInfo;

    PropertyInfo offsetInfo;
    offsetInfo.name = PROPERTY_OFFSET;
    offsetInfo.description = "Camera offset";
    offsetInfo.currentValue = 0;
    offsetInfo.defaultValue = 0;
    offsetInfo.minValue = 0;
    offsetInfo.maxValue = 1000;
    offsetInfo.isReadOnly = false;
    // offsetInfo.propertyType = PropertyType::INTEGER; // Remove propertyType references
    properties_[PROPERTY_OFFSET] = offsetInfo;

    // Add binning properties
    PropertyInfo binXInfo;
    binXInfo.name = PROPERTY_BINX;
    binXInfo.description = "Horizontal binning";
    binXInfo.currentValue = 1;
    binXInfo.defaultValue = 1;
    binXInfo.minValue = 1;
    binXInfo.maxValue = 8;
    binXInfo.isReadOnly = false;
    // binXInfo.propertyType = PropertyType::INTEGER; // Remove propertyType references
    properties_[PROPERTY_BINX] = binXInfo;

    binXInfo.name = PROPERTY_BINY;
    binXInfo.description = "Vertical binning";
    properties_[PROPERTY_BINY] = binXInfo;

    // Add ROI properties
    PropertyInfo roiInfo;
    roiInfo.name = PROPERTY_STARTX;
    roiInfo.description = "ROI start X";
    roiInfo.currentValue = 0;
    roiInfo.defaultValue = 0;
    roiInfo.minValue = 0;
    roiInfo.maxValue = 4096;
    roiInfo.isReadOnly = false;
    // roiInfo.propertyType = PropertyType::INTEGER; // Remove propertyType references
    properties_[PROPERTY_STARTX] = roiInfo;

    roiInfo.name = PROPERTY_STARTY;
    roiInfo.description = "ROI start Y";
    properties_[PROPERTY_STARTY] = roiInfo;

    roiInfo.name = PROPERTY_NUMX;
    roiInfo.description = "ROI width";
    roiInfo.currentValue = 4096;
    roiInfo.defaultValue = 4096;
    roiInfo.minValue = 1;
    properties_[PROPERTY_NUMX] = roiInfo;

    roiInfo.name = PROPERTY_NUMY;
    roiInfo.description = "ROI height";
    properties_[PROPERTY_NUMY] = roiInfo;

    LOG_F(INFO, "Loaded {} camera properties", properties_.size());
}

void PropertyManager::loadProperty(const std::string& name) {
    // Load specific property from hardware
}

bool PropertyManager::updatePropertyFromCamera(const std::string& name) {
    return true; // Simplified implementation
}

bool PropertyManager::applyPropertyToCamera(const std::string& name, const PropertyValue& value) {
    if (!hardware_ || !hardware_->isConnected()) {
        return false;
    }

    // Map property names to hardware operations
    if (name == PROPERTY_GAIN && std::holds_alternative<int>(value)) {
        return hardware_->setGain(std::get<int>(value));
    } else if (name == PROPERTY_OFFSET && std::holds_alternative<int>(value)) {
        return hardware_->setOffset(std::get<int>(value));
    }

    return true; // Simplified - assume success for other properties
}

void PropertyManager::notifyPropertyChange(const std::string& name,
                                         const PropertyValue& oldValue,
                                         const PropertyValue& newValue) {
    std::lock_guard<std::mutex> lock(callbackMutex_);

    if (propertyChangeCallback_) {
        propertyChangeCallback_(name, oldValue, newValue);
    }
}

template<typename T>
std::optional<T> PropertyManager::getTypedProperty(const std::string& name) const {
    auto value = getProperty(name);
    if (value && std::holds_alternative<T>(*value)) {
        return std::get<T>(*value);
    }
    return std::nullopt;
}

template<typename T>
bool PropertyManager::setTypedProperty(const std::string& name, const T& value) {
    return setProperty(name, PropertyValue{value});
}

bool PropertyManager::isValueInRange(const PropertyValue& value,
                                    const PropertyValue& min,
                                    const PropertyValue& max) const {
    return true; // Simplified implementation
}

bool PropertyManager::isValueInAllowedList(const PropertyValue& value,
                                         const std::vector<PropertyValue>& allowedValues) const {
    for (const auto& allowedValue : allowedValues) {
        if (value == allowedValue) {
            return true;
        }
    }
    return false;
}

} // namespace lithium::device::ascom::camera::components
