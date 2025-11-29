/*
 * manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-4-30

Description: High-performance Configuration Manager with Split Components

**************************************************/

#include "manager.hpp"
#include "atom/macro.hpp"

#include <atomic>
#include <condition_variable>
#include <fstream>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

#include <spdlog/spdlog.h>
#include "atom/type/json.hpp"

using json = nlohmann::json;

namespace lithium::config {

// Helper function to split string by delimiter
static auto splitString(std::string_view str, char delim = '/') {
    return str | std::ranges::views::split(delim) |
           std::ranges::views::transform([](auto&& rng) {
               return std::string_view(rng.begin(), rng.end());
           });
}

/**
 * @brief Implementation class for ConfigManager with integrated components
 */
class ConfigManagerImpl {
public:
    explicit ConfigManagerImpl(const ConfigManager::Options& options)
        : options_(options),
          nextCallbackId_(1),
          running_(true),
          logger_(spdlog::get("config_manager") ? spdlog::get("config_manager")
                                                : spdlog::default_logger()),
          cache_(options.cache_options),
          validator_(options.validator_options),
          serializer_(),
          watcher_(options.watcher_options) {
        logger_->info(
            "ConfigManager implementation initialized with components");

        // Initialize metrics
        metrics_.last_operation = std::chrono::steady_clock::now();

        // Start background save thread
        saveThread_ = std::thread(&ConfigManagerImpl::savingThread, this);

        // Start file watcher if enabled
        if (options_.enable_auto_reload) {
            watcher_.startWatching();
            logger_->info("File watcher started for auto-reload functionality");
        }
    }

    ~ConfigManagerImpl() {
        logger_->debug("ConfigManager implementation shutting down");

        running_.store(false);

        // Stop file watcher
        watcher_.stopAll();

        // Stop background save thread
        if (saveThread_.joinable()) {
            saveCondition_.notify_all();
            saveThread_.join();
        }

        logger_->info("ConfigManager implementation shut down completed");
    }

    // Helper method to navigate through JSON using a key path with caching and
    // validation
    template <typename ValueType>
    auto setOrAppendImpl(std::string_view key_path, ValueType&& value,
                         bool append) -> bool {
        const auto start_time = std::chrono::steady_clock::now();

        std::unique_lock lock(rwMutex_);

        try {
            // Validate the value if validation is enabled
            if (options_.enable_validation) {
                json temp_value = std::forward<ValueType>(value);
                auto validation_result =
                    validator_.validate(temp_value, key_path);
                if (!validation_result.isValid) {
                    logger_->error("Validation failed for key '{}': {}",
                                   key_path,
                                   validation_result.getErrorMessage());
                    ++metrics_.validation_failures;
                    return false;
                }
                ++metrics_.validation_successes;
            }

            // Check if the key_path is "/" and set the root value directly
            if (key_path == "/") {
                if (append) {
                    if (!config_.is_array()) {
                        config_ = json::array();
                    }
                    config_.push_back(std::forward<ValueType>(value));
                } else {
                    config_ = std::forward<ValueType>(value);
                }

                // Update cache
                if (options_.enable_caching) {
                    cache_.put("/", config_);
                }

                notifyChanges("/");
                logger_->info("Set root config: {}", config_.dump());
                updateOperationMetrics("set_root", start_time);
                return true;
            }

            json* p = &config_;
            auto keys = splitString(key_path);
            std::string currentPath;

            for (auto it = keys.begin(); it != keys.end(); ++it) {
                std::string keyStr((*it).begin(), (*it).end());
                if (keyStr.empty())
                    continue;  // Skip empty segments

                // Build current path for notifications
                if (!currentPath.empty())
                    currentPath += "/";
                currentPath += keyStr;

                logger_->debug("Processing path segment: {}", keyStr);

                if (std::next(it) == keys.end()) {  // If this is the last key
                    if (append) {
                        if (!p->contains(keyStr)) {
                            (*p)[keyStr] = json::array();
                        } else if (!(*p)[keyStr].is_array()) {
                            logger_->error("Target key is not an array: {}",
                                           keyStr);
                            return false;
                        }
                        (*p)[keyStr].push_back(std::forward<ValueType>(value));
                    } else {
                        (*p)[keyStr] = std::forward<ValueType>(value);
                    }

                    // Update cache
                    if (options_.enable_caching) {
                        cache_.put(currentPath, (*p)[keyStr]);
                    }

                    notifyChanges(currentPath);
                    logger_->debug("Set config at {}: {}", currentPath,
                                   (*p)[keyStr].dump());
                    updateOperationMetrics(append ? "append" : "set",
                                           start_time);
                    return true;
                }

                // Create intermediate objects if needed
                if (!p->contains(keyStr) || !(*p)[keyStr].is_object()) {
                    (*p)[keyStr] = json::object();
                }
                p = &(*p)[keyStr];
            }

            return false;
        } catch (const std::exception& e) {
            logger_->error("Error in setOrAppendImpl for '{}': {}", key_path,
                           e.what());
            return false;
        }
    }

    // Uses vector to store callback information for better cache locality
    struct CallbackInfo {
        size_t id;
        std::function<void(std::string_view)> callback;
    };

    // Notify all callbacks about configuration changes
    void notifyChanges(const std::string& path) {
        std::shared_lock lock(callbackMutex_);
        for (const auto& cb : callbacks_) {
            try {
                cb.callback(path);
            } catch (const std::exception& e) {
                logger_->error(
                    "Exception in config change callback for '{}': {}", path,
                    e.what());
            }
        }
    }

    // Add background saving functionality with improved performance
    void scheduleSave(const fs::path& path) {
        std::unique_lock lock(saveMutex_);
        pendingSaves_[path.string()] =
            std::chrono::system_clock::now() + options_.auto_save_delay;
        saveCondition_.notify_one();

        logger_->debug("Scheduled save for: {}", path.string());
    }

    // Background saving thread function with performance improvements
    void savingThread() {
        logger_->debug("Background saving thread started");

        while (running_.load()) {
            std::vector<std::string> pathsToSave;

            {
                std::unique_lock lock(saveMutex_);
                auto now = std::chrono::system_clock::now();

                // Wait until a save is due or we're shutting down
                saveCondition_.wait_for(
                    lock, std::chrono::seconds(1), [this, now]() {
                        return !running_.load() ||
                               std::ranges::any_of(
                                   pendingSaves_, [now](const auto& pair) {
                                       return pair.second <= now;
                                   });
                    });

                if (!running_.load())
                    return;

                // Find paths that need saving
                for (auto it = pendingSaves_.begin();
                     it != pendingSaves_.end();) {
                    if (it->second <= now) {
                        pathsToSave.push_back(it->first);
                        it = pendingSaves_.erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            // Save each file outside the lock with improved error handling
            for (const auto& path : pathsToSave) {
                const auto save_start = std::chrono::steady_clock::now();

                std::shared_lock configLock(rwMutex_);
                try {
                    fs::path filePath(path);
                    std::string filename = filePath.stem().string();

                    if (config_.contains(filename)) {
                        // Use serializer component for saving
                        auto result = serializer_.serialize(config_[filename]);
                        if (result.success) {
                            std::ofstream ofs(filePath);
                            if (ofs) {
                                ofs << result.data;
                                ++metrics_.files_saved;
                                updateOperationMetrics("auto_save", save_start);
                                logger_->info("Config auto-saved to file: {}",
                                              path);
                            } else {
                                logger_->error(
                                    "Failed to open file for auto-save: {}",
                                    path);
                            }
                        } else {
                            logger_->error(
                                "Failed to serialize config for auto-save: {}",
                                result.errorMessage);
                        }
                    }
                } catch (const std::exception& e) {
                    logger_->error("Error during auto-save of '{}': {}", path,
                                   e.what());
                }
            }
        }

        logger_->debug("Background saving thread stopped");
    }

    // Helper method to update operation metrics
    void updateOperationMetrics(
        const std::string& operation_type,
        std::chrono::steady_clock::time_point start_time) {
        const auto end_time = std::chrono::steady_clock::now();
        const auto duration_ms =
            std::chrono::duration_cast<std::chrono::microseconds>(end_time -
                                                                  start_time)
                .count() /
            1000.0;

        std::unique_lock lock(metricsMutex_);
        ++metrics_.total_operations;
        metrics_.last_operation = end_time;

        // Update running average
        if (operation_type == "get" || operation_type == "has") {
            if (metrics_.total_operations == 1) {
                metrics_.average_access_time_ms = duration_ms;
            } else {
                metrics_.average_access_time_ms =
                    (metrics_.average_access_time_ms *
                         (metrics_.total_operations - 1) +
                     duration_ms) /
                    metrics_.total_operations;
            }
        } else if (operation_type == "save" || operation_type == "auto_save") {
            if (metrics_.files_saved == 1) {
                metrics_.average_save_time_ms = duration_ms;
            } else {
                metrics_.average_save_time_ms =
                    (metrics_.average_save_time_ms *
                         (metrics_.files_saved - 1) +
                     duration_ms) /
                    metrics_.files_saved;
            }
        }
    }

    // Member variables
    ConfigManager::Options options_;  ///< Configuration options
    mutable std::shared_mutex
        rwMutex_;                ///< Thread safety mutex for config data
    json config_;                ///< Main configuration storage
    std::thread saveThread_;     ///< Background save thread
    std::atomic<bool> running_;  ///< Running state

    std::shared_ptr<spdlog::logger> logger_;  ///< Logger instance
    ConfigCache cache_;                       ///< Cache component
    ConfigValidator validator_;               ///< Validator component
    ConfigSerializer serializer_;             ///< Serializer component
    ConfigWatcher watcher_;                   ///< File watcher component

    // Callback management
    std::vector<CallbackInfo> callbacks_;  ///< Registered callbacks
    std::shared_mutex callbackMutex_;      ///< Callback thread safety
    std::atomic<size_t> nextCallbackId_;   ///< Next callback ID

    // Auto-save management
    std::mutex saveMutex_;                   ///< Save operations mutex
    std::condition_variable saveCondition_;  ///< Save condition variable
    std::unordered_map<std::string, std::chrono::system_clock::time_point>
        pendingSaves_;  ///< Pending saves

    // Metrics
    mutable std::shared_mutex metricsMutex_;  ///< Metrics thread safety
    ConfigManager::Metrics metrics_;          ///< Performance metrics
};

// ConfigManager implementation starts here
ConfigManager::ConfigManager() : ConfigManager(Options{}) {}

ConfigManager::ConfigManager(const Options& options)
    : impl_(std::make_unique<ConfigManagerImpl>(options)) {
    impl_->logger_->info("ConfigManager created with integrated components");
}

ConfigManager::~ConfigManager() {
    if (impl_) {
        // Save any remaining configs before destruction
        ATOM_UNUSED_RESULT(saveAll("./"));
        impl_->logger_->info("ConfigManager destroyed");
    }
}

ConfigManager::ConfigManager(ConfigManager&& other) noexcept
    : impl_(std::move(other.impl_)) {
    if (impl_) {
        impl_->logger_->debug("ConfigManager moved");
    }
}

ConfigManager& ConfigManager::operator=(ConfigManager&& other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
        if (impl_) {
            impl_->logger_->debug("ConfigManager move-assigned");
        }
    }
    return *this;
}

auto ConfigManager::createShared() -> std::shared_ptr<ConfigManager> {
    return createShared(Options{});
}

auto ConfigManager::createShared(const Options& options)
    -> std::shared_ptr<ConfigManager> {
    static std::mutex instanceMutex;
    static std::weak_ptr<ConfigManager> instancePtr;

    std::lock_guard lock(instanceMutex);
    if (auto shared = instancePtr.lock()) {
        return shared;
    }

    auto instance = std::shared_ptr<ConfigManager>(new ConfigManager(options));
    instancePtr = instance;
    return instance;
}

auto ConfigManager::createUnique() -> std::unique_ptr<ConfigManager> {
    return createUnique(Options{});
}

auto ConfigManager::createUnique(const Options& options)
    -> std::unique_ptr<ConfigManager> {
    return std::unique_ptr<ConfigManager>(new ConfigManager(options));
}

auto ConfigManager::get(std::string_view key_path) const
    -> std::optional<json> {
    const auto start_time = std::chrono::steady_clock::now();

    // Try cache first if caching is enabled
    if (impl_->options_.enable_caching) {
        if (auto cached_value = impl_->cache_.get(std::string(key_path))) {
            ++impl_->metrics_.cache_hits;
            impl_->updateOperationMetrics("get", start_time);
            return cached_value.value();
        }
        ++impl_->metrics_.cache_misses;
    }

    std::shared_lock lock(impl_->rwMutex_);
    try {
        const json* p = &impl_->config_;
        auto keys = splitString(key_path);

        for (const auto& key : keys) {
            if (key.empty())
                continue;  // Skip empty segments

            if (p->is_object() && p->contains(std::string(key))) {
                p = &(*p)[std::string(key)];
            } else {
                impl_->logger_->debug("Key not found: {}", key_path);
                impl_->updateOperationMetrics("get", start_time);
                return std::nullopt;
            }
        }

        json result = *p;

        // Cache the result if caching is enabled
        if (impl_->options_.enable_caching) {
            impl_->cache_.put(std::string(key_path), result);
        }

        impl_->updateOperationMetrics("get", start_time);
        return result;
    } catch (const std::exception& e) {
        impl_->logger_->error("Error retrieving key '{}': {}", key_path,
                              e.what());
        impl_->updateOperationMetrics("get", start_time);
        return std::nullopt;
    }
}

auto ConfigManager::set(std::string_view key_path, const json& value) -> bool {
    return impl_->setOrAppendImpl(key_path, value, false);
}

auto ConfigManager::set(std::string_view key_path, json&& value) -> bool {
    return impl_->setOrAppendImpl(key_path, std::move(value), false);
}

auto ConfigManager::append(std::string_view key_path, const json& value)
    -> bool {
    return impl_->setOrAppendImpl(key_path, value, true);
}

auto ConfigManager::remove(std::string_view key_path) -> bool {
    const auto start_time = std::chrono::steady_clock::now();

    std::unique_lock lock(impl_->rwMutex_);
    try {
        json* p = &impl_->config_;
        std::vector<std::string> keys;

        for (const auto& key : splitString(key_path)) {
            if (!key.empty()) {
                keys.emplace_back(key);
            }
        }

        if (keys.empty()) {
            impl_->logger_->warn("Invalid key path for deletion: {}", key_path);
            return false;
        }

        for (auto it = keys.begin(); it != keys.end(); ++it) {
            if (std::next(it) == keys.end()) {
                if (p->is_object() && p->contains(*it)) {
                    p->erase(*it);

                    // Remove from cache
                    if (impl_->options_.enable_caching) {
                        impl_->cache_.remove(std::string(key_path));
                    }

                    impl_->notifyChanges(std::string(key_path));
                    impl_->logger_->info("Deleted key: {}", key_path);
                    impl_->updateOperationMetrics("remove", start_time);
                    return true;
                }
                impl_->logger_->warn("Key not found for deletion: {}",
                                     key_path);
                return false;
            }

            if (!p->is_object() || !p->contains(*it)) {
                impl_->logger_->warn("Key not found for deletion: {}",
                                     key_path);
                return false;
            }
            p = &(*p)[*it];
        }

        return false;
    } catch (const std::exception& e) {
        impl_->logger_->error("Error removing key '{}': {}", key_path,
                              e.what());
        return false;
    }
}

auto ConfigManager::has(std::string_view key_path) const -> bool {
    const auto start_time = std::chrono::steady_clock::now();

    // Check cache first if caching is enabled
    if (impl_->options_.enable_caching) {
        if (impl_->cache_.contains(std::string(key_path))) {
            ++impl_->metrics_.cache_hits;
            impl_->updateOperationMetrics("has", start_time);
            return true;
        }
        ++impl_->metrics_.cache_misses;
    }

    bool result = get(key_path).has_value();
    impl_->updateOperationMetrics("has", start_time);
    return result;
}

auto ConfigManager::getKeys() const -> std::vector<std::string> {
    std::shared_lock lock(impl_->rwMutex_);
    std::vector<std::string> keys;

    if (impl_->config_.is_object()) {
        for (auto it = impl_->config_.begin(); it != impl_->config_.end();
             ++it) {
            keys.push_back(it.key());
        }
    }

    return keys;
}

auto ConfigManager::listPaths() const -> std::vector<std::string> {
    std::shared_lock lock(impl_->rwMutex_);
    std::vector<std::string> paths;

    std::function<void(const json&, const std::string&)> collectPaths =
        [&](const json& node, const std::string& prefix) {
            if (node.is_object()) {
                for (auto it = node.begin(); it != node.end(); ++it) {
                    std::string path =
                        prefix.empty() ? it.key() : prefix + "/" + it.key();
                    paths.push_back(path);
                    collectPaths(it.value(), path);
                }
            }
        };

    collectPaths(impl_->config_, "");
    return paths;
}

auto ConfigManager::loadFromFile(const fs::path& path) -> bool {
    const auto start_time = std::chrono::steady_clock::now();

    try {
        auto result = impl_->serializer_.deserializeFromFile(path);
        if (!result.isValid()) {
            impl_->logger_->error("Failed to load config from '{}': {}",
                                  path.string(), result.errorMessage);
            return false;
        }

        std::string filename = path.stem().string();

        std::unique_lock lock(impl_->rwMutex_);
        impl_->config_[filename] = std::move(result.data);

        // Clear related cache entries
        if (impl_->options_.enable_caching) {
            impl_->cache_.clear();
        }

        ++impl_->metrics_.files_loaded;
        impl_->updateOperationMetrics("load", start_time);
        impl_->logger_->info("Loaded config from file: {}", path.string());

        return true;
    } catch (const std::exception& e) {
        impl_->logger_->error("Exception loading config from '{}': {}",
                              path.string(), e.what());
        return false;
    }
}

auto ConfigManager::loadFromFiles(std::span<const fs::path> paths) -> size_t {
    size_t loaded = 0;
    for (const auto& path : paths) {
        if (loadFromFile(path)) {
            ++loaded;
        }
    }
    return loaded;
}

auto ConfigManager::loadFromDir(const fs::path& dir_path, bool recursive)
    -> bool {
    if (!fs::exists(dir_path) || !fs::is_directory(dir_path)) {
        impl_->logger_->error("Invalid directory path: {}", dir_path.string());
        return false;
    }

    size_t loaded = 0;
    auto process_entry = [&](const fs::directory_entry& entry) {
        if (entry.is_regular_file()) {
            auto ext = entry.path().extension().string();
            if (ext == ".json" || ext == ".json5" || ext == ".lithium") {
                if (loadFromFile(entry.path())) {
                    ++loaded;
                }
            }
        }
    };

    try {
        if (recursive) {
            for (const auto& entry :
                 fs::recursive_directory_iterator(dir_path)) {
                process_entry(entry);
            }
        } else {
            for (const auto& entry : fs::directory_iterator(dir_path)) {
                process_entry(entry);
            }
        }

        impl_->logger_->info("Loaded {} config files from directory: {}",
                             loaded, dir_path.string());
        return loaded > 0;
    } catch (const std::exception& e) {
        impl_->logger_->error("Error loading from directory '{}': {}",
                              dir_path.string(), e.what());
        return false;
    }
}

auto ConfigManager::save(const fs::path& file_path) const -> bool {
    const auto start_time = std::chrono::steady_clock::now();

    std::shared_lock lock(impl_->rwMutex_);
    try {
        bool result =
            impl_->serializer_.serializeToFile(impl_->config_, file_path);
        if (result) {
            ++impl_->metrics_.files_saved;
            impl_->updateOperationMetrics("save", start_time);
            impl_->logger_->info("Saved config to file: {}",
                                 file_path.string());
        }
        return result;
    } catch (const std::exception& e) {
        impl_->logger_->error("Error saving config to '{}': {}",
                              file_path.string(), e.what());
        return false;
    }
}

auto ConfigManager::saveAll(const fs::path& dir_path) const -> bool {
    std::shared_lock lock(impl_->rwMutex_);

    if (!impl_->config_.is_object()) {
        return save(dir_path / "config.json");
    }

    bool all_success = true;
    for (auto it = impl_->config_.begin(); it != impl_->config_.end(); ++it) {
        fs::path file_path = dir_path / (it.key() + ".json");
        if (!impl_->serializer_.serializeToFile(it.value(), file_path)) {
            all_success = false;
            impl_->logger_->error("Failed to save config section: {}",
                                  it.key());
        }
    }

    return all_success;
}

void ConfigManager::tidy() {
    std::unique_lock lock(impl_->rwMutex_);

    // Clear cache to force refresh
    if (impl_->options_.enable_caching) {
        impl_->cache_.cleanup();
    }

    impl_->logger_->debug("Configuration tidied");
}

void ConfigManager::clear() {
    std::unique_lock lock(impl_->rwMutex_);

    impl_->config_ = json::object();

    if (impl_->options_.enable_caching) {
        impl_->cache_.clear();
    }

    impl_->logger_->info("Configuration cleared");
}

void ConfigManager::merge(const json& src) {
    std::unique_lock lock(impl_->rwMutex_);
    merge(src, impl_->config_);

    if (impl_->options_.enable_caching) {
        impl_->cache_.clear();
    }

    impl_->notifyChanges("/");
    impl_->logger_->debug("Configuration merged");
}

void ConfigManager::merge(const json& src, json& target) {
    if (src.is_object() && target.is_object()) {
        for (auto it = src.begin(); it != src.end(); ++it) {
            if (target.contains(it.key()) && target[it.key()].is_object() &&
                it.value().is_object()) {
                merge(it.value(), target[it.key()]);
            } else {
                target[it.key()] = it.value();
            }
        }
    } else {
        target = src;
    }
}

size_t ConfigManager::onChanged(
    std::function<void(std::string_view path)> callback) {
    std::unique_lock lock(impl_->callbackMutex_);
    size_t id = impl_->nextCallbackId_++;
    impl_->callbacks_.push_back({id, std::move(callback)});
    return id;
}

bool ConfigManager::removeCallback(size_t id) {
    std::unique_lock lock(impl_->callbackMutex_);
    auto it = std::find_if(impl_->callbacks_.begin(), impl_->callbacks_.end(),
                           [id](const auto& cb) { return cb.id == id; });
    if (it != impl_->callbacks_.end()) {
        impl_->callbacks_.erase(it);
        return true;
    }
    return false;
}

// Component access methods
const ConfigCache& ConfigManager::getCache() const { return impl_->cache_; }

const ConfigValidator& ConfigManager::getValidator() const {
    return impl_->validator_;
}

const ConfigSerializer& ConfigManager::getSerializer() const {
    return impl_->serializer_;
}

const ConfigWatcher& ConfigManager::getWatcher() const {
    return impl_->watcher_;
}

// Configuration and metrics methods
void ConfigManager::updateOptions(const Options& options) {
    impl_->options_ = options;
    impl_->logger_->info("ConfigManager options updated");
}

const ConfigManager::Options& ConfigManager::getOptions() const noexcept {
    return impl_->options_;
}

ConfigManager::Metrics ConfigManager::getMetrics() const {
    std::shared_lock lock(impl_->metricsMutex_);

    // Combine metrics from all components
    auto metrics = impl_->metrics_;

    // Add cache metrics
    auto cache_stats = impl_->cache_.getStatistics();
    metrics.cache_hits = cache_stats.hits;
    metrics.cache_misses = cache_stats.misses;

    // Add watcher metrics
    auto watcher_stats = impl_->watcher_.getStatistics();
    metrics.auto_reloads = watcher_stats.total_events_processed;

    return metrics;
}

void ConfigManager::resetMetrics() {
    std::unique_lock lock(impl_->metricsMutex_);
    impl_->metrics_ = Metrics{};
    impl_->metrics_.last_operation = std::chrono::steady_clock::now();

    impl_->watcher_.resetStatistics();

    impl_->logger_->debug("All metrics reset");
}

// Validation methods
bool ConfigManager::setSchema(std::string_view schema_path,
                              const json& schema) {
    try {
        bool result = impl_->validator_.setSchema(schema);
        if (result) {
            impl_->logger_->info("Schema set for path: {}", schema_path);
        } else {
            impl_->logger_->error("Failed to set schema for path: {}",
                                  schema_path);
        }
        return result;
    } catch (const std::exception& e) {
        impl_->logger_->error("Exception setting schema for '{}': {}",
                              schema_path, e.what());
        return false;
    }
}

bool ConfigManager::loadSchema(std::string_view schema_path,
                               const fs::path& file_path) {
    try {
        bool result = impl_->validator_.loadSchema(file_path.string());
        if (result) {
            impl_->logger_->info("Schema loaded for path '{}' from file: {}",
                                 schema_path, file_path.string());
        } else {
            impl_->logger_->error(
                "Failed to load schema for path '{}' from file: {}",
                schema_path, file_path.string());
        }
        return result;
    } catch (const std::exception& e) {
        impl_->logger_->error("Exception loading schema for '{}' from '{}': {}",
                              schema_path, file_path.string(), e.what());
        return false;
    }
}

ValidationResult ConfigManager::validate(std::string_view config_path) const {
    auto value = get(config_path);
    if (!value) {
        return ValidationResult{
            false,
            {},
            {},
            "Configuration path not found: " + std::string(config_path)};
    }

    return impl_->validator_.validate(value.value(), config_path);
}

ValidationResult ConfigManager::validateAll() const {
    std::shared_lock lock(impl_->rwMutex_);
    return impl_->validator_.validate(impl_->config_, "");
}

// File watching methods
bool ConfigManager::enableAutoReload(const fs::path& file_path) {
    try {
        bool result = impl_->watcher_.watchFile(
            file_path, [this](const fs::path& changed_path, FileEvent event) {
                onFileChanged(changed_path, event);
            });

        if (result) {
            impl_->logger_->info("Auto-reload enabled for: {}",
                                 file_path.string());
        } else {
            impl_->logger_->error("Failed to enable auto-reload for: {}",
                                  file_path.string());
        }
        return result;
    } catch (const std::exception& e) {
        impl_->logger_->error("Exception enabling auto-reload for '{}': {}",
                              file_path.string(), e.what());
        return false;
    }
}

bool ConfigManager::disableAutoReload(const fs::path& file_path) {
    try {
        bool result = impl_->watcher_.stopWatching(file_path);
        if (result) {
            impl_->logger_->info("Auto-reload disabled for: {}",
                                 file_path.string());
        }
        return result;
    } catch (const std::exception& e) {
        impl_->logger_->error("Exception disabling auto-reload for '{}': {}",
                              file_path.string(), e.what());
        return false;
    }
}

bool ConfigManager::isAutoReloadEnabled(const fs::path& file_path) const {
    return impl_->watcher_.isWatching(file_path);
}

// Helper methods
void ConfigManager::logTypeConversionError(std::string_view key_path,
                                           const char* type_name,
                                           const char* error_message) const {
    impl_->logger_->error("Type conversion error for '{}' to type '{}': {}",
                          key_path, type_name, error_message);
}

void ConfigManager::onFileChanged(const fs::path& file_path, FileEvent event) {
    const char* event_name = [event]() {
        switch (event) {
            case FileEvent::CREATED:
                return "CREATED";
            case FileEvent::MODIFIED:
                return "MODIFIED";
            case FileEvent::DELETED:
                return "DELETED";
            case FileEvent::MOVED:
                return "MOVED";
            default:
                return "UNKNOWN";
        }
    }();

    impl_->logger_->info("File {} event for: {}", event_name,
                         file_path.string());

    if (event == FileEvent::MODIFIED || event == FileEvent::CREATED) {
        // Reload the file
        if (loadFromFile(file_path)) {
            ++impl_->metrics_.auto_reloads;
            impl_->logger_->info("Auto-reloaded configuration from: {}",
                                 file_path.string());
        } else {
            impl_->logger_->error(
                "Failed to auto-reload configuration from: {}",
                file_path.string());
        }
    } else if (event == FileEvent::DELETED) {
        // Remove from configuration
        std::string filename = file_path.stem().string();
        if (remove("/" + filename)) {
            impl_->logger_->info("Removed deleted configuration: {}", filename);
        }
    }
}

void ConfigManager::updateMetrics(const std::string& operation_type,
                                  double duration_ms) {
    auto duration =
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(
            std::chrono::duration<double, std::milli>(duration_ms));
    impl_->updateOperationMetrics(operation_type,
                                  std::chrono::steady_clock::now() - duration);
}

}  // namespace lithium::config
