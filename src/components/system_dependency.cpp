#include "system_dependency.hpp"

#include <algorithm>
#include <fstream>
#include <future>
#include <list>
#include <map>
#include <mutex>
#include <ranges>
#include <regex>
#include <sstream>
#include <unordered_map>
#include <shared_mutex>
#include <optional>
#include <variant>
#include <atomic>

#if defined(__linux__)
#define PLATFORM_LINUX
#elif defined(__APPLE__)
#define PLATFORM_MAC
#elif defined(_WIN32) || defined(_WIN64)
#define PLATFORM_WINDOWS
#else
#error "Unsupported platform"
#endif

#include "atom/async/pool.hpp"
#include "atom/function/global_ptr.hpp"
#include "atom/log/loguru.hpp"
#include "atom/system/command.hpp"
#include "atom/type/json.hpp"

#include "constant/constant.hpp"

namespace lithium {
using json = nlohmann::json;

std::ostream& operator<<(std::ostream& os, const VersionInfo& vi) {
    os << vi.major << "." << vi.minor << "." << vi.patch;
    if (!vi.prerelease.empty()) {
        os << "-" << vi.prerelease;
    }
    return os;
}

class DependencyManager::Impl {
public:
    explicit Impl(const std::string& configPath) {
        detectPlatform();
        loadPackageManagerConfig(configPath);
        initializeCache();
    }

    ~Impl() { saveCacheToFile(); }

    void checkAndInstallDependencies() {
        auto threadPool =
            GetPtr<atom::async::ThreadPool<>>(Constants::THREAD_POOL).value();
        if (!threadPool) {
            LOG_F(ERROR, "Failed to get thread pool");
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
                fut.get();
            }
        }
    }

    void installDependencyAsync(const DependencyInfo& dep) {
        std::lock_guard lock(asyncMutex_);
        asyncFutures_.emplace_back(std::async(
            std::launch::async, [this, dep]() { installDependency(dep); }));
    }

    void cancelInstallation(const std::string& depName) {
        std::lock_guard lock(asyncMutex_);

        const std::unordered_map<std::string, std::string> pkgMgrProcessMap = {
            {"apt", "apt"},       {"dnf", "dnf"},      {"pacman", "pacman"},
            {"zypper", "zypper"}, {"brew", "brew"},    {"choco", "choco"},
            {"scoop", "scoop"},   {"winget", "winget"}};

#ifdef PLATFORM_LINUX
        for (const auto& [mgr, proc] : pkgMgrProcessMap) {
            std::string cmd = "pkill -9 " + proc;
            auto result = atom::system::executeCommandWithStatus(cmd);
            if (result.second == 0) {
                LOG_F(INFO, "Successfully terminated {} process", proc);
            }
        }
#elif defined(PLATFORM_WINDOWS)
        for (const auto& [mgr, proc] : pkgMgrProcessMap) {
            std::string cmd = "taskkill /F /IM " + proc + ".exe";
            auto result = atom::system::executeCommandWithStatus(cmd);
            if (result.second == 0) {
                LOG_F(INFO, "Successfully terminated {} process", proc);
            }
        }
#endif

        LOG_F(INFO, "Attempting to cancel installation for: {}", depName);
    }

    void setCustomInstallCommand(const std::string& dep,
                                 const std::string& command) {
        customInstallCommands_[dep] = command;
    }

    auto generateDependencyReport() -> std::string {
        std::ostringstream report;
        for (const auto& dep : dependencies_) {
            report << "Dependency: " << dep.name;
            if (!dep.version.major) {
                report << ", Version: " << dep.version;
            }
            report << ", Package Manager: " << dep.packageManager << "\n";
        }
        return report.str();
    }

    void uninstallDependency(const std::string& depName) {
        auto it = std::ranges::find_if(
            dependencies_,
            [&](const DependencyInfo& info) { return info.name == depName; });
        if (it == dependencies_.end()) {
            LOG_F(WARNING, "Dependency {} not managed.", depName);
            return;
        }

        if (!isDependencyInstalled(*it)) {
            LOG_F(INFO, "Dependency {} is not installed.", depName);
            return;
        }

        try {
            auto pkgMgr = getPackageManager(it->packageManager);
            if (!pkgMgr) {
                throw DependencyException("Package manager not found.");
            }
            auto res = atom::system::executeCommandWithStatus(
                pkgMgr->getUninstallCommand(*it));
            if (res.second != 0) {
                throw DependencyException("Failed to uninstall dependency.");
            }
            installedCache_[depName] = false;
            LOG_F(INFO, "Uninstalled dependency: {}", depName);
        } catch (const DependencyException& ex) {
            LOG_F(ERROR, "Error uninstalling {}: {}", depName, ex.what());
        }
    }

    auto getCurrentPlatform() const -> std::string { return platform_; }

    void addDependency(const DependencyInfo& dep) {
        std::lock_guard lock(cacheMutex_);
        dependencies_.emplace_back(dep);
        installedCache_.emplace(dep.name, false);
        LOG_F(INFO, "Added dependency: {}", dep.name);
    }

    void removeDependency(const std::string& depName) {
        std::lock_guard lock(cacheMutex_);
        dependencies_.erase(
            std::ranges::remove_if(
                dependencies_,
                [&](const DependencyInfo& dep) { return dep.name == depName; })
                .begin(),
            dependencies_.end());
        installedCache_.erase(depName);
        LOG_F(INFO, "Removed dependency: {}", depName);
    }

    auto searchDependency(const std::string& depName)
        -> std::vector<std::string> {
        std::vector<std::string> results;
        for (const auto& pkgMgr : packageManagers_) {
            auto res = atom::system::executeCommandWithStatus(
                pkgMgr.getSearchCommand(depName));
            if (res.second != 0) {
                LOG_F(ERROR, "Failed to search for dependency: {}", depName);
                continue;
            }
            std::istringstream iss(res.first);
            std::string line;
            while (std::getline(iss, line)) {
                results.emplace_back(line);
            }
        }
        return results;
    }

    void loadSystemPackageManagers() {
#ifdef PLATFORM_LINUX
        packageManagers_.emplace_back(PackageManagerInfo{
            "apt",
            [](const DependencyInfo& dep) -> std::string {
                return "dpkg -l " + dep.name;
            },
            [&](const DependencyInfo& dep) -> std::string {
                if (customInstallCommands_.contains(dep.name)) {
                    return customInstallCommands_[dep.name];
                }
                return "sudo apt-get install -y " + dep.name;
            },
            [](const DependencyInfo& dep) -> std::string {
                return "sudo apt-get remove -y " + dep.name;
            },
            [](const std::string& dep) -> std::string {
                return "apt-cache search " + dep;
            }});

        packageManagers_.emplace_back(PackageManagerInfo{
            "dnf",
            [](const DependencyInfo& dep) -> std::string {
                return "rpm -q " + dep.name;
            },
            [&](const DependencyInfo& dep) -> std::string {
                if (customInstallCommands_.contains(dep.name)) {
                    return customInstallCommands_[dep.name];
                }
                return "sudo dnf install -y " + dep.name;
            },
            [](const DependencyInfo& dep) -> std::string {
                return "sudo dnf remove -y " + dep.name;
            },
            [](const std::string& dep) -> std::string {
                return "dnf search " + dep;
            }});

        packageManagers_.emplace_back(PackageManagerInfo{
            "pacman",
            [](const DependencyInfo& dep) -> std::string {
                return "pacman -Qs " + dep.name;
            },
            [&](const DependencyInfo& dep) -> std::string {
                if (customInstallCommands_.contains(dep.name)) {
                    return customInstallCommands_[dep.name];
                }
                return "sudo pacman -S --noconfirm " + dep.name;
            },
            [](const DependencyInfo& dep) -> std::string {
                return "sudo pacman -R --noconfirm " + dep.name;
            },
            [](const std::string& dep) -> std::string {
                return "pacman -Ss " + dep;
            }});

        packageManagers_.emplace_back(PackageManagerInfo{
            "zypper",
            [](const DependencyInfo& dep) -> std::string {
                return "rpm -q " + dep.name;
            },
            [&](const DependencyInfo& dep) -> std::string {
                if (customInstallCommands_.contains(dep.name)) {
                    return customInstallCommands_[dep.name];
                }
                return "sudo zypper install -y " + dep.name;
            },
            [](const DependencyInfo& dep) -> std::string {
                return "sudo zypper remove -y " + dep.name;
            },
            [](const std::string& dep) -> std::string {
                return "zypper search " + dep;
            }});

        packageManagers_.emplace_back(PackageManagerInfo{
            "flatpak",
            [](const DependencyInfo& dep) -> std::string {
                return "flatpak list | grep " + dep.name;
            },
            [&](const DependencyInfo& dep) -> std::string {
                if (customInstallCommands_.contains(dep.name)) {
                    return customInstallCommands_[dep.name];
                }
                return "flatpak install -y " + dep.name;
            },
            [](const DependencyInfo& dep) -> std::string {
                return "flatpak uninstall -y " + dep.name;
            },
            [](const std::string& dep) -> std::string {
                return "flatpak search " + dep;
            }});

        packageManagers_.emplace_back(PackageManagerInfo{
            "snap",
            [](const DependencyInfo& dep) -> std::string {
                return "snap list " + dep.name;
            },
            [&](const DependencyInfo& dep) -> std::string {
                if (customInstallCommands_.contains(dep.name)) {
                    return customInstallCommands_[dep.name];
                }
                return "sudo snap install " + dep.name;
            },
            [](const DependencyInfo& dep) -> std::string {
                return "sudo snap remove " + dep.name;
            },
            [](const std::string& dep) -> std::string {
                return "snap find " + dep;
            }});
#endif

#ifdef PLATFORM_MAC
        packageManagers_.emplace_back(
            PackageManagerInfo{"brew",
                               [](const DependencyInfo& dep) -> std::string {
                                   return "brew list " + dep.name;
                               },
                               [&](const DependencyInfo& dep) -> std::string {
                                   if (customInstallCommands_.count(dep.name)) {
                                       return customInstallCommands_[dep.name];
                                   }
                                   return "brew install " + dep.name;
                               },
                               [](const DependencyInfo& dep) -> std::string {
                                   return "brew uninstall " + dep.name;
                               },
                               [](const std::string& dep) -> std::string {
                                   return "brew search " + dep;
                               }});

        packageManagers_.emplace_back(
            PackageManagerInfo{"port",
                               [](const DependencyInfo& dep) -> std::string {
                                   return "port installed " + dep.name;
                               },
                               [&](const DependencyInfo& dep) -> std::string {
                                   if (customInstallCommands_.count(dep.name)) {
                                       return customInstallCommands_[dep.name];
                                   }
                                   return "sudo port install " + dep.name;
                               },
                               [](const DependencyInfo& dep) -> std::string {
                                   return "sudo port uninstall " + dep.name;
                               },
                               [](const std::string& dep) -> std::string {
                                   return "port search " + dep;
                               }});
#endif

#ifdef PLATFORM_WINDOWS
        packageManagers_.emplace_back(
            PackageManagerInfo{"choco",
                               [](const DependencyInfo& dep) -> std::string {
                                   return "choco list --local-only " + dep.name;
                               },
                               [&](const DependencyInfo& dep) -> std::string {
                                   if (customInstallCommands_.count(dep.name)) {
                                       return customInstallCommands_[dep.name];
                                   }
                                   return "choco install " + dep.name + " -y";
                               },
                               [](const DependencyInfo& dep) -> std::string {
                                   return "choco uninstall " + dep.name + " -y";
                               },
                               [](const std::string& dep) -> std::string {
                                   return "choco search " + dep;
                               }});

        packageManagers_.emplace_back(
            PackageManagerInfo{"scoop",
                               [](const DependencyInfo& dep) -> std::string {
                                   return "scoop list " + dep.name;
                               },
                               [&](const DependencyInfo& dep) -> std::string {
                                   if (customInstallCommands_.count(dep.name)) {
                                       return customInstallCommands_[dep.name];
                                   }
                                   return "scoop install " + dep.name;
                               },
                               [](const DependencyInfo& dep) -> std::string {
                                   return "scoop uninstall " + dep.name;
                               },
                               [](const std::string& dep) -> std::string {
                                   return "scoop search " + dep;
                               }});

        packageManagers_.emplace_back(PackageManagerInfo{
            "winget",
            [](const DependencyInfo& dep) -> std::string {
                return "winget list " + dep.name;
            },
            [&](const DependencyInfo& dep) -> std::string {
                if (customInstallCommands_.count(dep.name)) {
                    return customInstallCommands_[dep.name];
                }
                return "winget install -e --id " + dep.name;
            },
            [](const DependencyInfo& dep) -> std::string {
                return "winget uninstall -e --id " + dep.name;
            },
            [](const std::string& dep) -> std::string {
                return "winget search " + dep;
            }});
#endif
    }

    auto getPackageManagers() const -> std::vector<PackageManagerInfo> {
        return packageManagers_;
    }

    void loadPackageManagerConfig(const std::string& configPath) {
        try {
            std::ifstream file(configPath);
            if (!file.is_open()) {
                throw DependencyException(
                    "Failed to open package manager config file");
            }

            json config;
            file >> config;

            for (const auto& pm : config["package_managers"]) {
                PackageManagerInfo info{
                    pm["name"].get<std::string>(),
                    [pm](const DependencyInfo& dep) -> std::string {
                        return pm["commands"]["check"].get<std::string>() +
                               " " + dep.name;
                    },
                    [pm](const DependencyInfo& dep) -> std::string {
                        return pm["commands"]["install"].get<std::string>() +
                               " " + dep.name;
                    },
                    [pm](const DependencyInfo& dep) -> std::string {
                        return pm["commands"]["uninstall"].get<std::string>() +
                               " " + dep.name;
                    },
                    [pm](const std::string& dep) -> std::string {
                        return pm["commands"]["search"].get<std::string>() +
                               " " + dep;
                    }};
                packageManagers_.push_back(info);
            }
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Failed to load package manager config: {}", e.what());
            throw DependencyException(
                "Failed to load package manager configuration");
        }
    }

    template <typename K, typename V>
    class LRUCache {
    public:
        explicit LRUCache(size_t capacity) : capacity_(capacity) {}

        void put(const K& key, const V& value) {
            std::lock_guard lock(mutex_);
            auto it = cacheItemsMap_.find(key);
            if (it != cacheItemsMap_.end()) {
                cacheItemsList_.erase(it->second);
                cacheItemsMap_.erase(it);
            }
            cacheItemsList_.emplace_front(key, value);
            cacheItemsMap_[key] = cacheItemsList_.begin();

            if (cacheItemsMap_.size() > capacity_) {
                auto last = cacheItemsList_.end();
                last--;
                cacheItemsMap_.erase(last->first);
                cacheItemsList_.pop_back();
            }
        }

        std::optional<V> get(const K& key) {
            std::lock_guard lock(mutex_);
            auto it = cacheItemsMap_.find(key);
            if (it == cacheItemsMap_.end()) {
                return std::nullopt;
            }
            cacheItemsList_.splice(cacheItemsList_.begin(), cacheItemsList_,
                                   it->second);
            return it->second->second;
        }

    private:
        std::mutex mutex_;
        size_t capacity_;
        std::list<std::pair<K, V>> cacheItemsList_;
        std::unordered_map<K, typename std::list<std::pair<K, V>>::iterator>
            cacheItemsMap_;
    };

    void initializeCache() { loadCacheFromFile(); }

    auto install(const std::string& name)
        -> std::future<DependencyResult<std::string>> {
        return std::async(
            std::launch::async,
            [this, name]() -> DependencyResult<std::string> {
                DependencyResult<std::string> result;
                try {
                    DependencyInfo dep{name, VersionInfo{},
                                       getDefaultPackageManager()};
                    installDependency(dep);
                } catch (const DependencyException& e) {
                    result.error = DependencyError(
                        DependencyErrorCode::INSTALL_FAILED, e.what());
                }
                return result;
            });
    }

    static auto parseVersion(const std::string& version) -> VersionInfo {
        std::regex version_regex(R"((\d+)\.(\d+)\.(\d+)(?:-(.+))?)");
        std::smatch matches;
        VersionInfo info;

        if (std::regex_match(version, matches, version_regex)) {
            info.major = std::stoi(matches[1]);
            info.minor = std::stoi(matches[2]);
            info.patch = std::stoi(matches[3]);
            if (matches.size() > 4 && matches[4].matched) {
                info.prerelease = matches[4];
            }
        }

        return info;
    }

    auto getDependencyGraph() const -> std::string {
        json graph;
        std::function<void(const std::string&, json&)> buildGraph =
            [&](const std::string& name, json& node) {
                auto it = std::ranges::find_if(dependencies_,
                                               [&](const DependencyInfo& dep) {
                                                   return dep.name == name;
                                               });

                if (it != dependencies_.end()) {
                    node["name"] = it->name;
                    node["version"] = it->version.major + "." +
                                      std::to_string(it->version.minor) + "." +
                                      std::to_string(it->version.patch) +
                                      (it->version.prerelease.empty()
                                           ? ""
                                           : "-" + it->version.prerelease);
                    node["children"] = json::array();

                    for (const auto& depName : it->dependencies) {
                        json child;
                        buildGraph(depName, child);
                        node["children"].push_back(child);
                    }
                }
            };

        graph["dependencies"] = json::array();
        for (const auto& dep : dependencies_) {
            if (std::ranges::none_of(dependencies_, [&](const auto& d) {
                    return std::ranges::find(d.dependencies, dep.name) !=
                           d.dependencies.end();
                })) {
                json node;
                buildGraph(dep.name, node);
                graph["dependencies"].push_back(node);
            }
        }

        return graph.dump(2);
    }

private:
    std::vector<DependencyInfo> dependencies_;
    std::unordered_map<std::string, bool> installedCache_;
    std::unordered_map<std::string, std::string> customInstallCommands_;
    std::shared_mutex cacheMutex_;
    std::mutex asyncMutex_;
    std::vector<std::future<void>> asyncFutures_;
    std::vector<PackageManagerInfo> packageManagers_;

    enum class DistroType {
        UNKNOWN,
        DEBIAN,
        REDHAT,
        ARCH,
        OPENSUSE,
        GENTOO,
        SLACKWARE,
        VOID,
        ALPINE,
        CLEAR,
        SOLUS,
        EMBEDDED,
        MACOS,
        WINDOWS
    };

    DistroType distroType_ = DistroType::UNKNOWN;
    std::string platform_;

    const std::string CACHE_FILE = "dependency_cache.json";

    void detectPlatform() {
#ifdef PLATFORM_LINUX
        std::ifstream osReleaseFile("/etc/os-release");
        std::string line;
        std::regex debianRegex(
            R"(ID=(?:debian|ubuntu|linuxmint|elementary|pop|zorin|deepin|kali|parrot|mx|raspbian))");
        std::regex redhatRegex(
            R"(ID=(?:fedora|rhel|centos|rocky|alma|oracle|scientific|amazon))");
        std::regex archRegex(
            R"(ID=(?:arch|manjaro|endeavouros|artix|garuda|blackarch))");
        std::regex suseRegex(
            R"(ID=(?:opensuse|opensuse-leap|opensuse-tumbleweed|suse|sled|sles))");
        std::regex gentooRegex(R"(ID=(?:gentoo|calculate|redcore|sabayon))");
        std::regex slackwareRegex(R"(ID=(?:slackware))");
        std::regex voidRegex(R"(ID=(?:void))");
        std::regex alpineRegex(R"(ID=(?:alpine))");
        std::regex clearRegex(R"(ID=(?:clear-linux-os))");
        std::regex solusRegex(R"(ID=(?:solus))");
        std::regex embeddedRegex(R"(ID=(?:openwrt|buildroot|yocto))");

        if (osReleaseFile.is_open()) {
            while (std::getline(osReleaseFile, line)) {
                if (std::regex_search(line, debianRegex)) {
                    distroType_ = DistroType::DEBIAN;
                    platform_ = "Debian-based Linux";
                    return;
                }
                if (std::regex_search(line, redhatRegex)) {
                    distroType_ = DistroType::REDHAT;
                    platform_ = "RedHat-based Linux";
                    return;
                }
                if (std::regex_search(line, archRegex)) {
                    distroType_ = DistroType::ARCH;
                    platform_ = "Arch-based Linux";
                    return;
                }
                if (std::regex_search(line, suseRegex)) {
                    distroType_ = DistroType::OPENSUSE;
                    platform_ = "SUSE Linux";
                    return;
                }
                if (std::regex_search(line, gentooRegex)) {
                    distroType_ = DistroType::GENTOO;
                    platform_ = "Gentoo-based Linux";
                    return;
                }
                if (std::regex_search(line, slackwareRegex)) {
                    distroType_ = DistroType::SLACKWARE;
                    platform_ = "Slackware Linux";
                    return;
                }
                if (std::regex_search(line, voidRegex)) {
                    distroType_ = DistroType::VOID;
                    platform_ = "Void Linux";
                    return;
                }
                if (std::regex_search(line, alpineRegex)) {
                    distroType_ = DistroType::ALPINE;
                    platform_ = "Alpine Linux";
                    return;
                }
                if (std::regex_search(line, clearRegex)) {
                    distroType_ = DistroType::CLEAR;
                    platform_ = "Clear Linux";
                    return;
                }
                if (std::regex_search(line, solusRegex)) {
                    distroType_ = DistroType::SOLUS;
                    platform_ = "Solus";
                    return;
                }
                if (std::regex_search(line, embeddedRegex)) {
                    distroType_ = DistroType::EMBEDDED;
                    platform_ = "Embedded Linux";
                    return;
                }
            }
        }
        distroType_ = DistroType::UNKNOWN;
        platform_ = "Unknown Linux";
#elif defined(PLATFORM_MAC)
        distroType_ = DistroType::MACOS;
        platform_ = "macOS";
#elif defined(PLATFORM_WINDOWS)
        distroType_ = DistroType::WINDOWS;
        platform_ = "Windows";
#else
        distroType_ = DistroType::UNKNOWN;
        platform_ = "Unknown";
#endif
    }

    void configurePackageManagers() {}

    bool isDependencyInstalled(const DependencyInfo& dep) {
        auto it = installedCache_.find(dep.name);
        return it != installedCache_.end() && it->second;
    }

    void installDependency(const DependencyInfo& dep) {
        try {
            auto pkgMgr = getPackageManager(dep.packageManager);
            if (!pkgMgr)
                throw DependencyException("Package manager not found.");
            if (!isDependencyInstalled(dep)) {
                auto res = atom::system::executeCommandWithStatus(
                    pkgMgr->getInstallCommand(dep));
                if (res.second != 0) {
                    throw DependencyException("Failed to install dependency.");
                }
                installedCache_[dep.name] = true;
                LOG_F(INFO, "Installed dependency: {}", dep.name);
            }
        } catch (const DependencyException& ex) {
            LOG_F(ERROR, "Error installing {}: {}", dep.name, ex.what());
        }
    }

    std::optional<PackageManagerInfo> getPackageManager(
        const std::string& name) const {
        auto it = std::ranges::find_if(
            packageManagers_,
            [&](const PackageManagerInfo& pm) { return pm.name == name; });
        if (it != packageManagers_.end()) {
            return *it;
        }
        return std::nullopt;
    }

    void loadCacheFromFile() {
        std::ifstream cacheFile(CACHE_FILE);
        if (!cacheFile.is_open()) {
            LOG_F(WARNING, "Cache file not found.");
            return;
        }
        json j;
        cacheFile >> j;
        for (const auto& dep : j["dependencies"]) {
            dependencies_.emplace_back(
                DependencyInfo{dep["name"].get<std::string>(),
                               parseVersion(dep.value("version", "0.0.0")),
                               dep.value("packageManager", "")});
            installedCache_[dep["name"].get<std::string>()] =
                dep.value("installed", false);
        }
    }

    void saveCacheToFile() const {
        std::ofstream cacheFile(CACHE_FILE);
        if (!cacheFile.is_open()) {
            LOG_F(ERROR, "Failed to open cache file for writing.");
            return;
        }
        json j;
        j["dependencies"] = json::array();
        for (const auto& dep : dependencies_) {
            j["dependencies"].push_back(
                json::object({{"name", dep.name},
                              {"version",
                               {{"major", dep.version.major},
                                {"minor", dep.version.minor},
                                {"patch", dep.version.patch},
                                {"prerelease", dep.version.prerelease}}},
                              {"packageManager", dep.packageManager},
                              {"installed", installedCache_.at(dep.name)}}));
        }
        cacheFile << j.dump(4);
    }

    LRUCache<std::string, bool> installationCache_{100};

    auto getDefaultPackageManager() const -> std::string {
        switch (distroType_) {
            case DistroType::DEBIAN:
                return "apt";
            case DistroType::REDHAT:
                return "dnf";
            case DistroType::ARCH:
                return "pacman";
            case DistroType::MACOS:
                return "brew";
            case DistroType::WINDOWS:
                return "choco";
            default:
                return "apt";
        }
    }
};

DependencyManager::DependencyManager(const std::string& configPath)
    : pImpl_(std::make_unique<Impl>(configPath)) {}

auto DependencyManager::install(const std::string& name)
    -> std::future<DependencyResult<std::string>> {
    return pImpl_->install(name);
}

DependencyManager::~DependencyManager() = default;

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

}  // namespace lithium
