#include "package_manager.hpp"
#include <spdlog/spdlog.h>
#include "atom/system/platform.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

#if defined(_WIN32)
// clang-format off
#include <windows.h>
#include <tlhelp32.h>
// clang-format on
#elif defined(__APPLE__)
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#elif defined(__linux__)
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

#include "atom/type/json.hpp"

namespace lithium::system {

using json = nlohmann::json;

namespace {

bool commandExists(const std::string& command) {
    std::string testCmd;
#if defined(_WIN32)
    testCmd = "where " + command + " >nul 2>nul";
#else
    testCmd = "which " + command + " >/dev/null 2>&1";
#endif
    return std::system(testCmd.c_str()) == 0;
}

std::string executeCommand(const std::string& command) {
    std::string result;
    result.reserve(1024);
    char buffer[256];

#if defined(_WIN32)
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif

    if (!pipe) {
        spdlog::error("Failed to execute command: {}", command);
        return result;
    }

    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

#if defined(_WIN32)
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    return result;
}

void killProcessByName(const std::string& processName) {
#if defined(_WIN32)
    std::string killCmd = "taskkill /F /IM " + processName + ".exe";
    std::system(killCmd.c_str());
#else
    std::string killCmd = "pkill -f " + processName;
    std::system(killCmd.c_str());
#endif
}

std::vector<int> getProcessIdsByName(const std::string& processName) {
    std::vector<int> pids;

#if defined(_WIN32)
    HANDLE hProcessSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hProcessSnap == INVALID_HANDLE_VALUE) {
        return pids;
    }

    PROCESSENTRY32 pe32;
    pe32.dwSize = sizeof(PROCESSENTRY32);

    if (Process32First(hProcessSnap, &pe32)) {
        do {
            std::string exeName = pe32.szExeFile;
            if (exeName.find(processName) != std::string::npos) {
                pids.push_back(pe32.th32ProcessID);
            }
        } while (Process32Next(hProcessSnap, &pe32));
    }

    CloseHandle(hProcessSnap);
#else
    std::string cmd = "pgrep -f " + processName;
    std::string output = executeCommand(cmd);

    std::istringstream iss(output);
    std::string line;
    while (std::getline(iss, line)) {
        if (!line.empty()) {
            try {
                pids.push_back(std::stoi(line));
            } catch (const std::exception&) {
                // Ignore invalid PIDs
            }
        }
    }
#endif

    return pids;
}

auto makeCheckCmd(const std::string& exe) {
    return [exe](const DependencyInfo&) {
#if defined(_WIN32)
        return "where " + exe + " >nul 2>nul";
#else
        return "which " + exe + " >/dev/null 2>&1";
#endif
    };
}

auto makeInstallCmd(const std::string& exe, const std::string& fmt) {
    return [exe, fmt](const DependencyInfo& dep) {
        std::string cmd = fmt;
        size_t pos = cmd.find("{}");
        if (pos != std::string::npos)
            cmd.replace(pos, 2, dep.name);
        return cmd;
    };
}

auto makeUninstallCmd(const std::string& exe, const std::string& fmt) {
    return [exe, fmt](const DependencyInfo& dep) {
        std::string cmd = fmt;
        size_t pos = cmd.find("{}");
        if (pos != std::string::npos)
            cmd.replace(pos, 2, dep.name);
        return cmd;
    };
}

auto makeSearchCmd(const std::string& exe, const std::string& fmt) {
    return [exe, fmt](const std::string& depName) {
        std::string cmd = fmt;
        size_t pos = cmd.find("{}");
        if (pos != std::string::npos)
            cmd.replace(pos, 2, depName);
        return cmd;
    };
}

const std::unordered_map<std::string, std::vector<std::string>>
    PKG_MGR_PROCESS_MAP = {{"apt", {"apt", "apt-get", "dpkg"}},
                           {"dnf", {"dnf", "yum"}},
                           {"pacman", {"pacman"}},
                           {"zypper", {"zypper"}},
                           {"yum", {"yum"}},
                           {"brew", {"brew"}},
                           {"port", {"port"}},
                           {"choco", {"choco", "chocolatey"}},
                           {"scoop", {"scoop"}},
                           {"winget", {"winget"}}};

}  // namespace

PackageManagerRegistry::PackageManagerRegistry(const PlatformDetector& detector)
    : platformDetector_(detector) {
    configurePackageManagers();
}

void PackageManagerRegistry::loadSystemPackageManagers() {
    spdlog::info("Loading system package managers for platform: {}",
                 ATOM_PLATFORM);

    packageManagers_.clear();
    packageManagers_.reserve(10);

    // Linux package managers
    constexpr auto isLinux = std::string_view(ATOM_PLATFORM) == "Linux";
    if constexpr (isLinux) {
        const std::vector<PackageManagerInfo> linuxPkgMgrs = {
            {"apt", makeCheckCmd("apt"),
             makeInstallCmd("apt", "apt install -y {}"),
             makeUninstallCmd("apt", "apt remove -y {}"),
             makeSearchCmd("apt", "apt search {}")},
            {"dnf", makeCheckCmd("dnf"),
             makeInstallCmd("dnf", "dnf install -y {}"),
             makeUninstallCmd("dnf", "dnf remove -y {}"),
             makeSearchCmd("dnf", "dnf search {}")},
            {"pacman", makeCheckCmd("pacman"),
             makeInstallCmd("pacman", "pacman -S --noconfirm {}"),
             makeUninstallCmd("pacman", "pacman -R --noconfirm {}"),
             makeSearchCmd("pacman", "pacman -Ss {}")},
            {"zypper", makeCheckCmd("zypper"),
             makeInstallCmd("zypper", "zypper install -y {}"),
             makeUninstallCmd("zypper", "zypper remove -y {}"),
             makeSearchCmd("zypper", "zypper search {}")},
            {"yum", makeCheckCmd("yum"),
             makeInstallCmd("yum", "yum install -y {}"),
             makeUninstallCmd("yum", "yum remove -y {}"),
             makeSearchCmd("yum", "yum search {}")}};

        for (const auto& pkgMgr : linuxPkgMgrs) {
            if (std::system(pkgMgr.getCheckCommand({}).c_str()) == 0) {
                packageManagers_.push_back(pkgMgr);
                spdlog::info("Found package manager: {}", pkgMgr.name);
            }
        }
    }

    // macOS package managers
    constexpr auto isMacOS = std::string_view(ATOM_PLATFORM) == "macOS";
    if constexpr (isMacOS) {
        const std::vector<PackageManagerInfo> macosPkgMgrs = {
            {"brew", makeCheckCmd("brew"),
             makeInstallCmd("brew", "brew install {}"),
             makeUninstallCmd("brew", "brew uninstall {}"),
             makeSearchCmd("brew", "brew search {}")},
            {"port", makeCheckCmd("port"),
             makeInstallCmd("port", "port install {}"),
             makeUninstallCmd("port", "port uninstall {}"),
             makeSearchCmd("port", "port search {}")}};

        for (const auto& pkgMgr : macosPkgMgrs) {
            if (std::system(pkgMgr.getCheckCommand({}).c_str()) == 0) {
                packageManagers_.push_back(pkgMgr);
                spdlog::info("Found package manager: {}", pkgMgr.name);
            }
        }
    }

    // Windows package managers
    constexpr auto isWindows =
        std::string_view(ATOM_PLATFORM).find("Windows") !=
        std::string_view::npos;
    if constexpr (isWindows) {
        const std::vector<PackageManagerInfo> windowsPkgMgrs = {
            {"choco", makeCheckCmd("choco"),
             makeInstallCmd("choco", "choco install -y {}"),
             makeUninstallCmd("choco", "choco uninstall -y {}"),
             makeSearchCmd("choco", "choco search {}")},
            {"scoop", makeCheckCmd("scoop"),
             makeInstallCmd("scoop", "scoop install {}"),
             makeUninstallCmd("scoop", "scoop uninstall {}"),
             makeSearchCmd("scoop", "scoop search {}")},
            {"winget", makeCheckCmd("winget"),
             makeInstallCmd("winget", "winget install --id {} --silent"),
             makeUninstallCmd("winget", "winget uninstall --id {}"),
             makeSearchCmd("winget", "winget search {}")}};

        for (const auto& pkgMgr : windowsPkgMgrs) {
            if (std::system(pkgMgr.getCheckCommand({}).c_str()) == 0) {
                packageManagers_.push_back(pkgMgr);
                spdlog::info("Found package manager: {}", pkgMgr.name);
            }
        }
    }

    spdlog::info("Loaded {} package managers", packageManagers_.size());
}

void PackageManagerRegistry::loadPackageManagerConfig(
    const std::string& configPath) {
    try {
        if (!std::filesystem::exists(configPath)) {
            spdlog::warn("Package manager config file does not exist: {}",
                         configPath);
            return;
        }

        std::ifstream configFile(configPath);
        if (!configFile.is_open()) {
            spdlog::warn("Could not open package manager config file: {}",
                         configPath);
            return;
        }

        json config;
        configFile >> config;

        if (!config.contains("package_managers")) {
            spdlog::warn("Config file missing 'package_managers' section");
            return;
        }

        for (const auto& pmConfig : config["package_managers"]) {
            PackageManagerInfo pkgMgr;

            if (pmConfig.contains("name")) {
                pkgMgr.name = pmConfig["name"];
            }
            if (pmConfig.contains("check_cmd")) {
                std::string checkCmd = pmConfig["check_cmd"];
                pkgMgr.getCheckCommand = [checkCmd](const DependencyInfo&) {
                    return checkCmd;
                };
            }
            if (pmConfig.contains("install_cmd")) {
                std::string installCmd = pmConfig["install_cmd"];
                pkgMgr.getInstallCommand =
                    [installCmd](const DependencyInfo& dep) {
                        std::string cmd = installCmd;
                        size_t pos = cmd.find("{}");
                        if (pos != std::string::npos)
                            cmd.replace(pos, 2, dep.name);
                        return cmd;
                    };
            }
            if (pmConfig.contains("uninstall_cmd")) {
                std::string uninstallCmd = pmConfig["uninstall_cmd"];
                pkgMgr.getUninstallCommand =
                    [uninstallCmd](const DependencyInfo& dep) {
                        std::string cmd = uninstallCmd;
                        size_t pos = cmd.find("{}");
                        if (pos != std::string::npos)
                            cmd.replace(pos, 2, dep.name);
                        return cmd;
                    };
            }
            if (pmConfig.contains("search_cmd")) {
                std::string searchCmd = pmConfig["search_cmd"];
                pkgMgr.getSearchCommand =
                    [searchCmd](const std::string& depName) {
                        std::string cmd = searchCmd;
                        size_t pos = cmd.find("{}");
                        if (pos != std::string::npos)
                            cmd.replace(pos, 2, depName);
                        return cmd;
                    };
            }

            if (pkgMgr.getCheckCommand &&
                std::system(pkgMgr.getCheckCommand({}).c_str()) == 0) {
                auto existingIt = std::find_if(
                    packageManagers_.begin(), packageManagers_.end(),
                    [&](const PackageManagerInfo& existing) {
                        return existing.name == pkgMgr.name;
                    });

                if (existingIt != packageManagers_.end()) {
                    *existingIt = pkgMgr;
                    spdlog::info("Updated package manager configuration: {}",
                                 pkgMgr.name);
                } else {
                    packageManagers_.push_back(pkgMgr);
                    spdlog::info("Added new package manager from config: {}",
                                 pkgMgr.name);
                }
            } else {
                spdlog::warn(
                    "Package manager '{}' is configured but not available",
                    pkgMgr.name);
            }
        }

        spdlog::info("Loaded package manager configuration from: {}",
                     configPath);
    } catch (const std::exception& e) {
        spdlog::error("Error loading package manager config: {}", e.what());
    }
}

auto PackageManagerRegistry::getPackageManager(const std::string& name) const
    -> std::optional<PackageManagerInfo> {
    auto it = std::ranges::find_if(
        packageManagers_,
        [&](const PackageManagerInfo& pm) { return pm.name == name; });
    return (it != packageManagers_.end()) ? std::make_optional(*it)
                                          : std::nullopt;
}

auto PackageManagerRegistry::getPackageManagers() const
    -> std::vector<PackageManagerInfo> {
    return packageManagers_;
}

auto PackageManagerRegistry::searchDependency(const std::string& depName)
    -> std::vector<std::string> {
    std::vector<std::string> results;
    std::unordered_set<std::string> uniqueResults;

    spdlog::info("Searching for dependency: {}", depName);

    for (const auto& pkgMgr : packageManagers_) {
        try {
            std::string searchCmd = pkgMgr.getSearchCommand(depName);
            spdlog::debug("Searching with {}: {}", pkgMgr.name, searchCmd);

            std::string output = executeCommand(searchCmd);
            if (!output.empty()) {
                std::vector<std::string> pkgResults =
                    parseSearchOutput(pkgMgr.name, output, depName);
                for (const auto& result : pkgResults) {
                    if (uniqueResults.insert(result).second) {
                        results.push_back(result);
                        spdlog::debug("Found package: {} (via {})", result,
                                      pkgMgr.name);
                    }
                }
            }
        } catch (const std::exception& e) {
            spdlog::error("Error searching with package manager {}: {}",
                          pkgMgr.name, e.what());
        }
    }

    spdlog::info("Search completed. Found {} unique packages", results.size());
    return results;
}

void PackageManagerRegistry::cancelInstallation(const std::string& depName) {
    spdlog::info("Attempting to cancel installation for: {}", depName);

    bool processFound = false;
    for (const auto& pkgMgr : packageManagers_) {
        auto processIt = PKG_MGR_PROCESS_MAP.find(pkgMgr.name);
        if (processIt == PKG_MGR_PROCESS_MAP.end()) {
            continue;
        }

        for (const auto& processName : processIt->second) {
            std::vector<int> pids = getProcessIdsByName(processName);
            if (!pids.empty()) {
                processFound = true;
                spdlog::info("Found {} running processes for {}", pids.size(),
                             processName);

                for (int pid : pids) {
#if defined(_WIN32)
                    HANDLE hProcess =
                        OpenProcess(PROCESS_TERMINATE, FALSE, pid);
                    if (hProcess != NULL) {
                        if (TerminateProcess(hProcess, 1)) {
                            spdlog::info("Terminated process {} (PID: {})",
                                         processName, pid);
                        } else {
                            spdlog::warn(
                                "Failed to terminate process {} (PID: {})",
                                processName, pid);
                        }
                        CloseHandle(hProcess);
                    }
#else
                    if (kill(pid, SIGTERM) == 0) {
                        spdlog::info("Sent SIGTERM to process {} (PID: {})",
                                     processName, pid);
                        std::this_thread::sleep_for(
                            std::chrono::milliseconds(500));

                        if (kill(pid, 0) == 0) {
                            if (kill(pid, SIGKILL) == 0) {
                                spdlog::info(
                                    "Force killed process {} (PID: {})",
                                    processName, pid);
                            } else {
                                spdlog::warn(
                                    "Failed to kill process {} (PID: {})",
                                    processName, pid);
                            }
                        }
                    } else {
                        spdlog::warn(
                            "Failed to send signal to process {} (PID: {})",
                            processName, pid);
                    }
#endif
                }
                killProcessByName(processName);
            }
        }
    }

    if (!processFound) {
        spdlog::info(
            "No package manager processes found running for dependency: {}",
            depName);
    } else {
        spdlog::info("Installation cancellation attempted for: {}", depName);
    }
}

void PackageManagerRegistry::configurePackageManagers() {
    spdlog::info("Configuring package managers for platform: {} ({})",
                 ATOM_PLATFORM, ATOM_ARCHITECTURE);

    loadSystemPackageManagers();

    const std::vector<std::string> configPaths = {
        "./package_managers.json",
        std::string(std::getenv("HOME") ? std::getenv("HOME") : "") +
            "/.lithium/package_managers.json",
        "/etc/lithium/package_managers.json"
#if defined(_WIN32)
        ,
        std::string(std::getenv("APPDATA") ? std::getenv("APPDATA") : "") +
            "\\lithium\\package_managers.json",
        "C:\\ProgramData\\lithium\\package_managers.json"
#endif
    };

    for (const auto& configPath : configPaths) {
        if (std::filesystem::exists(configPath)) {
            loadPackageManagerConfig(configPath);
            break;
        }
    }

    if (!packageManagers_.empty()) {
        std::string availableManagers;
        for (size_t i = 0; i < packageManagers_.size(); ++i) {
            if (i > 0)
                availableManagers += ", ";
            availableManagers += packageManagers_[i].name;
        }
        spdlog::info("Available package managers: {}", availableManagers);
    } else {
        spdlog::warn("No package managers are available on this system");
    }
}

std::vector<std::string> PackageManagerRegistry::parseSearchOutput(
    const std::string& packageManager, const std::string& output,
    const std::string& searchTerm) const {
    std::vector<std::string> results;
    std::istringstream iss(output);
    std::string line;

    if (packageManager == "apt") {
        while (std::getline(iss, line)) {
            if (line.find("/") != std::string::npos && !line.empty()) {
                size_t slashPos = line.find("/");
                if (slashPos != std::string::npos) {
                    std::string packageName = line.substr(0, slashPos);
                    if (!packageName.empty() &&
                        packageName.find(searchTerm) != std::string::npos) {
                        results.push_back(packageName);
                    }
                }
            }
        }
    } else if (packageManager == "dnf" || packageManager == "yum") {
        while (std::getline(iss, line)) {
            if (line.find(".") != std::string::npos && !line.empty()) {
                size_t dotPos = line.find(".");
                if (dotPos != std::string::npos) {
                    std::string packageName = line.substr(0, dotPos);
                    if (!packageName.empty() &&
                        packageName.find(searchTerm) != std::string::npos) {
                        results.push_back(packageName);
                    }
                }
            }
        }
    } else if (packageManager == "pacman") {
        while (std::getline(iss, line)) {
            if (line.find("/") != std::string::npos && !line.empty()) {
                size_t slashPos = line.find("/");
                size_t spacePos = line.find(" ", slashPos);
                if (slashPos != std::string::npos &&
                    spacePos != std::string::npos) {
                    std::string packageName =
                        line.substr(slashPos + 1, spacePos - slashPos - 1);
                    if (!packageName.empty() &&
                        packageName.find(searchTerm) != std::string::npos) {
                        results.push_back(packageName);
                    }
                }
            }
        }
    } else if (packageManager == "brew") {
        while (std::getline(iss, line)) {
            if (!line.empty() && line.find("==>") == std::string::npos) {
                std::istringstream lineStream(line);
                std::string packageName;
                while (lineStream >> packageName) {
                    if (packageName.find(searchTerm) != std::string::npos) {
                        results.push_back(packageName);
                    }
                }
            }
        }
    } else if (packageManager == "choco") {
        while (std::getline(iss, line)) {
            if (!line.empty() && line.find(" ") != std::string::npos &&
                line.find("Chocolatey") == std::string::npos) {
                size_t spacePos = line.find(" ");
                std::string packageName = line.substr(0, spacePos);
                if (!packageName.empty() &&
                    packageName.find(searchTerm) != std::string::npos) {
                    results.push_back(packageName);
                }
            }
        }
    } else if (packageManager == "scoop") {
        while (std::getline(iss, line)) {
            if (!line.empty() && line.find("'") != std::string::npos) {
                size_t start = line.find("'");
                size_t end = line.find("'", start + 1);
                if (start != std::string::npos && end != std::string::npos) {
                    std::string packageName =
                        line.substr(start + 1, end - start - 1);
                    if (!packageName.empty() &&
                        packageName.find(searchTerm) != std::string::npos) {
                        results.push_back(packageName);
                    }
                }
            }
        }
    } else if (packageManager == "winget") {
        bool inResults = false;
        while (std::getline(iss, line)) {
            if (line.find("Name") != std::string::npos &&
                line.find("Id") != std::string::npos) {
                inResults = true;
                continue;
            }
            if (inResults && !line.empty() &&
                line.find("-") == std::string::npos) {
                std::istringstream lineStream(line);
                std::string packageName;
                if (lineStream >> packageName &&
                    packageName.find(searchTerm) != std::string::npos) {
                    results.push_back(packageName);
                }
            }
        }
    } else {
        while (std::getline(iss, line)) {
            if (!line.empty() && line.find(searchTerm) != std::string::npos) {
                std::istringstream lineStream(line);
                std::string packageName;
                if (lineStream >> packageName) {
                    results.push_back(packageName);
                }
            }
        }
    }

    return results;
}

}  // namespace lithium::system
