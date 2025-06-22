#include "property_manager.hpp"
#include "hardware_interface.hpp"
#include <algorithm>

namespace lithium::device::asi::camera::components {

PropertyManager::PropertyManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(std::move(hardware)) {}

PropertyManager::~PropertyManager() = default;

bool PropertyManager::initialize() {
    if (!hardware_->isConnected()) {
        return false;
    }

    std::lock_guard<std::mutex> lock(propertiesMutex_);
    
    try {
        // Load property capabilities
        if (!loadPropertyCapabilities()) {
            return false;
        }
        
        // Load current property values
        if (!loadCurrentPropertyValues()) {
            return false;
        }
        
        initialized_ = true;
        return true;
    } catch (const std::exception&) {
        return false;
    }
}

bool PropertyManager::refresh() {
    if (!initialized_) {
        return initialize();
    }
    
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    return loadCurrentPropertyValues();
}

std::vector<PropertyManager::PropertyInfo> PropertyManager::getAllProperties() const {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    
    std::vector<PropertyInfo> result;
    result.reserve(properties_.size());
    
    for (const auto& [controlType, prop] : properties_) {
        result.push_back(prop);
    }
    
    return result;
}

std::optional<PropertyManager::PropertyInfo> PropertyManager::getProperty(ASI_CONTROL_TYPE controlType) const {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    
    auto it = properties_.find(controlType);
    if (it != properties_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

bool PropertyManager::hasProperty(ASI_CONTROL_TYPE controlType) const {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    return properties_.find(controlType) != properties_.end();
}

std::vector<ASI_CONTROL_TYPE> PropertyManager::getAvailableProperties() const {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    
    std::vector<ASI_CONTROL_TYPE> result;
    result.reserve(properties_.size());
    
    for (const auto& [controlType, prop] : properties_) {
        if (prop.isAvailable) {
            result.push_back(controlType);
        }
    }
    
    return result;
}

bool PropertyManager::setProperty(ASI_CONTROL_TYPE controlType, long value, bool isAuto) {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    
    auto it = properties_.find(controlType);
    if (it == properties_.end() || !it->second.isWritable) {
        return false;
    }
    
    auto& prop = it->second;
    
    // Validate value
    if (!validatePropertyValue(controlType, value)) {
        return false;
    }
    
    // Clamp value to valid range
    value = clampPropertyValue(controlType, value);
    
    // Apply to hardware - stub implementation
    
    // Update cached value
    updatePropertyValue(controlType, value, isAuto);
    
    return true;
}

bool PropertyManager::getProperty(ASI_CONTROL_TYPE controlType, long& value, bool& isAuto) const {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    
    auto it = properties_.find(controlType);
    if (it == properties_.end()) {
        return false;
    }
    
    value = it->second.currentValue;
    isAuto = it->second.isAuto;
    return true;
}

bool PropertyManager::setPropertyAuto(ASI_CONTROL_TYPE controlType, bool enable) {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    
    auto it = properties_.find(controlType);
    if (it == properties_.end() || !it->second.isAutoSupported) {
        return false;
    }
    
    // Apply to hardware - stub implementation
    
    // Update cached value
    it->second.isAuto = enable;
    notifyPropertyChange(controlType, it->second.currentValue, enable);
    
    return true;
}

bool PropertyManager::resetProperty(ASI_CONTROL_TYPE controlType) {
    std::lock_guard<std::mutex> lock(propertiesMutex_);
    
    auto it = properties_.find(controlType);
    if (it == properties_.end()) {
        return false;
    }
    
    return setProperty(controlType, it->second.defaultValue, false);
}

// Convenience methods for common properties
bool PropertyManager::setGain(int gain) {
    return setProperty(ASI_GAIN, static_cast<long>(gain));
}

int PropertyManager::getGain() const {
    long value;
    bool isAuto;
    if (getProperty(ASI_GAIN, value, isAuto)) {
        return static_cast<int>(value);
    }
    return -1;
}

std::pair<int, int> PropertyManager::getGainRange() const {
    auto prop = getProperty(ASI_GAIN);
    if (prop) {
        return {static_cast<int>(prop->minValue), static_cast<int>(prop->maxValue)};
    }
    return {0, 0};
}

bool PropertyManager::setAutoGain(bool enable) {
    return setPropertyAuto(ASI_GAIN, enable);
}

bool PropertyManager::isAutoGainEnabled() const {
    long value;
    bool isAuto;
    if (getProperty(ASI_GAIN, value, isAuto)) {
        return isAuto;
    }
    return false;
}

bool PropertyManager::setExposure(long exposureUs) {
    return setProperty(ASI_EXPOSURE, exposureUs);
}

long PropertyManager::getExposure() const {
    long value;
    bool isAuto;
    if (getProperty(ASI_EXPOSURE, value, isAuto)) {
        return value;
    }
    return -1;
}

std::pair<long, long> PropertyManager::getExposureRange() const {
    auto prop = getProperty(ASI_EXPOSURE);
    if (prop) {
        return {prop->minValue, prop->maxValue};
    }
    return {0, 0};
}

bool PropertyManager::setAutoExposure(bool enable) {
    return setPropertyAuto(ASI_EXPOSURE, enable);
}

bool PropertyManager::isAutoExposureEnabled() const {
    long value;
    bool isAuto;
    if (getProperty(ASI_EXPOSURE, value, isAuto)) {
        return isAuto;
    }
    return false;
}

bool PropertyManager::setOffset(int offset) {
    return setProperty(ASI_OFFSET, static_cast<long>(offset));
}

int PropertyManager::getOffset() const {
    long value;
    bool isAuto;
    if (getProperty(ASI_OFFSET, value, isAuto)) {
        return static_cast<int>(value);
    }
    return -1;
}

std::pair<int, int> PropertyManager::getOffsetRange() const {
    auto prop = getProperty(ASI_OFFSET);
    if (prop) {
        return {static_cast<int>(prop->minValue), static_cast<int>(prop->maxValue)};
    }
    return {0, 0};
}

// ROI Management
bool PropertyManager::setROI(const ROI& roi) {
    if (!validateROI(roi)) {
        return false;
    }
    
    // Apply to hardware - stub implementation
    
    currentROI_ = roi;
    notifyROIChange(roi);
    return true;
}

bool PropertyManager::setROI(int x, int y, int width, int height) {
    ROI roi{x, y, width, height};
    return setROI(roi);
}

PropertyManager::ROI PropertyManager::getROI() const {
    return currentROI_;
}

PropertyManager::ROI PropertyManager::getMaxROI() const {
    // Return maximum possible ROI - stub implementation
    return ROI{0, 0, 4096, 4096}; // Placeholder values
}

bool PropertyManager::validateROI(const ROI& roi) const {
    return roi.isValid() && isValidROI(roi);
}

bool PropertyManager::resetROI() {
    auto maxROI = getMaxROI();
    return setROI(maxROI);
}

// Binning Management
bool PropertyManager::setBinning(const BinningMode& binning) {
    if (!validateBinning(binning)) {
        return false;
    }
    
    // Apply to hardware - stub implementation
    
    currentBinning_ = binning;
    notifyBinningChange(binning);
    return true;
}

bool PropertyManager::setBinning(int binX, int binY) {
    BinningMode binning{binX, binY, ""};
    return setBinning(binning);
}

PropertyManager::BinningMode PropertyManager::getBinning() const {
    return currentBinning_;
}

std::vector<PropertyManager::BinningMode> PropertyManager::getSupportedBinning() const {
    // Return supported binning modes - stub implementation
    return {
        {1, 1, "1x1 (No Binning)"},
        {2, 2, "2x2 Binning"},
        {3, 3, "3x3 Binning"},
        {4, 4, "4x4 Binning"}
    };
}

bool PropertyManager::validateBinning(const BinningMode& binning) const {
    return isValidBinning(binning);
}

// Image Format Management
bool PropertyManager::setImageFormat(ASI_IMG_TYPE format) {
    // Apply to hardware - stub implementation
    currentImageFormat_ = format;
    return true;
}

ASI_IMG_TYPE PropertyManager::getImageFormat() const {
    return currentImageFormat_;
}

std::vector<PropertyManager::ImageFormat> PropertyManager::getSupportedImageFormats() const {
    // Return supported image formats - stub implementation
    return {
        {ASI_IMG_RAW8, "RAW8", "8-bit RAW format", 1, false},
        {ASI_IMG_RAW16, "RAW16", "16-bit RAW format", 2, false},
        {ASI_IMG_RGB24, "RGB24", "24-bit RGB format", 3, true}
    };
}

PropertyManager::ImageFormat PropertyManager::getImageFormatInfo(ASI_IMG_TYPE format) const {
    auto formats = getSupportedImageFormats();
    for (const auto& fmt : formats) {
        if (fmt.type == format) {
            return fmt;
        }
    }
    return {ASI_IMG_RAW16, "Unknown", "Unknown format", 0, false};
}

// Callbacks
void PropertyManager::setPropertyChangeCallback(PropertyChangeCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    propertyChangeCallback_ = std::move(callback);
}

void PropertyManager::setROIChangeCallback(ROIChangeCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    roiChangeCallback_ = std::move(callback);
}

void PropertyManager::setBinningChangeCallback(BinningChangeCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    binningChangeCallback_ = std::move(callback);
}

// Validation
bool PropertyManager::validatePropertyValue(ASI_CONTROL_TYPE controlType, long value) const {
    auto it = properties_.find(controlType);
    if (it == properties_.end()) {
        return false;
    }
    
    const auto& prop = it->second;
    return value >= prop.minValue && value <= prop.maxValue;
}

long PropertyManager::clampPropertyValue(ASI_CONTROL_TYPE controlType, long value) const {
    auto it = properties_.find(controlType);
    if (it == properties_.end()) {
        return value;
    }
    
    const auto& prop = it->second;
    return std::clamp(value, prop.minValue, prop.maxValue);
}

// Private methods
bool PropertyManager::loadPropertyCapabilities() {
    // Load property capabilities from hardware - stub implementation
    
    // Add common ASI camera properties
    PropertyInfo gain;
    gain.name = "Gain";
    gain.controlType = ASI_GAIN;
    gain.minValue = 0;
    gain.maxValue = 600;
    gain.defaultValue = 0;
    gain.currentValue = 0;
    gain.isAutoSupported = true;
    gain.isWritable = true;
    gain.isAvailable = true;
    properties_[ASI_GAIN] = gain;
    
    PropertyInfo exposure;
    exposure.name = "Exposure";
    exposure.controlType = ASI_EXPOSURE;
    exposure.minValue = 32;
    exposure.maxValue = 600000000;
    exposure.defaultValue = 100000;
    exposure.currentValue = 100000;
    exposure.isAutoSupported = true;
    exposure.isWritable = true;
    exposure.isAvailable = true;
    properties_[ASI_EXPOSURE] = exposure;
    
    PropertyInfo offset;
    offset.name = "Offset";
    offset.controlType = ASI_OFFSET;
    offset.minValue = 0;
    offset.maxValue = 255;
    offset.defaultValue = 8;
    offset.currentValue = 8;
    offset.isAutoSupported = false;
    offset.isWritable = true;
    offset.isAvailable = true;
    properties_[ASI_OFFSET] = offset;
    
    return true;
}

bool PropertyManager::loadCurrentPropertyValues() {
    // Load current values from hardware - stub implementation
    return true;
}

PropertyManager::PropertyInfo PropertyManager::createPropertyInfo(const ASI_CONTROL_CAPS& caps) const {
    PropertyInfo prop;
    prop.name = std::string(caps.Name);
    prop.description = std::string(caps.Description);
    prop.controlType = caps.ControlType;
    prop.minValue = caps.MinValue;
    prop.maxValue = caps.MaxValue;
    prop.defaultValue = caps.DefaultValue;
    prop.isAutoSupported = caps.IsAutoSupported == ASI_TRUE;
    prop.isWritable = caps.IsWritable == ASI_TRUE;
    prop.isAvailable = true;
    return prop;
}

void PropertyManager::updatePropertyValue(ASI_CONTROL_TYPE controlType, long value, bool isAuto) {
    auto it = properties_.find(controlType);
    if (it != properties_.end()) {
        it->second.currentValue = value;
        it->second.isAuto = isAuto;
        notifyPropertyChange(controlType, value, isAuto);
    }
}

void PropertyManager::notifyPropertyChange(ASI_CONTROL_TYPE controlType, long value, bool isAuto) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (propertyChangeCallback_) {
        propertyChangeCallback_(controlType, value, isAuto);
    }
}

void PropertyManager::notifyROIChange(const ROI& roi) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (roiChangeCallback_) {
        roiChangeCallback_(roi);
    }
}

void PropertyManager::notifyBinningChange(const BinningMode& binning) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    if (binningChangeCallback_) {
        binningChangeCallback_(binning);
    }
}

bool PropertyManager::isValidROI(const ROI& roi) const {
    auto maxROI = getMaxROI();
    return roi.x >= 0 && roi.y >= 0 && 
           roi.x + roi.width <= maxROI.width && 
           roi.y + roi.height <= maxROI.height;
}

bool PropertyManager::isValidBinning(const BinningMode& binning) const {
    auto supported = getSupportedBinning();
    return std::find(supported.begin(), supported.end(), binning) != supported.end();
}

} // namespace lithium::device::asi::camera::components

} // namespace lithium::device::asi::camera::components
