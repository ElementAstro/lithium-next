#include "property_manager.hpp"
#include "hardware_interface.hpp"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <chrono>

namespace lithium::device::ascom::focuser::components {

PropertyManager::PropertyManager(std::shared_ptr<HardwareInterface> hardware)
    : hardware_(hardware)
    , config_{}
    , monitoring_active_(false)
{
}

PropertyManager::~PropertyManager() {
    stopMonitoring();
}

auto PropertyManager::initialize() -> bool {
    try {
        // Initialize default configuration
        config_.enableCaching = true;
        config_.enableValidation = true;
        config_.enableNotifications = true;
        config_.defaultCacheTimeout = std::chrono::milliseconds(5000);
        config_.propertyUpdateInterval = std::chrono::milliseconds(1000);
        config_.maxCacheSize = 100;
        config_.strictValidation = false;
        config_.logPropertyAccess = false;
        
        // Register standard ASCOM focuser properties
        registerStandardProperties();
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

auto PropertyManager::destroy() -> bool {
    try {
        stopMonitoring();
        clearPropertyCache();
        
        std::lock_guard<std::mutex> metadata_lock(metadata_mutex_);
        std::lock_guard<std::mutex> cache_lock(cache_mutex_);
        std::lock_guard<std::mutex> stats_lock(stats_mutex_);
        
        property_metadata_.clear();
        property_cache_.clear();
        property_stats_.clear();
        property_validators_.clear();
        
        return true;
    } catch (const std::exception& e) {
        return false;
    }
}

auto PropertyManager::setPropertyConfig(const PropertyConfig& config) -> void {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_ = config;
}

auto PropertyManager::getPropertyConfig() const -> PropertyConfig {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_;
}

auto PropertyManager::registerProperty(const std::string& name, const PropertyMetadata& metadata) -> bool {
    std::lock_guard<std::mutex> lock(metadata_mutex_);
    
    if (property_metadata_.find(name) != property_metadata_.end()) {
        return false; // Property already registered
    }
    
    property_metadata_[name] = metadata;
    
    // Initialize cache entry
    std::lock_guard<std::mutex> cache_lock(cache_mutex_);
    PropertyCacheEntry entry;
    entry.value = metadata.defaultValue;
    entry.timestamp = std::chrono::steady_clock::now();
    entry.isValid = false;
    entry.isDirty = false;
    entry.accessCount = 0;
    entry.lastAccess = std::chrono::steady_clock::now();
    
    property_cache_[name] = entry;
    
    // Initialize statistics
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    property_stats_[name] = PropertyStats{};
    
    return true;
}

auto PropertyManager::unregisterProperty(const std::string& name) -> bool {
    std::lock_guard<std::mutex> metadata_lock(metadata_mutex_);
    std::lock_guard<std::mutex> cache_lock(cache_mutex_);
    std::lock_guard<std::mutex> stats_lock(stats_mutex_);
    
    auto it = property_metadata_.find(name);
    if (it == property_metadata_.end()) {
        return false;
    }
    
    property_metadata_.erase(it);
    property_cache_.erase(name);
    property_stats_.erase(name);
    property_validators_.erase(name);
    
    return true;
}

auto PropertyManager::getPropertyMetadata(const std::string& name) -> std::optional<PropertyMetadata> {
    std::lock_guard<std::mutex> lock(metadata_mutex_);
    
    auto it = property_metadata_.find(name);
    if (it != property_metadata_.end()) {
        return it->second;
    }
    
    return std::nullopt;
}

auto PropertyManager::getRegisteredProperties() -> std::vector<std::string> {
    std::lock_guard<std::mutex> lock(metadata_mutex_);
    std::vector<std::string> properties;
    
    properties.reserve(property_metadata_.size());
    for (const auto& [name, metadata] : property_metadata_) {
        properties.push_back(name);
    }
    
    return properties;
}

auto PropertyManager::isPropertyRegistered(const std::string& name) -> bool {
    std::lock_guard<std::mutex> lock(metadata_mutex_);
    return property_metadata_.find(name) != property_metadata_.end();
}

auto PropertyManager::setPropertyMetadata(const std::string& name, const PropertyMetadata& metadata) -> bool {
    std::lock_guard<std::mutex> lock(metadata_mutex_);
    
    auto it = property_metadata_.find(name);
    if (it == property_metadata_.end()) {
        return false;
    }
    
    it->second = metadata;
    return true;
}

auto PropertyManager::getProperty(const std::string& name) -> std::optional<PropertyValue> {
    auto start_time = std::chrono::steady_clock::now();
    
    // Check if property is registered
    if (!isPropertyRegistered(name)) {
        return std::nullopt;
    }
    
    // Try to get from cache first
    if (config_.enableCaching) {
        auto cached_value = getCachedProperty(name);
        if (cached_value.has_value()) {
            auto duration = std::chrono::steady_clock::now() - start_time;
            updatePropertyStats(name, true, false, 
                              std::chrono::duration_cast<std::chrono::milliseconds>(duration), true);
            return cached_value;
        }
    }
    
    // Get from hardware
    auto value = getPropertyFromHardware(name);
    if (value.has_value()) {
        if (config_.enableCaching) {
            setCachedProperty(name, value.value());
        }
        
        auto duration = std::chrono::steady_clock::now() - start_time;
        updatePropertyStats(name, true, false, 
                          std::chrono::duration_cast<std::chrono::milliseconds>(duration), true);
        return value;
    }
    
    // Update statistics for failed read
    auto duration = std::chrono::steady_clock::now() - start_time;
    updatePropertyStats(name, true, false, 
                      std::chrono::duration_cast<std::chrono::milliseconds>(duration), false);
    
    return std::nullopt;
}

auto PropertyManager::setProperty(const std::string& name, const PropertyValue& value) -> bool {
    auto start_time = std::chrono::steady_clock::now();
    
    // Check if property is registered
    if (!isPropertyRegistered(name)) {
        return false;
    }
    
    // Check if property is read-only
    auto metadata = getPropertyMetadata(name);
    if (metadata && metadata->readOnly) {
        return false;
    }
    
    // Validate value
    if (config_.enableValidation && !validatePropertyValue(name, value)) {
        auto duration = std::chrono::steady_clock::now() - start_time;
        updatePropertyStats(name, false, true, 
                          std::chrono::duration_cast<std::chrono::milliseconds>(duration), false);
        return false;
    }
    
    // Get old value for notification
    auto old_value = getProperty(name);
    
    // Set to hardware
    bool success = setPropertyToHardware(name, value);
    
    if (success) {
        // Update cache
        if (config_.enableCaching) {
            setCachedProperty(name, value);
        }
        
        // Notify change
        if (config_.enableNotifications && old_value.has_value()) {
            notifyPropertyChange(name, old_value.value(), value);
        }
    }
    
    auto duration = std::chrono::steady_clock::now() - start_time;
    updatePropertyStats(name, false, true, 
                      std::chrono::duration_cast<std::chrono::milliseconds>(duration), success);
    
    return success;
}

auto PropertyManager::getProperties(const std::vector<std::string>& names) -> std::unordered_map<std::string, PropertyValue> {
    std::unordered_map<std::string, PropertyValue> result;
    
    for (const auto& name : names) {
        auto value = getProperty(name);
        if (value.has_value()) {
            result[name] = value.value();
        }
    }
    
    return result;
}

auto PropertyManager::setProperties(const std::unordered_map<std::string, PropertyValue>& properties) -> bool {
    bool all_success = true;
    
    for (const auto& [name, value] : properties) {
        if (!setProperty(name, value)) {
            all_success = false;
        }
    }
    
    return all_success;
}

auto PropertyManager::validateProperty(const std::string& name, const PropertyValue& value) -> bool {
    return validatePropertyValue(name, value);
}

auto PropertyManager::getValidationError(const std::string& name) -> std::string {
    // Return last validation error for property
    return ""; // Placeholder
}

auto PropertyManager::setPropertyValidator(const std::string& name, 
                                         std::function<bool(const PropertyValue&)> validator) -> bool {
    if (!isPropertyRegistered(name)) {
        return false;
    }
    
    property_validators_[name] = std::move(validator);
    return true;
}

auto PropertyManager::clearPropertyValidator(const std::string& name) -> bool {
    auto it = property_validators_.find(name);
    if (it != property_validators_.end()) {
        property_validators_.erase(it);
        return true;
    }
    
    return false;
}

auto PropertyManager::enablePropertyCaching(bool enable) -> void {
    std::lock_guard<std::mutex> lock(config_mutex_);
    config_.enableCaching = enable;
}

auto PropertyManager::isPropertyCachingEnabled() -> bool {
    std::lock_guard<std::mutex> lock(config_mutex_);
    return config_.enableCaching;
}

auto PropertyManager::clearPropertyCache() -> void {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    property_cache_.clear();
}

auto PropertyManager::clearPropertyFromCache(const std::string& name) -> void {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    property_cache_.erase(name);
}

auto PropertyManager::getCacheStats() -> std::unordered_map<std::string, PropertyStats> {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return property_stats_;
}

auto PropertyManager::getCacheHitRate() -> double {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    int total_cache_hits = 0;
    int total_cache_misses = 0;
    
    for (const auto& [name, stats] : property_stats_) {
        total_cache_hits += stats.cacheHits;
        total_cache_misses += stats.cacheMisses;
    }
    
    int total_accesses = total_cache_hits + total_cache_misses;
    if (total_accesses == 0) {
        return 0.0;
    }
    
    return static_cast<double>(total_cache_hits) / total_accesses;
}

auto PropertyManager::setCacheTimeout(const std::string& name, std::chrono::milliseconds timeout) -> bool {
    std::lock_guard<std::mutex> lock(metadata_mutex_);
    
    auto it = property_metadata_.find(name);
    if (it != property_metadata_.end()) {
        it->second.cacheTimeout = timeout;
        return true;
    }
    
    return false;
}

auto PropertyManager::synchronizeProperty(const std::string& name) -> bool {
    auto value = getPropertyFromHardware(name);
    if (value.has_value()) {
        setCachedProperty(name, value.value());
        return true;
    }
    
    return false;
}

auto PropertyManager::synchronizeAllProperties() -> bool {
    bool all_success = true;
    
    auto properties = getRegisteredProperties();
    for (const auto& name : properties) {
        if (!synchronizeProperty(name)) {
            all_success = false;
        }
    }
    
    return all_success;
}

auto PropertyManager::getPropertyFromHardware(const std::string& name) -> std::optional<PropertyValue> {
    try {
        // This would interface with the hardware layer
        // For now, return default values based on property name
        
        if (name == "Connected") {
            return PropertyValue(hardware_->isConnected());
        } else if (name == "IsMoving") {
            return PropertyValue(hardware_->isMoving());
        } else if (name == "Position") {
            auto pos = hardware_->getCurrentPosition();
            return pos.has_value() ? PropertyValue(pos.value()) : std::nullopt;
        } else if (name == "MaxStep") {
            return PropertyValue(hardware_->getMaxPosition());
        } else if (name == "MaxIncrement") {
            return PropertyValue(hardware_->getMaxIncrement());
        } else if (name == "StepSize") {
            return PropertyValue(hardware_->getStepSize());
        } else if (name == "TempCompAvailable") {
            return PropertyValue(hardware_->hasTemperatureSensor());
        } else if (name == "TempComp") {
            return PropertyValue(hardware_->getTemperatureCompensation());
        } else if (name == "Temperature") {
            auto temp = hardware_->getExternalTemperature();
            return temp.has_value() ? PropertyValue(temp.value()) : std::nullopt;
        } else if (name == "Absolute") {
            return PropertyValue(true); // Always absolute
        }
        
        return std::nullopt;
    } catch (const std::exception& e) {
        return std::nullopt;
    }
}

auto PropertyManager::setPropertyToHardware(const std::string& name, const PropertyValue& value) -> bool {
    try {
        // This would interface with the hardware layer
        // For now, handle known writable properties
        
        if (name == "Connected") {
            if (std::holds_alternative<bool>(value)) {
                return hardware_->setConnected(std::get<bool>(value));
            }
        } else if (name == "Position") {
            if (std::holds_alternative<int>(value)) {
                return hardware_->moveToPosition(std::get<int>(value));
            }
        } else if (name == "TempComp") {
            if (std::holds_alternative<bool>(value)) {
                return hardware_->setTemperatureCompensation(std::get<bool>(value));
            }
        }
        
        return false;
    } catch (const std::exception& e) {
        return false;
    }
}

auto PropertyManager::isPropertySynchronized(const std::string& name) -> bool {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = property_cache_.find(name);
    if (it != property_cache_.end()) {
        return it->second.isValid && !it->second.isDirty;
    }
    
    return false;
}

auto PropertyManager::markPropertyDirty(const std::string& name) -> void {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = property_cache_.find(name);
    if (it != property_cache_.end()) {
        it->second.isDirty = true;
    }
}

auto PropertyManager::startMonitoring() -> bool {
    if (monitoring_active_.load()) {
        return true; // Already monitoring
    }
    
    monitoring_active_.store(true);
    monitoring_thread_ = std::thread(&PropertyManager::monitoringLoop, this);
    
    return true;
}

auto PropertyManager::stopMonitoring() -> bool {
    if (!monitoring_active_.load()) {
        return true; // Already stopped
    }
    
    monitoring_active_.store(false);
    
    if (monitoring_thread_.joinable()) {
        monitoring_thread_.join();
    }
    
    return true;
}

auto PropertyManager::isMonitoring() -> bool {
    return monitoring_active_.load();
}

auto PropertyManager::addPropertyToMonitoring(const std::string& name) -> bool {
    if (!isPropertyRegistered(name)) {
        return false;
    }
    
    std::lock_guard<std::mutex> lock(monitoring_mutex_);
    
    auto it = std::find(monitored_properties_.begin(), monitored_properties_.end(), name);
    if (it == monitored_properties_.end()) {
        monitored_properties_.push_back(name);
    }
    
    return true;
}

auto PropertyManager::removePropertyFromMonitoring(const std::string& name) -> bool {
    std::lock_guard<std::mutex> lock(monitoring_mutex_);
    
    auto it = std::find(monitored_properties_.begin(), monitored_properties_.end(), name);
    if (it != monitored_properties_.end()) {
        monitored_properties_.erase(it);
        return true;
    }
    
    return false;
}

auto PropertyManager::getMonitoredProperties() -> std::vector<std::string> {
    std::lock_guard<std::mutex> lock(monitoring_mutex_);
    return monitored_properties_;
}

auto PropertyManager::registerStandardProperties() -> bool {
    // Register standard ASCOM focuser properties
    
    PropertyMetadata metadata;
    
    // Absolute property
    metadata.name = "Absolute";
    metadata.description = "True if the focuser is capable of absolute positioning";
    metadata.defaultValue = PropertyValue(true);
    metadata.readOnly = true;
    metadata.cached = true;
    registerProperty("Absolute", metadata);
    
    // Connected property
    metadata.name = "Connected";
    metadata.description = "Connection status";
    metadata.defaultValue = PropertyValue(false);
    metadata.readOnly = false;
    metadata.cached = false;
    registerProperty("Connected", metadata);
    
    // IsMoving property
    metadata.name = "IsMoving";
    metadata.description = "True if the focuser is currently moving";
    metadata.defaultValue = PropertyValue(false);
    metadata.readOnly = true;
    metadata.cached = false;
    registerProperty("IsMoving", metadata);
    
    // Position property
    metadata.name = "Position";
    metadata.description = "Current focuser position";
    metadata.defaultValue = PropertyValue(0);
    metadata.readOnly = false;
    metadata.cached = true;
    registerProperty("Position", metadata);
    
    // MaxStep property
    metadata.name = "MaxStep";
    metadata.description = "Maximum step position";
    metadata.defaultValue = PropertyValue(65535);
    metadata.readOnly = true;
    metadata.cached = true;
    registerProperty("MaxStep", metadata);
    
    // MaxIncrement property
    metadata.name = "MaxIncrement";
    metadata.description = "Maximum increment for a single move";
    metadata.defaultValue = PropertyValue(1000);
    metadata.readOnly = true;
    metadata.cached = true;
    registerProperty("MaxIncrement", metadata);
    
    // StepSize property
    metadata.name = "StepSize";
    metadata.description = "Step size in microns";
    metadata.defaultValue = PropertyValue(1.0);
    metadata.readOnly = true;
    metadata.cached = true;
    registerProperty("StepSize", metadata);
    
    // Temperature compensation properties
    metadata.name = "TempCompAvailable";
    metadata.description = "True if temperature compensation is available";
    metadata.defaultValue = PropertyValue(false);
    metadata.readOnly = true;
    metadata.cached = true;
    registerProperty("TempCompAvailable", metadata);
    
    metadata.name = "TempComp";
    metadata.description = "Temperature compensation enabled";
    metadata.defaultValue = PropertyValue(false);
    metadata.readOnly = false;
    metadata.cached = true;
    registerProperty("TempComp", metadata);
    
    metadata.name = "Temperature";
    metadata.description = "Current temperature";
    metadata.defaultValue = PropertyValue(0.0);
    metadata.readOnly = true;
    metadata.cached = true;
    registerProperty("Temperature", metadata);
    
    return true;
}

// Standard property implementations
auto PropertyManager::getAbsolute() -> bool {
    auto value = getPropertyAs<bool>("Absolute");
    return value.value_or(true);
}

auto PropertyManager::getIsMoving() -> bool {
    auto value = getPropertyAs<bool>("IsMoving");
    return value.value_or(false);
}

auto PropertyManager::getPosition() -> int {
    auto value = getPropertyAs<int>("Position");
    return value.value_or(0);
}

auto PropertyManager::getMaxStep() -> int {
    auto value = getPropertyAs<int>("MaxStep");
    return value.value_or(65535);
}

auto PropertyManager::getMaxIncrement() -> int {
    auto value = getPropertyAs<int>("MaxIncrement");
    return value.value_or(1000);
}

auto PropertyManager::getStepSize() -> double {
    auto value = getPropertyAs<double>("StepSize");
    return value.value_or(1.0);
}

auto PropertyManager::getTempCompAvailable() -> bool {
    auto value = getPropertyAs<bool>("TempCompAvailable");
    return value.value_or(false);
}

auto PropertyManager::getTempComp() -> bool {
    auto value = getPropertyAs<bool>("TempComp");
    return value.value_or(false);
}

auto PropertyManager::setTempComp(bool value) -> bool {
    return setPropertyAs<bool>("TempComp", value);
}

auto PropertyManager::getTemperature() -> double {
    auto value = getPropertyAs<double>("Temperature");
    return value.value_or(0.0);
}

auto PropertyManager::getConnected() -> bool {
    auto value = getPropertyAs<bool>("Connected");
    return value.value_or(false);
}

auto PropertyManager::setConnected(bool value) -> bool {
    return setPropertyAs<bool>("Connected", value);
}

auto PropertyManager::setPropertyChangeCallback(PropertyChangeCallback callback) -> void {
    property_change_callback_ = std::move(callback);
}

auto PropertyManager::setPropertyErrorCallback(PropertyErrorCallback callback) -> void {
    property_error_callback_ = std::move(callback);
}

auto PropertyManager::setPropertyValidationCallback(PropertyValidationCallback callback) -> void {
    property_validation_callback_ = std::move(callback);
}

auto PropertyManager::getPropertyStats() -> std::unordered_map<std::string, PropertyStats> {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return property_stats_;
}

auto PropertyManager::resetPropertyStats() -> void {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    for (auto& [name, stats] : property_stats_) {
        stats = PropertyStats{};
    }
}

auto PropertyManager::getPropertyAccessHistory(const std::string& name) -> std::vector<std::chrono::steady_clock::time_point> {
    // Implementation for access history
    return {}; // Placeholder
}

auto PropertyManager::exportPropertyData() -> std::string {
    // Implementation for JSON export
    return "{}"; // Placeholder
}

auto PropertyManager::importPropertyData(const std::string& json) -> bool {
    // Implementation for JSON import
    return false; // Placeholder
}

// Private methods

auto PropertyManager::getCachedProperty(const std::string& name) -> std::optional<PropertyValue> {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = property_cache_.find(name);
    if (it != property_cache_.end()) {
        if (isCacheValid(name)) {
            it->second.accessCount++;
            it->second.lastAccess = std::chrono::steady_clock::now();
            
            // Update statistics
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            auto stats_it = property_stats_.find(name);
            if (stats_it != property_stats_.end()) {
                stats_it->second.cacheHits++;
            }
            
            return it->second.value;
        } else {
            // Cache expired
            std::lock_guard<std::mutex> stats_lock(stats_mutex_);
            auto stats_it = property_stats_.find(name);
            if (stats_it != property_stats_.end()) {
                stats_it->second.cacheMisses++;
            }
        }
    }
    
    return std::nullopt;
}

auto PropertyManager::setCachedProperty(const std::string& name, const PropertyValue& value) -> void {
    std::lock_guard<std::mutex> lock(cache_mutex_);
    
    auto it = property_cache_.find(name);
    if (it != property_cache_.end()) {
        it->second.value = value;
        it->second.timestamp = std::chrono::steady_clock::now();
        it->second.isValid = true;
        it->second.isDirty = false;
    }
}

auto PropertyManager::isCacheValid(const std::string& name) -> bool {
    auto it = property_cache_.find(name);
    if (it == property_cache_.end()) {
        return false;
    }
    
    if (!it->second.isValid) {
        return false;
    }
    
    // Check timeout
    auto metadata = getPropertyMetadata(name);
    if (metadata) {
        auto now = std::chrono::steady_clock::now();
        auto elapsed = now - it->second.timestamp;
        return elapsed < metadata->cacheTimeout;
    }
    
    return false;
}

auto PropertyManager::updatePropertyCache(const std::string& name, const PropertyValue& value) -> void {
    setCachedProperty(name, value);
}

auto PropertyManager::updatePropertyStats(const std::string& name, bool isRead, bool isWrite, 
                                         std::chrono::milliseconds duration, bool success) -> void {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    
    auto it = property_stats_.find(name);
    if (it != property_stats_.end()) {
        auto& stats = it->second;
        
        if (isRead) {
            stats.totalReads++;
            stats.averageReadTime = std::chrono::milliseconds(
                (stats.averageReadTime.count() + duration.count()) / 2);
        }
        
        if (isWrite) {
            stats.totalWrites++;
            stats.averageWriteTime = std::chrono::milliseconds(
                (stats.averageWriteTime.count() + duration.count()) / 2);
        }
        
        if (!success) {
            stats.hardwareErrors++;
        }
        
        stats.lastAccess = std::chrono::steady_clock::now();
    }
}

auto PropertyManager::monitoringLoop() -> void {
    while (monitoring_active_.load()) {
        try {
            checkPropertyChanges();
            std::this_thread::sleep_for(config_.propertyUpdateInterval);
        } catch (const std::exception& e) {
            // Log error but continue monitoring
        }
    }
}

auto PropertyManager::checkPropertyChanges() -> void {
    std::lock_guard<std::mutex> lock(monitoring_mutex_);
    
    for (const auto& name : monitored_properties_) {
        auto current_value = getPropertyFromHardware(name);
        auto cached_value = getCachedProperty(name);
        
        if (current_value.has_value() && cached_value.has_value()) {
            if (!comparePropertyValues(current_value.value(), cached_value.value())) {
                // Property changed
                setCachedProperty(name, current_value.value());
                notifyPropertyChange(name, cached_value.value(), current_value.value());
            }
        }
    }
}

auto PropertyManager::validatePropertyValue(const std::string& name, const PropertyValue& value) -> bool {
    // Check custom validator first
    auto validator_it = property_validators_.find(name);
    if (validator_it != property_validators_.end()) {
        if (!validator_it->second(value)) {
            return false;
        }
    }
    
    // Check metadata constraints
    auto metadata = getPropertyMetadata(name);
    if (metadata) {
        // Check range constraints
        if (metadata->minValue.index() == value.index() && 
            metadata->maxValue.index() == value.index()) {
            
            auto clamped = clampPropertyValue(value, metadata->minValue, metadata->maxValue);
            if (!comparePropertyValues(value, clamped)) {
                return false;
            }
        }
    }
    
    return true;
}

auto PropertyManager::notifyPropertyChange(const std::string& name, const PropertyValue& oldValue, 
                                         const PropertyValue& newValue) -> void {
    if (property_change_callback_) {
        try {
            property_change_callback_(name, oldValue, newValue);
        } catch (const std::exception& e) {
            // Log error but continue
        }
    }
}

auto PropertyManager::notifyPropertyError(const std::string& name, const std::string& error) -> void {
    if (property_error_callback_) {
        try {
            property_error_callback_(name, error);
        } catch (const std::exception& e) {
            // Log error but continue
        }
    }
}

auto PropertyManager::notifyPropertyValidation(const std::string& name, const PropertyValue& value, bool isValid) -> void {
    if (property_validation_callback_) {
        try {
            property_validation_callback_(name, value, isValid);
        } catch (const std::exception& e) {
            // Log error but continue
        }
    }
}

auto PropertyManager::propertyValueToString(const PropertyValue& value) -> std::string {
    if (std::holds_alternative<bool>(value)) {
        return std::get<bool>(value) ? "true" : "false";
    } else if (std::holds_alternative<int>(value)) {
        return std::to_string(std::get<int>(value));
    } else if (std::holds_alternative<double>(value)) {
        return std::to_string(std::get<double>(value));
    } else if (std::holds_alternative<std::string>(value)) {
        return std::get<std::string>(value);
    }
    
    return "";
}

auto PropertyManager::stringToPropertyValue(const std::string& str, const PropertyValue& defaultValue) -> PropertyValue {
    // Try to parse based on the type of the default value
    if (std::holds_alternative<bool>(defaultValue)) {
        return PropertyValue(str == "true");
    } else if (std::holds_alternative<int>(defaultValue)) {
        try {
            return PropertyValue(std::stoi(str));
        } catch (const std::exception& e) {
            return defaultValue;
        }
    } else if (std::holds_alternative<double>(defaultValue)) {
        try {
            return PropertyValue(std::stod(str));
        } catch (const std::exception& e) {
            return defaultValue;
        }
    } else if (std::holds_alternative<std::string>(defaultValue)) {
        return PropertyValue(str);
    }
    
    return defaultValue;
}

auto PropertyManager::comparePropertyValues(const PropertyValue& a, const PropertyValue& b) -> bool {
    if (a.index() != b.index()) {
        return false;
    }
    
    if (std::holds_alternative<bool>(a)) {
        return std::get<bool>(a) == std::get<bool>(b);
    } else if (std::holds_alternative<int>(a)) {
        return std::get<int>(a) == std::get<int>(b);
    } else if (std::holds_alternative<double>(a)) {
        return std::abs(std::get<double>(a) - std::get<double>(b)) < 1e-9;
    } else if (std::holds_alternative<std::string>(a)) {
        return std::get<std::string>(a) == std::get<std::string>(b);
    }
    
    return false;
}

auto PropertyManager::clampPropertyValue(const PropertyValue& value, const PropertyValue& min, const PropertyValue& max) -> PropertyValue {
    if (std::holds_alternative<int>(value)) {
        int val = std::get<int>(value);
        int min_val = std::get<int>(min);
        int max_val = std::get<int>(max);
        return PropertyValue(std::clamp(val, min_val, max_val));
    } else if (std::holds_alternative<double>(value)) {
        double val = std::get<double>(value);
        double min_val = std::get<double>(min);
        double max_val = std::get<double>(max);
        return PropertyValue(std::clamp(val, min_val, max_val));
    }
    
    return value;
}

auto PropertyManager::initializeStandardProperty(const std::string& name, const PropertyValue& defaultValue,
                                                const std::string& description, const std::string& unit,
                                                bool readOnly) -> void {
    PropertyMetadata metadata;
    metadata.name = name;
    metadata.description = description;
    metadata.unit = unit;
    metadata.defaultValue = defaultValue;
    metadata.readOnly = readOnly;
    metadata.cached = true;
    metadata.cacheTimeout = config_.defaultCacheTimeout;
    
    registerProperty(name, metadata);
}

auto PropertyManager::getStandardPropertyValue(const std::string& name) -> std::optional<PropertyValue> {
    return getProperty(name);
}

auto PropertyManager::setStandardPropertyValue(const std::string& name, const PropertyValue& value) -> bool {
    return setProperty(name, value);
}

} // namespace lithium::device::ascom::focuser::components
