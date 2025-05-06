/*
 * configor.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/*************************************************

Date: 2023-4-30

Description: Configor

**************************************************/

#include "configor.hpp"
#include "atom/macro.hpp"
#include "json5.hpp"

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <future>
#include <mutex>
#include <ranges>
#include <shared_mutex>
#include <thread>
#include <unordered_map>

#include "atom/function/global_ptr.hpp"
#include "atom/io/io.hpp"
#include "atom/log/loguru.hpp"
#include "atom/system/env.hpp"
#include "atom/type/json.hpp"

#include "constant/constant.hpp"

using json = nlohmann::json;

namespace lithium {

// Helper function to split string by delimiter
static auto splitString(std::string_view str, char delim = '/') {
    return str | std::ranges::views::split(delim) |
           std::ranges::views::transform([](auto &&rng) {
               return std::string_view(rng.begin(), rng.end());
           });
}

class ConfigManagerImpl {
public:
    ConfigManagerImpl() : nextCallbackId(1) {}

    ~ConfigManagerImpl() {
        running = false;
        if (saveThread.joinable()) {
            saveCondition.notify_all();
            saveThread.join();
        }
    }

    // Helper method to navigate through JSON using a key path
    template <typename ValueType>
    auto setOrAppendImpl(std::string_view key_path, ValueType &&value,
                         bool append) -> bool {
        std::unique_lock lock(rwMutex);

        try {
            // Check if the key_path is "/" and set the root value directly
            if (key_path == "/") {
                if (append) {
                    if (!config.is_array()) {
                        config = json::array();
                    }
                    config.push_back(std::forward<ValueType>(value));
                } else {
                    config = std::forward<ValueType>(value);
                }
                notifyChanges("/");
                LOG_F(INFO, "Set root config: {}", config.dump());
                return true;
            }

            json *p = &config;
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

                LOG_F(INFO, "Processing path segment: {}", keyStr);

                if (std::next(it) == keys.end()) {  // If this is the last key
                    if (append) {
                        if (!p->contains(keyStr)) {
                            (*p)[keyStr] = json::array();
                        } else if (!(*p)[keyStr].is_array()) {
                            LOG_F(ERROR, "Target key is not an array: {}",
                                  keyStr);
                            return false;
                        }
                        (*p)[keyStr].push_back(std::forward<ValueType>(value));
                    } else {
                        (*p)[keyStr] = std::forward<ValueType>(value);
                    }

                    notifyChanges(currentPath);
                    LOG_F(INFO, "Final config at {}: {}", currentPath,
                          (*p)[keyStr].dump());
                    return true;
                }

                // Create intermediate objects if needed
                if (!p->contains(keyStr) || !(*p)[keyStr].is_object()) {
                    (*p)[keyStr] = json::object();
                }
                p = &(*p)[keyStr];
            }

            return false;
        } catch (const std::exception &e) {
            LOG_F(ERROR, "Error in setOrAppendImpl: {}", e.what());
            return false;
        }
    }

    // Uses vector to store callback information for better cache locality
    struct CallbackInfo {
        size_t id;
        std::function<void(std::string_view)> callback;
    };

    // Notify all callbacks about configuration changes
    void notifyChanges(const std::string &path) {
        std::shared_lock lock(callbackMutex);
        for (const auto &cb : callbacks) {
            try {
                cb.callback(path);
            } catch (const std::exception &e) {
                LOG_F(ERROR, "Exception in config change callback: {}",
                      e.what());
            }
        }
    }

    // Add background saving functionality
    void scheduleSave(const fs::path &path) {
        std::unique_lock lock(saveMutex);
        pendingSaves[path.string()] =
            std::chrono::system_clock::now() + std::chrono::seconds(5);
        saveCondition.notify_one();
    }

    // Background saving thread function
    void savingThread() {
        while (running) {
            std::vector<std::string> pathsToSave;

            {
                std::unique_lock lock(saveMutex);
                auto now = std::chrono::system_clock::now();

                // Wait until a save is due or we're shutting down
                saveCondition.wait_for(
                    lock, std::chrono::seconds(1), [this, now]() {
                        return !running ||
                               std::ranges::any_of(
                                   pendingSaves, [now](const auto &pair) {
                                       return pair.second <= now;
                                   });
                    });

                if (!running)
                    return;

                // Find paths that need saving
                for (auto it = pendingSaves.begin();
                     it != pendingSaves.end();) {
                    if (it->second <= now) {
                        pathsToSave.push_back(it->first);
                        it = pendingSaves.erase(it);
                    } else {
                        ++it;
                    }
                }
            }

            // Save each file outside the lock
            for (const auto &path : pathsToSave) {
                std::shared_lock configLock(rwMutex);
                try {
                    fs::path filePath(path);
                    std::string filename = filePath.stem().string();

                    if (config.contains(filename)) {
                        std::ofstream ofs(filePath);
                        if (ofs) {
                            ofs << config[filename].dump(4);
                            LOG_F(INFO, "Config auto-saved to file: {}", path);
                        }
                    }
                } catch (const std::exception &e) {
                    LOG_F(ERROR, "Error during auto-save: {}", e.what());
                }
            }
        }
    }

    mutable std::shared_mutex rwMutex;
    json config;
    std::thread saveThread;
    std::atomic<bool> running{true};

    // Callback management
    std::vector<CallbackInfo> callbacks;
    std::shared_mutex callbackMutex;
    std::atomic<size_t> nextCallbackId;

    // Auto-save management
    std::mutex saveMutex;
    std::condition_variable saveCondition;
    std::unordered_map<std::string, std::chrono::system_clock::time_point>
        pendingSaves;
};

ConfigManager::ConfigManager() : impl_(std::make_unique<ConfigManagerImpl>()) {
    LOG_F(INFO, "ConfigManager created.");
    // Start background saving thread
    impl_->saveThread =
        std::thread(&ConfigManagerImpl::savingThread, impl_.get());
}

ConfigManager::~ConfigManager() {
    // Signal thread to stop
    impl_->running = false;

    if (impl_->saveThread.joinable()) {
        impl_->saveCondition.notify_all();
        impl_->saveThread.join();
    }

    // Save any remaining configs
    ATOM_UNUSED_RESULT(saveAll("./"));
    LOG_F(INFO, "ConfigManager destroyed.");
}

ConfigManager::ConfigManager(ConfigManager &&other) noexcept
    : impl_(std::move(other.impl_)) {
    LOG_F(INFO, "ConfigManager moved.");
}

ConfigManager &ConfigManager::operator=(ConfigManager &&other) noexcept {
    if (this != &other) {
        impl_ = std::move(other.impl_);
        LOG_F(INFO, "ConfigManager move-assigned.");
    }
    return *this;
}

auto ConfigManager::createShared() -> std::shared_ptr<ConfigManager> {
    static std::mutex instanceMutex;
    static std::weak_ptr<ConfigManager> instancePtr;

    std::lock_guard lock(instanceMutex);
    if (auto shared = instancePtr.lock()) {
        return shared;
    }

    auto instance = std::shared_ptr<ConfigManager>(new ConfigManager());
    instancePtr = instance;
    return instance;
}

auto ConfigManager::createUnique() -> std::unique_ptr<ConfigManager> {
    return std::unique_ptr<ConfigManager>(new ConfigManager());
}

auto ConfigManager::loadFromFile(const fs::path &path) -> bool {
    try {
        std::ifstream ifs(path);
        if (!ifs || ifs.peek() == std::ifstream::traits_type::eof()) {
            LOG_F(ERROR, "Failed to open file: {}", path.string());
            return false;
        }

        std::string filename = path.stem().string();

        if (path.extension() == ".json" || path.extension() == ".lithium") {
            json j = json::parse(ifs);
            if (j.empty()) {
                LOG_F(WARNING, "Config file is empty: {}", path.string());
                return false;
            }

            std::unique_lock lock(impl_->rwMutex);
            impl_->config[filename] = j;
            impl_->notifyChanges("/" + filename);
        } else if (path.extension() == ".json5" ||
                   path.extension() == ".lithium5") {
            // Use improved json5 parser with string_view for better performance
            std::string json5((std::istreambuf_iterator<char>(ifs)),
                              std::istreambuf_iterator<char>());

            auto result = internal::convertJSON5toJSON(json5);
            if (!result) {
                LOG_F(ERROR, "Failed to parse JSON5 file: {}, error: {}",
                      path.string(), result.error().what());
                return false;
            }

            json j = json::parse(result.value());
            if (j.empty()) {
                LOG_F(WARNING, "Config file is empty: {}", path.string());
                return false;
            }

            std::unique_lock lock(impl_->rwMutex);
            impl_->config[filename] = j;
            impl_->notifyChanges("/" + filename);
        } else {
            LOG_F(WARNING, "Unsupported file extension: {}",
                  path.extension().string());
            return false;
        }

        LOG_F(INFO, "Config loaded from file: {}", path.string());
        return true;
    } catch (const json::exception &e) {
        LOG_F(ERROR, "Failed to parse file: {}, error message: {}",
              path.string(), e.what());
    } catch (const std::exception &e) {
        LOG_F(ERROR, "Failed to load config file: {}, error message: {}",
              path.string(), e.what());
    }
    return false;
}

auto ConfigManager::loadFromFiles(std::span<const fs::path> paths) -> size_t {
    size_t successCount = 0;

    // Use parallel algorithms for multiple files
    if (paths.size() > 4) {  // Threshold for parallelism
        std::vector<std::future<bool>> results;
        results.reserve(paths.size());

        for (const auto &path : paths) {
            results.push_back(std::async(std::launch::async, [this, &path]() {
                return loadFromFile(path);
            }));
        }

        for (auto &result : results) {
            if (result.get()) {
                successCount++;
            }
        }
    } else {
        // Sequential load for small numbers of files
        for (const auto &path : paths) {
            if (loadFromFile(path)) {
                successCount++;
            }
        }
    }

    return successCount;
}

auto ConfigManager::loadFromDir(const fs::path &dir_path,
                                bool recursive) -> bool {
    std::shared_lock lock(impl_->rwMutex);
    try {
        for (const auto &entry : fs::directory_iterator(dir_path)) {
            if (entry.is_regular_file()) {
                if (entry.path().extension() == ".json" ||
                    entry.path().extension() == ".lithium") {
                    if (!loadFromFile(entry.path())) {
                        LOG_F(WARNING, "Failed to load config file: {}",
                              entry.path().string());
                    }
                } else if (entry.path().extension() == ".json5" ||
                           entry.path().extension() == ".lithium5") {
                    std::ifstream ifs(entry.path());
                    if (!ifs ||
                        ifs.peek() == std::ifstream::traits_type::eof()) {
                        LOG_F(ERROR, "Failed to open file: {}",
                              entry.path().string());
                        return false;
                    }
                    std::string json5((std::istreambuf_iterator<char>(ifs)),
                                      std::istreambuf_iterator<char>());
                    json j = json::parse(internal::convertJSON5toJSON(json5));
                    if (j.empty()) {
                        LOG_F(WARNING, "Config file is empty: {}",
                              entry.path().string());
                        return false;
                    }
                    std::string filename = entry.path().stem().string();
                    impl_->config[filename] = j;
                }
            } else if (recursive && entry.is_directory()) {
                loadFromDir(entry.path(), true);
            }
        }
        LOG_F(INFO, "Config loaded from directory: {}", dir_path.string());
        return true;
    } catch (const std::exception &e) {
        LOG_F(ERROR, "Failed to load config file from: {}, error message: {}",
              dir_path.string(), e.what());
        return false;
    }
}

auto ConfigManager::save(const fs::path &file_path) const -> bool {
    std::unique_lock lock(impl_->rwMutex);
    std::ofstream ofs(file_path);
    if (!ofs) {
        LOG_F(ERROR, "Failed to open file: {}", file_path.string());
        return false;
    }
    try {
        std::string filename = file_path.stem().string();
        if (impl_->config.contains(filename)) {
            ofs << impl_->config[filename].dump(4);
            ofs.close();
            LOG_F(INFO, "Config saved to file: {}", file_path.string());
            return true;
        } else {
            LOG_F(ERROR, "Config for file: {} not found", file_path.string());
            return false;
        }
    } catch (const std::exception &e) {
        LOG_F(ERROR, "Failed to save config to file: {}, error message: {}",
              file_path.string(), e.what());
        return false;
    }
}

auto ConfigManager::saveAll(const fs::path &dir_path) const -> bool {
    std::unique_lock lock(impl_->rwMutex);
    try {
        for (const auto &[filename, config] : impl_->config.items()) {
            fs::path file_path = dir_path / (filename + ".json");
            std::ofstream ofs(file_path);
            if (!ofs) {
                LOG_F(ERROR, "Failed to open file: {}", file_path.string());
                return false;
            }
            ofs << config.dump(4);
            ofs.close();
            LOG_F(INFO, "Config saved to file: {}", file_path.string());
        }
        return true;
    } catch (const std::exception &e) {
        LOG_F(ERROR,
              "Failed to save all configs to directory: {}, error message: {}",
              dir_path.string(), e.what());
        return false;
    }
}

auto ConfigManager::get(std::string_view key_path) const
    -> std::optional<json> {
    std::shared_lock lock(impl_->rwMutex);
    try {
        const json *p = &impl_->config;
        auto keys = splitString(key_path);

        for (const auto &key : keys) {
            if (key.empty())
                continue;  // Skip empty segments

            if (p->is_object() && p->contains(std::string(key))) {
                p = &(*p)[std::string(key)];
            } else {
                LOG_F(WARNING, "Key not found: {}", key_path);
                return std::nullopt;
            }
        }
        return *p;
    } catch (const std::exception &e) {
        LOG_F(ERROR, "Error retrieving key {}: {}", key_path, e.what());
        return std::nullopt;
    }
}

auto ConfigManager::set(std::string_view key_path, const json &value) -> bool {
    return impl_->setOrAppendImpl(key_path, value, false);
}

auto ConfigManager::set(std::string_view key_path, json &&value) -> bool {
    return impl_->setOrAppendImpl(key_path, std::move(value), false);
}

auto ConfigManager::append(std::string_view key_path,
                           const json &value) -> bool {
    return impl_->setOrAppendImpl(key_path, value, true);
}

auto ConfigManager::remove(std::string_view key_path) -> bool {
    std::unique_lock lock(impl_->rwMutex);
    try {
        json *p = &impl_->config;
        std::vector<std::string> keys;

        for (const auto &key : splitString(key_path)) {
            if (!key.empty()) {
                keys.emplace_back(key);
            }
        }

        if (keys.empty()) {
            LOG_F(WARNING, "Invalid key path for deletion: {}", key_path);
            return false;
        }

        for (auto it = keys.begin(); it != keys.end(); ++it) {
            if (std::next(it) == keys.end()) {
                if (p->is_object() && p->contains(*it)) {
                    p->erase(*it);
                    impl_->notifyChanges(std::string(key_path));
                    LOG_F(INFO, "Deleted key: {}", key_path);
                    return true;
                }
                LOG_F(WARNING, "Key not found for deletion: {}", key_path);
                return false;
            }

            if (!p->is_object() || !p->contains(*it)) {
                LOG_F(WARNING, "Key not found for deletion: {}", key_path);
                return false;
            }
            p = &(*p)[*it];
        }

        return false;
    } catch (const std::exception &e) {
        LOG_F(ERROR, "Error removing key {}: {}", key_path, e.what());
        return false;
    }
}

auto ConfigManager::has(std::string_view key_path) const -> bool {
    return get(key_path).has_value();
}

// Implementation of the new callback system
size_t ConfigManager::onChanged(
    std::function<void(std::string_view path)> callback) {
    if (!callback) {
        LOG_F(WARNING, "Attempted to register null callback");
        return 0;
    }

    std::unique_lock lock(impl_->callbackMutex);
    size_t id = impl_->nextCallbackId++;
    impl_->callbacks.push_back({id, std::move(callback)});
    return id;
}

bool ConfigManager::removeCallback(size_t id) {
    if (id == 0)
        return false;

    std::unique_lock lock(impl_->callbackMutex);
    auto it = std::find_if(impl_->callbacks.begin(), impl_->callbacks.end(),
                           [id](const auto &cb) { return cb.id == id; });

    if (it != impl_->callbacks.end()) {
        impl_->callbacks.erase(it);
        return true;
    }
    return false;
}

void ConfigManager::tidy() {
    std::unique_lock lock(impl_->rwMutex);
    json updatedConfig;
    for (const auto &[key, value] : impl_->config.items()) {
        json *p = &updatedConfig;
        for (const auto &subKey : key | std::views::split('/')) {
            std::string subKeyStr = std::string(subKey.begin(), subKey.end());
            if (!p->contains(subKeyStr)) {
                (*p)[subKeyStr] = json::object();
            }
            p = &(*p)[subKeyStr];
        }
        *p = value;
        LOG_F(INFO, "Tidied key: {} with value: {}", key, value.dump());
    }
    impl_->config = std::move(updatedConfig);
    LOG_F(INFO, "Config tidied.");
}

void ConfigManager::merge(const json &src, json &target) {
    for (auto it = src.begin(); it != src.end(); ++it) {
        LOG_F(INFO, "Merge config: {}", it.key());
        if (it->is_object() && target.contains(it.key()) &&
            target[it.key()].is_object()) {
            merge(*it, target[it.key()]);
        } else {
            target[it.key()] = *it;
        }
    }
}

void ConfigManager::merge(const json &src) {
    merge(src, impl_->config);
    LOG_F(INFO, "Config merged.");
}

void ConfigManager::clear() {
    std::unique_lock lock(impl_->rwMutex);
    impl_->config.clear();
    LOG_F(INFO, "Config cleared.");
}

// Use C++20 ranges for better performance and cleaner code
auto ConfigManager::getKeys() const -> std::vector<std::string> {
    std::shared_lock lock(impl_->rwMutex);
    std::vector<std::string> paths;

    // More efficient recursive lambda with C++20 features
    std::function<void(const json &, std::string)> extractPaths =
        [&](const json &j, std::string path) {
            // Use ranges and structured bindings for cleaner iteration
            for (const auto &[key, value] : j.items()) {
                std::string currentPath = path.empty() ? key : path + "/" + key;

                if (value.is_object()) {
                    extractPaths(value, currentPath);
                } else {
                    paths.push_back(currentPath);
                }
            }
        };

    extractPaths(impl_->config, "");
    return paths;
}

auto ConfigManager::listPaths() const -> std::vector<std::string> {
    try {
        std::shared_lock lock(impl_->rwMutex);
        std::weak_ptr<atom::utils::Env> envPtr;
        GET_OR_CREATE_WEAK_PTR(envPtr, atom::utils::Env,
                               Constants::ENVIRONMENT);
        auto env = envPtr.lock();

        if (!env) {
            LOG_F(ERROR, "Failed to get environment instance");
            return {};
        }

        // Get the config directory with better error handling
        auto configDir = env->get("config");
        if (configDir.empty()) {
            configDir = env->getEnv("LITHIUM_CONFIG_DIR", "./config");
            LOG_F(INFO, "Using environment config directory: {}", configDir);
        }

        if (!atom::io::isFolderExists(configDir)) {
            LOG_F(WARNING, "Config directory does not exist: {}", configDir);
            // Try to create the directory
            try {
                if (fs::create_directories(configDir)) {
                    LOG_F(INFO, "Created config directory: {}", configDir);
                } else {
                    LOG_F(ERROR, "Failed to create config directory: {}",
                          configDir);
                    return {};
                }
            } catch (const fs::filesystem_error &e) {
                LOG_F(ERROR, "Filesystem error creating config directory: {}",
                      e.what());
                return {};
            }
        }

        // Check for more file types and use enhanced error handling
        auto paths = atom::io::checkFileTypeInFolder(
            configDir, {".json", ".json5", ".lithium", ".lithium5"},
            atom::io::FileOption::PATH);

        LOG_F(INFO, "Found {} configuration files", paths.size());
        return paths;
    } catch (const std::exception &e) {
        LOG_F(ERROR, "Error listing configuration paths: {}", e.what());
        return {};
    }
}
}  // namespace lithium
