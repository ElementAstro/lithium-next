#include "dependency_manager.hpp"

#include <algorithm>
#include <fstream>
#include <future>
#include <mutex>
#include <ranges>
#include <regex>
#include <shared_mutex>
#include <sstream>
#include <unordered_map>

#include <spdlog/spdlog.h>
#include "atom/async/pool.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/search/lru.hpp"
#include "atom/system/command.hpp"
#include "atom/type/json.hpp"

#undef VOID

#include "constant/constant.hpp"
#include "package_manager.hpp"
#include "platform_detector.hpp"

namespace lithium::system {

static std::string versionToString(const VersionInfo& vi) {
    std::ostringstream oss;
    oss << vi;
    return oss.str();
}

using json = nlohmann::json;
using LRUCacheType = atom::search::ThreadSafeLRUCache<std::string, bool>;

class DependencyManager::Impl {
public:
    explicit Impl(const std::string& configPath)
        : platformDetector_(),
          packageRegistry_(platformDetector_),
          installationCache_(100) {
        packageRegistry_.loadPackageManagerConfig(configPath);
        initializeCache();
    }

    ~Impl() { saveCacheToFile(); }

    void checkAndInstallDependencies() {
        auto threadPool =
            GetPtr<atom::async::ThreadPool<>>(Constants::THREAD_POOL).value();
        if (!threadPool) {
            spdlog::error("Failed to get thread pool");
            return;
        }

        std::vector<std::future<void>> futures;
        futures.reserve(dependencies_.size());

        for (const auto& dep : dependencies_) {
            futures.emplace_back(
                threadPool->enqueue([this, dep]() { installDependency(dep); }));
        }

        for (auto& fut : futures) {
            if (fut.valid()) {
                fut.wait();
            }
        }
    }

    void installDependencyAsync(const DependencyInfo& dep) {
        {
            std::unique_lock lock(cacheMutex_);
            std::future<void> fut = std::async(
                std::launch::async, [this, dep]() { installDependency(dep); });
            asyncFutures_.emplace_back(std::move(fut));
        }
    }

    void cancelInstallation(const std::string& depName) {
        packageRegistry_.cancelInstallation(depName);
    }

    void setCustomInstallCommand(const std::string& dep,
                                 const std::string& command) {
        std::unique_lock lock(cacheMutex_);
        customInstallCommands_[dep] = command;
    }

    auto generateDependencyReport() -> std::string {
        std::ostringstream report;
        std::shared_lock lock(cacheMutex_);

        for (const auto& dep : dependencies_) {
            report << "Dependency: " << dep.name;
            if (dep.version.major != 0) {
                report << " v" << dep.version;
            }
            report << ", Package Manager: " << dep.packageManager << "\n";
        }
        return report.str();
    }

    void uninstallDependency(const std::string& depName) {
        std::lock_guard lock(cacheMutex_);

        auto it = std::ranges::find_if(
            dependencies_,
            [&](const DependencyInfo& info) { return info.name == depName; });

        if (it == dependencies_.end()) {
            spdlog::warn("Dependency {} not managed", depName);
            return;
        }

        if (!isDependencyInstalled(*it)) {
            spdlog::info("Dependency {} is not installed", depName);
            return;
        }

        try {
            auto pkgMgr =
                packageRegistry_.getPackageManager(it->packageManager);
            if (!pkgMgr) {
                throw DependencyException(
                    DependencyErrorCode::PACKAGE_MANAGER_NOT_FOUND,
                    "Package manager not found: " + it->packageManager);
            }

            auto res = atom::system::executeCommandWithStatus(
                pkgMgr->getUninstallCommand(*it));
            if (res.second != 0) {
                throw DependencyException(DependencyErrorCode::UNINSTALL_FAILED,
                                          "Failed to uninstall " + depName);
            }

            installedCache_[depName] = false;
            installationCache_.put(depName, false);
            spdlog::info("Uninstalled dependency: {}", depName);
        } catch (const DependencyException& ex) {
            spdlog::error("Error uninstalling {}: {}", depName, ex.what());
        }
    }

    auto getCurrentPlatform() const -> std::string {
        return platformDetector_.getCurrentPlatform();
    }

    void addDependency(const DependencyInfo& dep) {
        {
            std::unique_lock lock(cacheMutex_);
            dependencies_.emplace_back(dep);
            installedCache_.emplace(dep.name, false);
        }
        spdlog::info("Added dependency: {}", dep.name);
    }

    void removeDependency(const std::string& depName) {
        {
            std::unique_lock lock(cacheMutex_);
            auto removed = std::ranges::remove_if(
                dependencies_,
                [&](const DependencyInfo& dep) { return dep.name == depName; });
            dependencies_.erase(removed.begin(), removed.end());
            installedCache_.erase(depName);
            installationCache_.erase(depName);
        }
        spdlog::info("Removed dependency: {}", depName);
    }

    auto searchDependency(const std::string& depName)
        -> std::vector<std::string> {
        return packageRegistry_.searchDependency(depName);
    }

    void loadSystemPackageManagers() {
        packageRegistry_.loadSystemPackageManagers();
    }

    auto getPackageManagers() const -> std::vector<PackageManagerInfo> {
        return packageRegistry_.getPackageManagers();
    }

    void initializeCache() { loadCacheFromFile(); }

    auto install(const std::string& name)
        -> std::future<DependencyResult<std::string>> {
        return std::async(
            std::launch::async,
            [this, name]() -> DependencyResult<std::string> {
                try {
                    if (auto cached = installationCache_.get(name);
                        cached.has_value()) {
                        if (cached.value()) {
                            return DependencyResult<std::string>{name,
                                                                 std::nullopt};
                        }
                    }

                    auto it = std::ranges::find_if(
                        dependencies_, [&](const DependencyInfo& dep) {
                            return dep.name == name;
                        });

                    if (it == dependencies_.end()) {
                        return DependencyResult<std::string>{
                            std::nullopt,
                            DependencyError{
                                DependencyErrorCode::DEPENDENCY_NOT_FOUND,
                                "Dependency not found: " + name}};
                    }

                    installDependency(*it);
                    installationCache_.put(name, true);
                    return DependencyResult<std::string>{name, std::nullopt};
                } catch (const std::exception& e) {
                    return DependencyResult<std::string>{
                        std::nullopt,
                        DependencyError{DependencyErrorCode::INSTALL_FAILED,
                                        e.what()}};
                }
            });
    }

    auto installWithVersion(const std::string& name, const std::string& version)
        -> std::future<DependencyResult<void>> {
        return std::async(
            std::launch::async,
            [this, name, version]() -> DependencyResult<void> {
                try {
                    DependencyInfo dep;
                    dep.name = name;
                    dep.version = parseVersion(version);

                    installDependency(dep);
                    return DependencyResult<void>{true, std::nullopt};
                } catch (const std::exception& e) {
                    return DependencyResult<void>{
                        false,
                        DependencyError{DependencyErrorCode::INSTALL_FAILED,
                                        e.what()}};
                }
            });
    }

    auto installMultiple(const std::vector<std::string>& deps)
        -> std::vector<std::future<DependencyResult<void>>> {
        std::vector<std::future<DependencyResult<void>>> futures;
        futures.reserve(deps.size());

        for (const auto& dep : deps) {
            futures.emplace_back(std::async(
                std::launch::async, [this, dep]() -> DependencyResult<void> {
                    try {
                        auto result = install(dep).get();
                        if (result.error) {
                            return DependencyResult<void>{false, result.error};
                        }
                        return DependencyResult<void>{true, std::nullopt};
                    } catch (const std::exception& e) {
                        return DependencyResult<void>{
                            false,
                            DependencyError{DependencyErrorCode::INSTALL_FAILED,
                                            e.what()}};
                    }
                }));
        }

        return futures;
    }

    auto checkVersionCompatibility(const std::string& name,
                                   const std::string& version)
        -> DependencyResult<bool> {
        try {
            auto it = std::ranges::find_if(
                dependencies_,
                [&](const DependencyInfo& dep) { return dep.name == name; });

            if (it == dependencies_.end()) {
                return DependencyResult<bool>{
                    false,
                    DependencyError{DependencyErrorCode::DEPENDENCY_NOT_FOUND,
                                    "Dependency not found: " + name}};
            }

            auto reqVersion = parseVersion(version);
            return DependencyResult<bool>{it->version >= reqVersion,
                                          std::nullopt};
        } catch (const std::exception& e) {
            return DependencyResult<bool>{
                false, DependencyError{DependencyErrorCode::INVALID_VERSION,
                                       e.what()}};
        }
    }

    auto verifyDependencies() -> std::future<DependencyResult<bool>> {
        return std::async(
            std::launch::async, [this]() -> DependencyResult<bool> {
                try {
                    std::shared_lock lock(cacheMutex_);

                    for (const auto& dep : dependencies_) {
                        if (!isDependencyInstalled(dep)) {
                            return DependencyResult<bool>{false, std::nullopt};
                        }
                    }
                    return DependencyResult<bool>{true, std::nullopt};
                } catch (const std::exception& e) {
                    return DependencyResult<bool>{
                        false,
                        DependencyError{DependencyErrorCode::UNKNOWN_ERROR,
                                        e.what()}};
                }
            });
    }

    auto exportConfig() const -> DependencyResult<std::string> {
        try {
            json config;
            config["dependencies"] = json::array();

            std::shared_lock lock(cacheMutex_);
            for (const auto& dep : dependencies_) {
                json depJson;
                depJson["name"] = dep.name;
                depJson["version"] = versionToString(dep.version);
                depJson["package_manager"] = dep.packageManager;
                config["dependencies"].push_back(depJson);
            }

            return DependencyResult<std::string>{config.dump(2), std::nullopt};
        } catch (const std::exception& e) {
            return DependencyResult<std::string>{
                std::nullopt,
                DependencyError{DependencyErrorCode::CONFIG_LOAD_FAILED,
                                e.what()}};
        }
    }

    auto importConfig(const std::string& config) -> DependencyResult<void> {
        try {
            auto j = json::parse(config);

            std::lock_guard lock(cacheMutex_);
            dependencies_.clear();

            for (const auto& depJson : j["dependencies"]) {
                DependencyInfo dep;
                dep.name = depJson["name"];
                dep.version = parseVersion(depJson["version"]);
                dep.packageManager = depJson.value("package_manager", "");
                dependencies_.push_back(dep);
            }

            return DependencyResult<void>{true, std::nullopt};
        } catch (const std::exception& e) {
            return DependencyResult<void>{
                false, DependencyError{DependencyErrorCode::CONFIG_LOAD_FAILED,
                                       e.what()}};
        }
    }

    static auto parseVersion(const std::string& version) -> VersionInfo {
        std::regex version_regex(R"((\d+)\.(\d+)\.(\d+)(?:-(.+))?)");
        std::smatch matches;
        VersionInfo info;

        if (std::regex_match(version, matches, version_regex)) {
            info.major = std::stoi(matches[1]);
            info.minor = std::stoi(matches[2]);
            info.patch = std::stoi(matches[3]);
            if (matches[4].matched) {
                info.prerelease = matches[4];
            }
        }

        return info;
    }

    auto getDependencyGraph() -> std::string {
        json graph;
        std::shared_lock lock(cacheMutex_);

        graph["dependencies"] = json::array();
        for (const auto& dep : dependencies_) {
            json depNode;
            depNode["name"] = dep.name;
            depNode["version"] = versionToString(dep.version);
            depNode["installed"] = isDependencyInstalled(dep);
            graph["dependencies"].push_back(depNode);
        }

        return graph.dump(2);
    }

private:
    std::vector<DependencyInfo> dependencies_;
    std::unordered_map<std::string, bool> installedCache_;
    std::unordered_map<std::string, std::string> customInstallCommands_;
    mutable std::shared_mutex cacheMutex_;
    std::mutex asyncMutex_;
    std::vector<std::future<void>> asyncFutures_;

    PlatformDetector platformDetector_;
    PackageManagerRegistry packageRegistry_;

    const std::string CACHE_FILE = "dependency_cache.json";
    LRUCacheType installationCache_;

    bool isDependencyInstalled(const DependencyInfo& dep) {
        if (auto cached = installationCache_.get(dep.name);
            cached.has_value()) {
            return cached.value();
        }
        std::shared_lock lock(cacheMutex_);
        auto it = installedCache_.find(dep.name);
        return it != installedCache_.end() && it->second;
    }

    void installDependency(const DependencyInfo& dep) {
        try {
            auto pkgMgr =
                packageRegistry_.getPackageManager(dep.packageManager);
            if (!pkgMgr) {
                throw DependencyException(
                    DependencyErrorCode::PACKAGE_MANAGER_NOT_FOUND,
                    "Package manager not found: " + dep.packageManager);
            }

            if (isDependencyInstalled(dep)) {
                spdlog::info("Dependency {} already installed", dep.name);
                return;
            }

            std::string command;
            {
                std::shared_lock lock(cacheMutex_);
                auto it = customInstallCommands_.find(dep.name);
                if (it != customInstallCommands_.end()) {
                    command = it->second;
                } else {
                    command = pkgMgr->getInstallCommand(dep);
                }
            }

            auto res = atom::system::executeCommandWithStatus(command);
            if (res.second != 0) {
                throw DependencyException(
                    DependencyErrorCode::INSTALL_FAILED,
                    "Failed to install " + dep.name + ": " + res.first);
            }

            {
                std::unique_lock lock(cacheMutex_);
                installedCache_[dep.name] = true;
            }
            installationCache_.put(dep.name, true);
            spdlog::info("Installed dependency: {}", dep.name);
        } catch (const DependencyException& ex) {
            spdlog::error("Error installing {}: {}", dep.name, ex.what());
            throw;
        }
    }

    void loadCacheFromFile() {
        std::ifstream cacheFile(CACHE_FILE);
        if (!cacheFile.is_open()) {
            spdlog::info("No cache file found, starting fresh");
            return;
        }

        try {
            json j;
            cacheFile >> j;

            std::lock_guard lock(cacheMutex_);
            for (const auto& dep : j["dependencies"]) {
                std::string name = dep["name"];
                bool installed = dep["installed"];
                installedCache_[name] = installed;
                installationCache_.put(name, installed);
            }
            spdlog::info("Loaded dependency cache from file");
        } catch (const std::exception& e) {
            spdlog::error("Failed to load cache file: {}", e.what());
        }
    }

    void saveCacheToFile() const {
        std::ofstream cacheFile(CACHE_FILE);
        if (!cacheFile.is_open()) {
            spdlog::error("Could not save cache file");
            return;
        }

        try {
            json j;
            j["dependencies"] = json::array();

            std::shared_lock lock(cacheMutex_);
            for (const auto& dep : dependencies_) {
                json depJson;
                depJson["name"] = dep.name;
                depJson["installed"] = installedCache_.count(dep.name) &&
                                       installedCache_.at(dep.name);
                j["dependencies"].push_back(depJson);
            }

            cacheFile << j.dump(4);
            spdlog::debug("Saved dependency cache to file");
        } catch (const std::exception& e) {
            spdlog::error("Failed to save cache file: {}", e.what());
        }
    }
};

DependencyManager::DependencyManager(const std::string& configPath)
    : pImpl_(std::make_unique<Impl>(configPath)) {}

DependencyManager::~DependencyManager() = default;

auto DependencyManager::install(const std::string& name)
    -> std::future<DependencyResult<std::string>> {
    return pImpl_->install(name);
}

auto DependencyManager::installWithVersion(const std::string& name,
                                           const std::string& version)
    -> std::future<DependencyResult<void>> {
    return pImpl_->installWithVersion(name, version);
}

auto DependencyManager::installMultiple(const std::vector<std::string>& deps)
    -> std::vector<std::future<DependencyResult<void>>> {
    return pImpl_->installMultiple(deps);
}

auto DependencyManager::checkVersionCompatibility(const std::string& name,
                                                  const std::string& version)
    -> DependencyResult<bool> {
    return pImpl_->checkVersionCompatibility(name, version);
}

auto DependencyManager::getDependencyGraph() const -> std::string {
    return pImpl_->getDependencyGraph();
}

auto DependencyManager::verifyDependencies()
    -> std::future<DependencyResult<bool>> {
    return pImpl_->verifyDependencies();
}

auto DependencyManager::exportConfig() const -> DependencyResult<std::string> {
    return pImpl_->exportConfig();
}

auto DependencyManager::importConfig(const std::string& config)
    -> DependencyResult<void> {
    return pImpl_->importConfig(config);
}

void DependencyManager::checkAndInstallDependencies() {
    pImpl_->checkAndInstallDependencies();
}

void DependencyManager::installDependencyAsync(const DependencyInfo& dep) {
    pImpl_->installDependencyAsync(dep);
}

void DependencyManager::cancelInstallation(const std::string& dep) {
    pImpl_->cancelInstallation(dep);
}

void DependencyManager::setCustomInstallCommand(const std::string& dep,
                                                const std::string& command) {
    pImpl_->setCustomInstallCommand(dep, command);
}

auto DependencyManager::generateDependencyReport() const -> std::string {
    return pImpl_->generateDependencyReport();
}

void DependencyManager::uninstallDependency(const std::string& dep) {
    pImpl_->uninstallDependency(dep);
}

auto DependencyManager::getCurrentPlatform() const -> std::string {
    return pImpl_->getCurrentPlatform();
}

void DependencyManager::addDependency(const DependencyInfo& dep) {
    pImpl_->addDependency(dep);
}

void DependencyManager::removeDependency(const std::string& depName) {
    pImpl_->removeDependency(depName);
}

auto DependencyManager::searchDependency(const std::string& depName)
    -> std::vector<std::string> {
    return pImpl_->searchDependency(depName);
}

void DependencyManager::loadSystemPackageManagers() {
    pImpl_->loadSystemPackageManagers();
}

auto DependencyManager::getPackageManagers() const
    -> std::vector<PackageManagerInfo> {
    return pImpl_->getPackageManagers();
}

}  // namespace lithium::system
