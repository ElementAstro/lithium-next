/*
 * config_registry.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2024-11-30

Description: ConfigRegistry implementation

**************************************************/

#include "config_registry.hpp"

#include <algorithm>
#include <atomic>
#include <fstream>
#include <regex>
#include <sstream>

#include "atom/log/spdlog_logger.hpp"

namespace lithium::config {

// ============================================================================
// Implementation class
// ============================================================================

class ConfigRegistryImpl {
public:
    ConfigRegistryImpl() = default;

    std::shared_ptr<ConfigManager> configManager;
    std::unordered_map<std::string, SectionInfo> sections;
    std::vector<IConfigurable*> components;
    std::vector<fs::path> loadedFiles;

    struct Subscription {
        size_t id;
        std::string pathPattern;
        std::function<void(const json&, const json&)> callback;
    };
    std::vector<Subscription> subscriptions;
    std::atomic<size_t> nextSubscriptionId{1};

    mutable std::shared_mutex mutex;
};

// ============================================================================
// ConfigRegistry implementation
// ============================================================================

ConfigRegistry& ConfigRegistry::instance() {
    static ConfigRegistry instance;
    return instance;
}

ConfigRegistry::ConfigRegistry() : impl_(std::make_unique<ConfigRegistryImpl>()) {
    LOG_DEBUG("ConfigRegistry initialized");
}

ConfigRegistry::~ConfigRegistry() {
    LOG_DEBUG("ConfigRegistry destroyed");
}

// ============================================================================
// Initialization
// ============================================================================

void ConfigRegistry::setConfigManager(std::shared_ptr<ConfigManager> manager) {
    std::unique_lock lock(impl_->mutex);
    impl_->configManager = std::move(manager);
    LOG_INFO("ConfigRegistry: ConfigManager set");
}

std::shared_ptr<ConfigManager> ConfigRegistry::getConfigManager() const {
    std::shared_lock lock(impl_->mutex);
    return impl_->configManager;
}

// ============================================================================
// Section Registration
// ============================================================================

void ConfigRegistry::registerSectionInfo(std::string path, SectionInfo info) {
    std::unique_lock lock(impl_->mutex);

    if (impl_->sections.contains(path)) {
        LOG_WARN("ConfigRegistry: Section '{}' already registered, overwriting", path);
    }

    impl_->sections[path] = std::move(info);
    LOG_INFO("ConfigRegistry: Registered section '{}'", path);

    // Register schema with ConfigManager if available
    if (impl_->configManager && impl_->sections[path].schemaGenerator) {
        auto schema = impl_->sections[path].schemaGenerator();
        impl_->configManager->setSchema(path, schema);
    }
}

void ConfigRegistry::registerComponent(IConfigurable* component) {
    if (!component) {
        return;
    }

    std::unique_lock lock(impl_->mutex);

    // Check if already registered
    auto it = std::find(impl_->components.begin(), impl_->components.end(), component);
    if (it != impl_->components.end()) {
        LOG_WARN("ConfigRegistry: Component '{}' already registered",
                 component->getComponentName());
        return;
    }

    impl_->components.push_back(component);

    // Register as section
    SectionInfo info;
    info.path = std::string(component->getConfigPath());
    info.schemaGenerator = [component]() { return component->getConfigSchema(); };
    info.defaultGenerator = [component]() { return component->getDefaultConfig(); };
    info.supportsRuntimeReconfig = component->supportsRuntimeReconfig();

    impl_->sections[info.path] = std::move(info);

    LOG_INFO("ConfigRegistry: Registered component '{}' at path '{}'",
             component->getComponentName(), component->getConfigPath());
}

void ConfigRegistry::unregisterComponent(IConfigurable* component) {
    if (!component) {
        return;
    }

    std::unique_lock lock(impl_->mutex);

    auto it = std::find(impl_->components.begin(), impl_->components.end(), component);
    if (it != impl_->components.end()) {
        impl_->components.erase(it);
        impl_->sections.erase(std::string(component->getConfigPath()));
        LOG_INFO("ConfigRegistry: Unregistered component '{}'",
                 component->getComponentName());
    }
}

bool ConfigRegistry::hasSection(std::string_view path) const {
    std::shared_lock lock(impl_->mutex);
    return impl_->sections.contains(std::string(path));
}

std::vector<std::string> ConfigRegistry::getRegisteredSections() const {
    std::shared_lock lock(impl_->mutex);
    std::vector<std::string> result;
    result.reserve(impl_->sections.size());
    for (const auto& [path, _] : impl_->sections) {
        result.push_back(path);
    }
    return result;
}

// ============================================================================
// Configuration Loading
// ============================================================================

void ConfigRegistry::applyDefaults() {
    std::unique_lock lock(impl_->mutex);

    if (!impl_->configManager) {
        LOG_ERROR("ConfigRegistry: ConfigManager not set, cannot apply defaults");
        return;
    }

    for (const auto& [path, info] : impl_->sections) {
        if (info.defaultGenerator) {
            auto defaults = info.defaultGenerator();
            impl_->configManager->set(path, defaults);
            LOG_DEBUG("ConfigRegistry: Applied defaults for '{}'", path);
        }
    }

    LOG_INFO("ConfigRegistry: Applied defaults for {} sections",
             impl_->sections.size());
}

bool ConfigRegistry::loadFromFile(const fs::path& path,
                                   const ConfigLoadOptions& options) {
    if (!fs::exists(path)) {
        LOG_ERROR("ConfigRegistry: File not found: {}", path.string());
        return false;
    }

    // Detect format
    ConfigFormat format = options.format;
    if (format == ConfigFormat::AUTO) {
        auto ext = path.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".yaml" || ext == ".yml") {
            format = ConfigFormat::YAML;
        } else if (ext == ".json5") {
            format = ConfigFormat::JSON5;
        } else {
            format = ConfigFormat::JSON;
        }
    }

    // Read file
    std::ifstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("ConfigRegistry: Cannot open file: {}", path.string());
        return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    // Parse content
    json data;
    try {
        if (format == ConfigFormat::YAML) {
            // YAML parsing - for now, treat as JSON (YAML is superset of JSON)
            // TODO: Add proper YAML parsing with yaml-cpp
            data = json::parse(content, nullptr, true, true);
        } else if (format == ConfigFormat::JSON5) {
            // JSON5 parsing with comments allowed
            data = json::parse(content, nullptr, true, true);
        } else {
            data = json::parse(content);
        }
    } catch (const json::exception& e) {
        LOG_ERROR("ConfigRegistry: Failed to parse {}: {}", path.string(), e.what());
        if (options.strict) {
            throw ConfigValidationException(
                ConfigValidationResult{{false, {{path.string(), e.what(), "parse"}}}});
        }
        return false;
    }

    // Validate if strict
    if (options.strict) {
        auto result = validateAll();
        if (!result.isValid()) {
            throw ConfigValidationException(result);
        }
    }

    // Apply to ConfigManager
    {
        std::unique_lock lock(impl_->mutex);

        if (!impl_->configManager) {
            LOG_ERROR("ConfigRegistry: ConfigManager not set");
            return false;
        }

        if (options.mergeWithExisting) {
            impl_->configManager->merge(data);
        } else {
            impl_->configManager->clear();
            impl_->configManager->merge(data);
        }

        impl_->loadedFiles.push_back(path);

        // Enable file watching if requested
        if (options.enableWatch) {
            impl_->configManager->enableAutoReload(path);
        }
    }

    // Notify components
    for (auto* component : impl_->components) {
        auto configPath = component->getConfigPath();
        auto value = getValue(configPath);
        if (value) {
            component->applyConfig(*value);
        }
    }

    LOG_INFO("ConfigRegistry: Loaded configuration from {}", path.string());
    return true;
}

size_t ConfigRegistry::loadFromFiles(const std::vector<fs::path>& paths,
                                      const ConfigLoadOptions& options) {
    size_t loaded = 0;
    for (const auto& path : paths) {
        if (loadFromFile(path, options)) {
            ++loaded;
        }
    }
    return loaded;
}

size_t ConfigRegistry::loadFromDirectory(const fs::path& dirPath, bool recursive,
                                          const ConfigLoadOptions& options) {
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        LOG_ERROR("ConfigRegistry: Directory not found: {}", dirPath.string());
        return 0;
    }

    std::vector<fs::path> configFiles;

    auto addFile = [&](const fs::path& p) {
        auto ext = p.extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
        if (ext == ".json" || ext == ".json5" || ext == ".yaml" || ext == ".yml") {
            configFiles.push_back(p);
        }
    };

    if (recursive) {
        for (const auto& entry : fs::recursive_directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                addFile(entry.path());
            }
        }
    } else {
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            if (entry.is_regular_file()) {
                addFile(entry.path());
            }
        }
    }

    // Sort for deterministic order
    std::sort(configFiles.begin(), configFiles.end());

    return loadFromFiles(configFiles, options);
}

bool ConfigRegistry::saveToFile(const fs::path& path, ConfigFormat format) const {
    std::shared_lock lock(impl_->mutex);

    if (!impl_->configManager) {
        LOG_ERROR("ConfigRegistry: ConfigManager not set");
        return false;
    }

    auto data = exportAll();

    std::ofstream file(path);
    if (!file.is_open()) {
        LOG_ERROR("ConfigRegistry: Cannot create file: {}", path.string());
        return false;
    }

    if (format == ConfigFormat::YAML) {
        // TODO: Proper YAML output with yaml-cpp
        // For now, output as formatted JSON (valid YAML)
        file << data.dump(2);
    } else {
        file << data.dump(2);
    }

    LOG_INFO("ConfigRegistry: Saved configuration to {}", path.string());
    return true;
}

bool ConfigRegistry::reload() {
    std::vector<fs::path> files;
    {
        std::shared_lock lock(impl_->mutex);
        files = impl_->loadedFiles;
    }

    if (files.empty()) {
        LOG_WARN("ConfigRegistry: No files to reload");
        return false;
    }

    // Clear and reload
    clear();
    applyDefaults();

    ConfigLoadOptions options;
    options.mergeWithExisting = true;

    for (const auto& file : files) {
        if (!loadFromFile(file, options)) {
            LOG_ERROR("ConfigRegistry: Failed to reload {}", file.string());
            return false;
        }
    }

    LOG_INFO("ConfigRegistry: Reloaded {} files", files.size());
    return true;
}

// ============================================================================
// Configuration Access
// ============================================================================

std::optional<json> ConfigRegistry::getValue(std::string_view path) const {
    std::shared_lock lock(impl_->mutex);

    if (!impl_->configManager) {
        return std::nullopt;
    }

    return impl_->configManager->get(path);
}

// ============================================================================
// Configuration Updates
// ============================================================================

ConfigValidationResult ConfigRegistry::updateSectionJson(std::string_view path,
                                                          const json& value) {
    ConfigValidationResult result;

    std::unique_lock lock(impl_->mutex);

    if (!impl_->configManager) {
        result.addError(std::string(path), "ConfigManager not set", "internal");
        return result;
    }

    // Get old value for notification
    auto oldValue = impl_->configManager->get(path);

    // Validate if section is registered
    auto sectionIt = impl_->sections.find(std::string(path));
    if (sectionIt != impl_->sections.end() && sectionIt->second.validator) {
        result = sectionIt->second.validator(value);
        if (!result.isValid()) {
            LOG_WARN("ConfigRegistry: Validation failed for '{}': {}",
                     path, result.errors[0].message);
            return result;
        }
    }

    // Apply update
    if (!impl_->configManager->set(path, value)) {
        result.addError(std::string(path), "Failed to set value", "set");
        return result;
    }

    // Notify
    lock.unlock();
    notifySubscribers(path, oldValue.value_or(json{}), value);

    // Notify components
    for (auto* component : impl_->components) {
        if (component->getConfigPath() == path ||
            std::string(path).starts_with(std::string(component->getConfigPath()))) {
            component->onConfigChanged(path, value);
        }
    }

    LOG_DEBUG("ConfigRegistry: Updated '{}'", path);
    return result;
}

ConfigValidationResult ConfigRegistry::updateValue(std::string_view path,
                                                    const json& value) {
    return updateSectionJson(path, value);
}

bool ConfigRegistry::deleteValue(std::string_view path) {
    std::unique_lock lock(impl_->mutex);

    if (!impl_->configManager) {
        return false;
    }

    auto oldValue = impl_->configManager->get(path);
    if (!impl_->configManager->remove(path)) {
        return false;
    }

    lock.unlock();
    notifySubscribers(path, oldValue.value_or(json{}), json{});

    return true;
}

// ============================================================================
// Change Subscriptions
// ============================================================================

size_t ConfigRegistry::subscribe(
    std::string_view path,
    std::function<void(const json&, const json&)> callback) {
    std::unique_lock lock(impl_->mutex);

    size_t id = impl_->nextSubscriptionId++;
    impl_->subscriptions.push_back({id, std::string(path), std::move(callback)});

    LOG_DEBUG("ConfigRegistry: Added subscription {} for '{}'", id, path);
    return id;
}

bool ConfigRegistry::unsubscribe(size_t subscriptionId) {
    std::unique_lock lock(impl_->mutex);

    auto it = std::find_if(impl_->subscriptions.begin(), impl_->subscriptions.end(),
                           [subscriptionId](const auto& sub) {
                               return sub.id == subscriptionId;
                           });

    if (it != impl_->subscriptions.end()) {
        impl_->subscriptions.erase(it);
        LOG_DEBUG("ConfigRegistry: Removed subscription {}", subscriptionId);
        return true;
    }
    return false;
}

void ConfigRegistry::unsubscribeAll(std::string_view path) {
    std::unique_lock lock(impl_->mutex);

    impl_->subscriptions.erase(
        std::remove_if(impl_->subscriptions.begin(), impl_->subscriptions.end(),
                       [path](const auto& sub) {
                           return sub.pathPattern == path;
                       }),
        impl_->subscriptions.end());
}

void ConfigRegistry::notifySubscribers(std::string_view path,
                                        const json& oldValue,
                                        const json& newValue) {
    std::vector<std::function<void(const json&, const json&)>> callbacks;

    {
        std::shared_lock lock(impl_->mutex);
        for (const auto& sub : impl_->subscriptions) {
            // Check if path matches (simple prefix match or wildcard)
            bool matches = false;
            if (sub.pathPattern.ends_with("*")) {
                std::string prefix = sub.pathPattern.substr(0, sub.pathPattern.size() - 1);
                matches = std::string(path).starts_with(prefix);
            } else {
                matches = (sub.pathPattern == path);
            }

            if (matches) {
                callbacks.push_back(sub.callback);
            }
        }
    }

    // Call callbacks outside lock
    for (const auto& cb : callbacks) {
        try {
            cb(oldValue, newValue);
        } catch (const std::exception& e) {
            LOG_ERROR("ConfigRegistry: Subscription callback error: {}", e.what());
        }
    }
}

// ============================================================================
// Validation
// ============================================================================

ConfigValidationResult ConfigRegistry::validateSection(std::string_view path) const {
    std::shared_lock lock(impl_->mutex);

    ConfigValidationResult result;

    auto sectionIt = impl_->sections.find(std::string(path));
    if (sectionIt == impl_->sections.end()) {
        result.addError(std::string(path), "Section not registered", "registry");
        return result;
    }

    if (!impl_->configManager) {
        result.addError(std::string(path), "ConfigManager not set", "internal");
        return result;
    }

    auto value = impl_->configManager->get(path);
    if (!value) {
        result.addError(std::string(path), "Section not found in config", "missing");
        return result;
    }

    if (sectionIt->second.validator) {
        return sectionIt->second.validator(*value);
    }

    return result;
}

ConfigValidationResult ConfigRegistry::validateAll() const {
    ConfigValidationResult combinedResult;

    std::shared_lock lock(impl_->mutex);

    for (const auto& [path, info] : impl_->sections) {
        if (!impl_->configManager) {
            combinedResult.addError(path, "ConfigManager not set", "internal");
            continue;
        }

        auto value = impl_->configManager->get(path);
        if (!value) {
            // Missing sections are okay, they'll use defaults
            continue;
        }

        if (info.validator) {
            auto result = info.validator(*value);
            for (const auto& error : result.errors) {
                combinedResult.errors.push_back(error);
            }
            if (!result.isValid()) {
                combinedResult.valid = false;
            }
        }
    }

    return combinedResult;
}

json ConfigRegistry::generateFullSchema() const {
    std::shared_lock lock(impl_->mutex);

    json schema;
    schema["$schema"] = "http://json-schema.org/draft-07/schema#";
    schema["type"] = "object";
    schema["properties"] = json::object();

    for (const auto& [path, info] : impl_->sections) {
        if (info.schemaGenerator) {
            // Convert path to nested structure
            // e.g., "/lithium/server" -> {"lithium": {"server": {...}}}
            std::vector<std::string> parts;
            std::string pathStr = path;
            size_t pos = 0;
            while ((pos = pathStr.find('/')) != std::string::npos) {
                if (pos > 0) {
                    parts.push_back(pathStr.substr(0, pos));
                }
                pathStr.erase(0, pos + 1);
            }
            if (!pathStr.empty()) {
                parts.push_back(pathStr);
            }

            // Build nested schema
            json* current = &schema["properties"];
            for (size_t i = 0; i < parts.size() - 1; ++i) {
                if (!current->contains(parts[i])) {
                    (*current)[parts[i]] = {{"type", "object"}, {"properties", json::object()}};
                }
                current = &(*current)[parts[i]]["properties"];
            }
            if (!parts.empty()) {
                (*current)[parts.back()] = info.schemaGenerator();
            }
        }
    }

    return schema;
}

// ============================================================================
// Utilities
// ============================================================================

json ConfigRegistry::exportAll() const {
    std::shared_lock lock(impl_->mutex);

    if (!impl_->configManager) {
        return json::object();
    }

    // Get flattened config and unflatten it
    auto flat = impl_->configManager->flatten();
    json result = json::object();

    for (const auto& [path, value] : flat) {
        // Parse path and create nested structure
        std::vector<std::string> parts;
        std::string pathStr = path;
        size_t pos = 0;
        while ((pos = pathStr.find('/')) != std::string::npos) {
            if (pos > 0) {
                parts.push_back(pathStr.substr(0, pos));
            }
            pathStr.erase(0, pos + 1);
        }
        if (!pathStr.empty()) {
            parts.push_back(pathStr);
        }

        json* current = &result;
        for (size_t i = 0; i < parts.size() - 1; ++i) {
            if (!current->contains(parts[i])) {
                (*current)[parts[i]] = json::object();
            }
            current = &(*current)[parts[i]];
        }
        if (!parts.empty()) {
            (*current)[parts.back()] = value;
        }
    }

    return result;
}

ConfigValidationResult ConfigRegistry::importAll(const json& config, bool validate) {
    ConfigValidationResult result;

    if (validate) {
        // Validate against all registered schemas
        for (const auto& [path, info] : impl_->sections) {
            // Extract section from config
            std::vector<std::string> parts;
            std::string pathStr = path;
            size_t pos = 0;
            while ((pos = pathStr.find('/')) != std::string::npos) {
                if (pos > 0) {
                    parts.push_back(pathStr.substr(0, pos));
                }
                pathStr.erase(0, pos + 1);
            }
            if (!pathStr.empty()) {
                parts.push_back(pathStr);
            }

            const json* current = &config;
            bool found = true;
            for (const auto& part : parts) {
                if (current->contains(part)) {
                    current = &(*current)[part];
                } else {
                    found = false;
                    break;
                }
            }

            if (found && info.validator) {
                auto sectionResult = info.validator(*current);
                for (const auto& error : sectionResult.errors) {
                    result.errors.push_back(error);
                }
                if (!sectionResult.isValid()) {
                    result.valid = false;
                }
            }
        }

        if (!result.isValid()) {
            return result;
        }
    }

    // Apply
    std::unique_lock lock(impl_->mutex);
    if (impl_->configManager) {
        impl_->configManager->merge(config);
    }

    return result;
}

void ConfigRegistry::clear() {
    std::unique_lock lock(impl_->mutex);

    if (impl_->configManager) {
        impl_->configManager->clear();
    }
    impl_->loadedFiles.clear();

    LOG_INFO("ConfigRegistry: Cleared all configuration");
}

json ConfigRegistry::getStats() const {
    std::shared_lock lock(impl_->mutex);

    return {
        {"registeredSections", impl_->sections.size()},
        {"registeredComponents", impl_->components.size()},
        {"activeSubscriptions", impl_->subscriptions.size()},
        {"loadedFiles", impl_->loadedFiles.size()}
    };
}

}  // namespace lithium::config
