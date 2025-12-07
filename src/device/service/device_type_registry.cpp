/*
 * device_type_registry.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-12

Description: Dynamic device type registry implementation

**************************************************/

#include "device_type_registry.hpp"

#include "atom/log/spdlog_logger.hpp"

namespace lithium::device {

// ============================================================================
// DeviceCapabilities
// ============================================================================

auto DeviceCapabilities::toJson() const -> nlohmann::json {
    nlohmann::json j;
    j["canConnect"] = canConnect;
    j["canDisconnect"] = canDisconnect;
    j["canAbort"] = canAbort;
    j["canPark"] = canPark;
    j["canHome"] = canHome;
    j["canSync"] = canSync;
    j["canSlew"] = canSlew;
    j["canTrack"] = canTrack;
    j["canGuide"] = canGuide;
    j["canCool"] = canCool;
    j["canFocus"] = canFocus;
    j["canRotate"] = canRotate;
    j["hasShutter"] = hasShutter;
    j["hasTemperature"] = hasTemperature;
    j["hasPosition"] = hasPosition;
    j["supportsAsync"] = supportsAsync;
    j["supportsEvents"] = supportsEvents;
    j["supportsProperties"] = supportsProperties;
    j["supportsBatch"] = supportsBatch;
    return j;
}

auto DeviceCapabilities::fromJson(const nlohmann::json& j)
    -> DeviceCapabilities {
    DeviceCapabilities caps;
    caps.canConnect = j.value("canConnect", true);
    caps.canDisconnect = j.value("canDisconnect", true);
    caps.canAbort = j.value("canAbort", false);
    caps.canPark = j.value("canPark", false);
    caps.canHome = j.value("canHome", false);
    caps.canSync = j.value("canSync", false);
    caps.canSlew = j.value("canSlew", false);
    caps.canTrack = j.value("canTrack", false);
    caps.canGuide = j.value("canGuide", false);
    caps.canCool = j.value("canCool", false);
    caps.canFocus = j.value("canFocus", false);
    caps.canRotate = j.value("canRotate", false);
    caps.hasShutter = j.value("hasShutter", false);
    caps.hasTemperature = j.value("hasTemperature", false);
    caps.hasPosition = j.value("hasPosition", false);
    caps.supportsAsync = j.value("supportsAsync", true);
    caps.supportsEvents = j.value("supportsEvents", true);
    caps.supportsProperties = j.value("supportsProperties", true);
    caps.supportsBatch = j.value("supportsBatch", false);
    return caps;
}

// ============================================================================
// DeviceTypeInfo
// ============================================================================

auto DeviceTypeInfo::toJson() const -> nlohmann::json {
    nlohmann::json j;
    j["typeName"] = typeName;
    j["category"] = category;
    j["displayName"] = displayName;
    j["description"] = description;
    j["pluginName"] = pluginName;
    j["version"] = version;
    j["capabilities"] = capabilities.toJson();
    j["propertySchema"] = propertySchema;
    j["metadata"] = metadata;
    j["priority"] = priority;
    j["enabled"] = enabled;
    return j;
}

auto DeviceTypeInfo::fromJson(const nlohmann::json& j) -> DeviceTypeInfo {
    DeviceTypeInfo info;
    info.typeName = j.value("typeName", "");
    info.category = j.value("category", "");
    info.displayName = j.value("displayName", "");
    info.description = j.value("description", "");
    info.pluginName = j.value("pluginName", "");
    info.version = j.value("version", "1.0.0");
    if (j.contains("capabilities")) {
        info.capabilities = DeviceCapabilities::fromJson(j["capabilities"]);
    }
    if (j.contains("propertySchema")) {
        info.propertySchema = j["propertySchema"];
    }
    if (j.contains("metadata")) {
        info.metadata = j["metadata"];
    }
    info.priority = j.value("priority", 0);
    info.enabled = j.value("enabled", true);
    return info;
}

// ============================================================================
// DeviceCategoryInfo
// ============================================================================

auto DeviceCategoryInfo::toJson() const -> nlohmann::json {
    nlohmann::json j;
    j["categoryName"] = categoryName;
    j["displayName"] = displayName;
    j["description"] = description;
    j["iconName"] = iconName;
    j["sortOrder"] = sortOrder;
    j["isBuiltIn"] = isBuiltIn;
    return j;
}

auto DeviceCategoryInfo::fromJson(const nlohmann::json& j)
    -> DeviceCategoryInfo {
    DeviceCategoryInfo info;
    info.categoryName = j.value("categoryName", "");
    info.displayName = j.value("displayName", "");
    info.description = j.value("description", "");
    info.iconName = j.value("iconName", "");
    info.sortOrder = j.value("sortOrder", 0);
    info.isBuiltIn = j.value("isBuiltIn", false);
    return info;
}

// ============================================================================
// DeviceTypeRegistry
// ============================================================================

DeviceTypeRegistry::DeviceTypeRegistry() { initializeBuiltInTypes(); }

auto DeviceTypeRegistry::getInstance() -> DeviceTypeRegistry& {
    static DeviceTypeRegistry instance;
    return instance;
}

auto DeviceTypeRegistry::registerType(const DeviceTypeInfo& info)
    -> DeviceResult<bool> {
    if (info.typeName.empty()) {
        return failure<bool>(DeviceErrorCode::InvalidArgument,
                             "Type name cannot be empty");
    }

    std::unique_lock lock(mutex_);

    // Check if type already exists
    if (types_.find(info.typeName) != types_.end()) {
        return failure<bool>(DeviceErrorCode::AlreadyExists,
                             "Type already registered: " + info.typeName);
    }

    // Check if category exists
    if (!info.category.empty() &&
        categories_.find(info.category) == categories_.end()) {
        LOG_WARN("Registering type {} with unknown category: {}", info.typeName,
                 info.category);
    }

    types_[info.typeName] = info;
    LOG_INFO("Registered device type: {} (category: {})", info.typeName,
             info.category);

    // Notify subscribers
    TypeRegistrationEvent event;
    event.action = TypeRegistrationEvent::Action::Registered;
    event.typeName = info.typeName;
    event.category = info.category;
    event.pluginName = info.pluginName;

    lock.unlock();
    notifySubscribers(event);

    return success(true);
}

auto DeviceTypeRegistry::registerTypeFromPlugin(const DeviceTypeInfo& info,
                                                const std::string& pluginName)
    -> DeviceResult<bool> {
    DeviceTypeInfo modifiedInfo = info;
    modifiedInfo.pluginName = pluginName;
    return registerType(modifiedInfo);
}

auto DeviceTypeRegistry::unregisterType(const std::string& typeName)
    -> DeviceResult<bool> {
    std::unique_lock lock(mutex_);

    auto it = types_.find(typeName);
    if (it == types_.end()) {
        return failure<bool>(DeviceErrorCode::NotFound,
                             "Type not found: " + typeName);
    }

    TypeRegistrationEvent event;
    event.action = TypeRegistrationEvent::Action::Unregistered;
    event.typeName = typeName;
    event.category = it->second.category;
    event.pluginName = it->second.pluginName;

    types_.erase(it);
    LOG_INFO("Unregistered device type: {}", typeName);

    lock.unlock();
    notifySubscribers(event);

    return success(true);
}

auto DeviceTypeRegistry::unregisterPluginTypes(const std::string& pluginName)
    -> size_t {
    std::unique_lock lock(mutex_);

    std::vector<std::string> toRemove;
    for (const auto& [name, info] : types_) {
        if (info.pluginName == pluginName) {
            toRemove.push_back(name);
        }
    }

    for (const auto& name : toRemove) {
        auto it = types_.find(name);
        if (it != types_.end()) {
            TypeRegistrationEvent event;
            event.action = TypeRegistrationEvent::Action::Unregistered;
            event.typeName = name;
            event.category = it->second.category;
            event.pluginName = pluginName;

            types_.erase(it);

            lock.unlock();
            notifySubscribers(event);
            lock.lock();
        }
    }

    if (!toRemove.empty()) {
        LOG_INFO("Unregistered {} types from plugin: {}", toRemove.size(),
                 pluginName);
    }

    return toRemove.size();
}

auto DeviceTypeRegistry::updateType(const DeviceTypeInfo& info)
    -> DeviceResult<bool> {
    std::unique_lock lock(mutex_);

    auto it = types_.find(info.typeName);
    if (it == types_.end()) {
        return failure<bool>(DeviceErrorCode::NotFound,
                             "Type not found: " + info.typeName);
    }

    it->second = info;
    LOG_INFO("Updated device type: {}", info.typeName);

    TypeRegistrationEvent event;
    event.action = TypeRegistrationEvent::Action::Updated;
    event.typeName = info.typeName;
    event.category = info.category;
    event.pluginName = info.pluginName;

    lock.unlock();
    notifySubscribers(event);

    return success(true);
}

auto DeviceTypeRegistry::hasType(const std::string& typeName) const -> bool {
    std::shared_lock lock(mutex_);
    return types_.find(typeName) != types_.end();
}

auto DeviceTypeRegistry::getTypeInfo(const std::string& typeName) const
    -> std::optional<DeviceTypeInfo> {
    std::shared_lock lock(mutex_);
    auto it = types_.find(typeName);
    if (it != types_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto DeviceTypeRegistry::getAllTypes() const -> std::vector<DeviceTypeInfo> {
    std::shared_lock lock(mutex_);
    std::vector<DeviceTypeInfo> result;
    result.reserve(types_.size());
    for (const auto& [_, info] : types_) {
        result.push_back(info);
    }
    return result;
}

auto DeviceTypeRegistry::getTypesByCategory(const std::string& category) const
    -> std::vector<DeviceTypeInfo> {
    std::shared_lock lock(mutex_);
    std::vector<DeviceTypeInfo> result;
    for (const auto& [_, info] : types_) {
        if (info.category == category) {
            result.push_back(info);
        }
    }
    return result;
}

auto DeviceTypeRegistry::getPluginTypes(const std::string& pluginName) const
    -> std::vector<DeviceTypeInfo> {
    std::shared_lock lock(mutex_);
    std::vector<DeviceTypeInfo> result;
    for (const auto& [_, info] : types_) {
        if (info.pluginName == pluginName) {
            result.push_back(info);
        }
    }
    return result;
}

auto DeviceTypeRegistry::getEnabledTypes() const
    -> std::vector<DeviceTypeInfo> {
    std::shared_lock lock(mutex_);
    std::vector<DeviceTypeInfo> result;
    for (const auto& [_, info] : types_) {
        if (info.enabled) {
            result.push_back(info);
        }
    }
    return result;
}

auto DeviceTypeRegistry::getTypeNames() const -> std::vector<std::string> {
    std::shared_lock lock(mutex_);
    std::vector<std::string> result;
    result.reserve(types_.size());
    for (const auto& [name, _] : types_) {
        result.push_back(name);
    }
    return result;
}

auto DeviceTypeRegistry::registerCategory(const DeviceCategoryInfo& info)
    -> DeviceResult<bool> {
    if (info.categoryName.empty()) {
        return failure<bool>(DeviceErrorCode::InvalidArgument,
                             "Category name cannot be empty");
    }

    std::unique_lock lock(mutex_);

    if (categories_.find(info.categoryName) != categories_.end()) {
        return failure<bool>(DeviceErrorCode::AlreadyExists,
                             "Category already registered: " + info.categoryName);
    }

    categories_[info.categoryName] = info;
    LOG_INFO("Registered device category: {}", info.categoryName);

    return success(true);
}

auto DeviceTypeRegistry::getCategoryInfo(const std::string& categoryName) const
    -> std::optional<DeviceCategoryInfo> {
    std::shared_lock lock(mutex_);
    auto it = categories_.find(categoryName);
    if (it != categories_.end()) {
        return it->second;
    }
    return std::nullopt;
}

auto DeviceTypeRegistry::getAllCategories() const
    -> std::vector<DeviceCategoryInfo> {
    std::shared_lock lock(mutex_);
    std::vector<DeviceCategoryInfo> result;
    result.reserve(categories_.size());
    for (const auto& [_, info] : categories_) {
        result.push_back(info);
    }
    // Sort by sortOrder
    std::sort(result.begin(), result.end(),
              [](const DeviceCategoryInfo& a, const DeviceCategoryInfo& b) {
                  return a.sortOrder < b.sortOrder;
              });
    return result;
}

auto DeviceTypeRegistry::hasCategory(const std::string& categoryName) const
    -> bool {
    std::shared_lock lock(mutex_);
    return categories_.find(categoryName) != categories_.end();
}

auto DeviceTypeRegistry::enableType(const std::string& typeName)
    -> DeviceResult<bool> {
    std::unique_lock lock(mutex_);

    auto it = types_.find(typeName);
    if (it == types_.end()) {
        return failure<bool>(DeviceErrorCode::NotFound,
                             "Type not found: " + typeName);
    }

    if (it->second.enabled) {
        return success(true);  // Already enabled
    }

    it->second.enabled = true;
    LOG_INFO("Enabled device type: {}", typeName);

    TypeRegistrationEvent event;
    event.action = TypeRegistrationEvent::Action::Enabled;
    event.typeName = typeName;
    event.category = it->second.category;
    event.pluginName = it->second.pluginName;

    lock.unlock();
    notifySubscribers(event);

    return success(true);
}

auto DeviceTypeRegistry::disableType(const std::string& typeName)
    -> DeviceResult<bool> {
    std::unique_lock lock(mutex_);

    auto it = types_.find(typeName);
    if (it == types_.end()) {
        return failure<bool>(DeviceErrorCode::NotFound,
                             "Type not found: " + typeName);
    }

    if (!it->second.enabled) {
        return success(true);  // Already disabled
    }

    it->second.enabled = false;
    LOG_INFO("Disabled device type: {}", typeName);

    TypeRegistrationEvent event;
    event.action = TypeRegistrationEvent::Action::Disabled;
    event.typeName = typeName;
    event.category = it->second.category;
    event.pluginName = it->second.pluginName;

    lock.unlock();
    notifySubscribers(event);

    return success(true);
}

auto DeviceTypeRegistry::isTypeEnabled(const std::string& typeName) const
    -> bool {
    std::shared_lock lock(mutex_);
    auto it = types_.find(typeName);
    if (it != types_.end()) {
        return it->second.enabled;
    }
    return false;
}

auto DeviceTypeRegistry::subscribe(TypeRegistrationCallback callback)
    -> TypeRegistrationCallbackId {
    std::unique_lock lock(mutex_);
    auto id = nextCallbackId_++;
    subscribers_[id] = std::move(callback);
    return id;
}

void DeviceTypeRegistry::unsubscribe(TypeRegistrationCallbackId callbackId) {
    std::unique_lock lock(mutex_);
    subscribers_.erase(callbackId);
}

void DeviceTypeRegistry::initializeBuiltInTypes() {
    // Register built-in categories
    struct CategoryDef {
        const char* name;
        const char* display;
        const char* desc;
        int order;
    };

    const CategoryDef builtInCategories[] = {
        {categories::CAMERA, "Camera", "CCD/CMOS cameras", 0},
        {categories::TELESCOPE, "Telescope", "Telescope mounts", 1},
        {categories::FOCUSER, "Focuser", "Focus controllers", 2},
        {categories::FILTERWHEEL, "Filter Wheel", "Filter wheel controllers", 3},
        {categories::DOME, "Dome", "Observatory domes", 4},
        {categories::ROTATOR, "Rotator", "Field rotators", 5},
        {categories::GUIDER, "Guider", "Autoguiding systems", 6},
        {categories::WEATHER, "Weather", "Weather stations", 7},
        {categories::GPS, "GPS", "GPS receivers", 8},
        {categories::AUXILIARY, "Auxiliary", "Auxiliary devices", 9},
        {categories::SWITCH, "Switch", "Power switches", 10},
        {categories::SAFETY_MONITOR, "Safety Monitor", "Safety monitoring", 11},
        {categories::COVER_CALIBRATOR, "Cover/Calibrator",
         "Cover and calibrator", 12},
        {categories::OBSERVING_CONDITIONS, "Observing Conditions",
         "Observing conditions sensors", 13},
        {categories::VIDEO, "Video", "Video cameras", 14}};

    for (const auto& cat : builtInCategories) {
        DeviceCategoryInfo info;
        info.categoryName = cat.name;
        info.displayName = cat.display;
        info.description = cat.desc;
        info.sortOrder = cat.order;
        info.isBuiltIn = true;

        categories_[info.categoryName] = info;
    }

    LOG_INFO("Initialized {} built-in device categories",
             std::size(builtInCategories));
}

void DeviceTypeRegistry::clear() {
    std::unique_lock lock(mutex_);
    types_.clear();
    categories_.clear();
    LOG_INFO("Cleared device type registry");
}

auto DeviceTypeRegistry::getStatistics() const -> nlohmann::json {
    std::shared_lock lock(mutex_);
    nlohmann::json stats;
    stats["totalTypes"] = types_.size();
    stats["totalCategories"] = categories_.size();
    stats["subscriberCount"] = subscribers_.size();

    // Count types per category
    nlohmann::json categoryStats;
    for (const auto& [catName, _] : categories_) {
        int count = 0;
        for (const auto& [_, typeInfo] : types_) {
            if (typeInfo.category == catName) {
                count++;
            }
        }
        categoryStats[catName] = count;
    }
    stats["typesPerCategory"] = categoryStats;

    // Count enabled vs disabled
    int enabledCount = 0;
    int pluginTypeCount = 0;
    for (const auto& [_, info] : types_) {
        if (info.enabled) {
            enabledCount++;
        }
        if (!info.pluginName.empty()) {
            pluginTypeCount++;
        }
    }
    stats["enabledTypes"] = enabledCount;
    stats["disabledTypes"] = static_cast<int>(types_.size()) - enabledCount;
    stats["pluginTypes"] = pluginTypeCount;
    stats["builtInTypes"] =
        static_cast<int>(types_.size()) - pluginTypeCount;

    return stats;
}

void DeviceTypeRegistry::notifySubscribers(const TypeRegistrationEvent& event) {
    std::shared_lock lock(mutex_);
    for (const auto& [_, callback] : subscribers_) {
        try {
            callback(event);
        } catch (const std::exception& e) {
            LOG_ERROR("Error in type registration callback: {}", e.what());
        }
    }
}

}  // namespace lithium::device
