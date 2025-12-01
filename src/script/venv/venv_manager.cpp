/*
 * venv_manager.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "venv_manager.hpp"

#include "process_utils.hpp"
#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <regex>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace lithium::venv {

namespace {

/**
 * @brief Parse pip list output (JSON format)
 */
std::vector<InstalledPackage> parsePipListJson(const std::string& jsonOutput) {
    std::vector<InstalledPackage> packages;

    // Simple JSON parsing for pip list --format=json output
    // Format: [{"name": "package", "version": "1.0.0"}, ...]
    std::regex packageRegex(R"delim(\{\s*"name"\s*:\s*"([^"]+)"\s*,\s*"version"\s*:\s*"([^"]+)"\s*\})delim");

    auto begin = std::sregex_iterator(jsonOutput.begin(), jsonOutput.end(), packageRegex);
    auto end = std::sregex_iterator();

    for (auto it = begin; it != end; ++it) {
        InstalledPackage pkg;
        pkg.name = (*it)[1].str();
        pkg.version = (*it)[2].str();
        packages.push_back(std::move(pkg));
    }

    return packages;
}

}  // anonymous namespace

// Implementation class
class VenvManager::Impl {
public:
    Impl() : packageManager_(), condaAdapter_() {
        spdlog::info("VenvManager initialized");
        detectSystemPython();
        condaAdapter_.detectConda();
    }

    ~Impl() {
        spdlog::info("VenvManager destroyed");
    }

    void detectSystemPython() {
#ifdef _WIN32
        auto result = executeCommand("where python", std::chrono::seconds{10});
        if (result.exitCode == 0 && !result.output.empty()) {
            std::istringstream iss(result.output);
            std::string line;
            if (std::getline(iss, line)) {
                // Remove trailing whitespace
                line.erase(line.find_last_not_of(" \r\n") + 1);
                defaultPython_ = std::filesystem::path(line);
                spdlog::info("Detected system Python: {}", defaultPython_.string());
                packageManager_.setPipExecutable(defaultPython_.parent_path() / "pip.exe");
            }
        }
#else
        auto result = executeCommand("which python3 || which python", std::chrono::seconds{10});
        if (result.exitCode == 0 && !result.output.empty()) {
            std::string path = result.output;
            path.erase(path.find_last_not_of(" \r\n") + 1);
            defaultPython_ = std::filesystem::path(path);
            spdlog::info("Detected system Python: {}", defaultPython_.string());
            packageManager_.setPipExecutable(defaultPython_.parent_path() / "pip");
        }
#endif
    }

    // venv Management
    VenvResult<VenvInfo> createVenv(const VenvConfig& config, ProgressCallback callback) {
        if (defaultPython_.empty()) {
            spdlog::error("Python interpreter not found");
            return std::unexpected(VenvError::PythonNotFound);
        }

        if (std::filesystem::exists(config.path) && !config.clear && !config.upgrade) {
            spdlog::warn("Virtual environment already exists at {}", config.path.string());
            return std::unexpected(VenvError::AlreadyExists);
        }

        if (callback) {
            callback(0.1f, "Creating virtual environment...");
        }

        // Build venv command
        std::ostringstream cmd;
        cmd << "\"" << defaultPython_.string() << "\" -m venv";

        if (config.systemSitePackages) {
            cmd << " --system-site-packages";
        }
        if (config.clear) {
            cmd << " --clear";
        }
        if (config.upgrade) {
            cmd << " --upgrade";
        }
        if (!config.withPip) {
            cmd << " --without-pip";
        }

        cmd << " \"" << config.path.string() << "\"";

        spdlog::info("Creating venv with command: {}", cmd.str());

        auto result = executeCommand(cmd.str(), operationTimeout_);
        if (result.exitCode != 0) {
            spdlog::error("Failed to create venv: {}", result.errorOutput);
            return std::unexpected(VenvError::VenvCreationFailed);
        }

        if (callback) {
            callback(0.5f, "Virtual environment created");
        }

        // Install extra packages if specified
        if (!config.extraPackages.empty() && config.withPip) {
            if (callback) {
                callback(0.6f, "Installing extra packages...");
            }

            auto pipPath = getPipExecutable(config.path);
            std::ostringstream installCmd;
            installCmd << "\"" << pipPath.string() << "\" install";
            for (const auto& pkg : config.extraPackages) {
                installCmd << " " << pkg;
            }

            auto installResult = executeCommand(installCmd.str(), operationTimeout_);
            if (installResult.exitCode != 0) {
                spdlog::warn("Failed to install some extra packages: {}", installResult.errorOutput);
            }
        }

        if (callback) {
            callback(1.0f, "Complete");
        }

        // Return info about the created environment
        return getVenvInfo(config.path);
    }

    VenvResult<void> activateVenv(const std::filesystem::path& path) {
        if (!isValidVenv(path)) {
            spdlog::error("Invalid virtual environment: {}", path.string());
            return std::unexpected(VenvError::VenvNotFound);
        }

        std::lock_guard<std::mutex> lock(mutex_);

        // Store the current venv path
        activeVenvPath_ = path;
        isVenvActive_ = true;

        // Modify environment variables
        auto scriptsPath = getScriptsPath(path);

        // Set PATH to include venv's bin/Scripts directory
#ifdef _WIN32
        std::string pathEnv = scriptsPath.string() + ";" + (std::getenv("PATH") ? std::getenv("PATH") : "");
        _putenv_s("PATH", pathEnv.c_str());
        _putenv_s("VIRTUAL_ENV", path.string().c_str());
#else
        std::string pathEnv = scriptsPath.string() + ":" + (std::getenv("PATH") ? std::getenv("PATH") : "");
        setenv("PATH", pathEnv.c_str(), 1);
        setenv("VIRTUAL_ENV", path.string().c_str(), 1);
#endif

        // Update pip executable path for PackageManager
        auto pipPath = getPipExecutable(path);
        packageManager_.setPipExecutable(pipPath);

        spdlog::info("Activated virtual environment: {}", path.string());
        return {};
    }

    VenvResult<void> deactivateVenv() {
        std::lock_guard<std::mutex> lock(mutex_);

        if (!isVenvActive_) {
            return {};  // Nothing to deactivate
        }

        // Clear VIRTUAL_ENV
#ifdef _WIN32
        _putenv_s("VIRTUAL_ENV", "");
#else
        unsetenv("VIRTUAL_ENV");
#endif

        activeVenvPath_.reset();
        isVenvActive_ = false;

        // Reset pip executable to default
        if (!defaultPython_.empty()) {
#ifdef _WIN32
            packageManager_.setPipExecutable(defaultPython_.parent_path() / "pip.exe");
#else
            packageManager_.setPipExecutable(defaultPython_.parent_path() / "pip");
#endif
        }

        spdlog::info("Deactivated virtual environment");
        return {};
    }

    VenvResult<void> deleteVenv(const std::filesystem::path& path) {
        if (!std::filesystem::exists(path)) {
            return std::unexpected(VenvError::VenvNotFound);
        }

        std::error_code ec;
        std::filesystem::remove_all(path, ec);

        if (ec) {
            spdlog::error("Failed to delete venv: {}", ec.message());
            return std::unexpected(VenvError::PermissionDenied);
        }

        spdlog::info("Deleted virtual environment: {}", path.string());
        return {};
    }

    VenvResult<VenvInfo> getVenvInfo(const std::filesystem::path& path) const {
        if (!isValidVenv(path)) {
            return std::unexpected(VenvError::VenvNotFound);
        }

        VenvInfo info;
        info.path = path;
        info.name = path.filename().string();
        info.isActive = (activeVenvPath_.has_value() && *activeVenvPath_ == path);
        info.isConda = false;

        // Get Python version
        auto pythonPath = getPythonExecutable(path);
        auto result = executeCommand("\"" + pythonPath.string() + "\" --version",
                                     std::chrono::seconds{10});
        if (result.exitCode == 0) {
            std::regex versionRegex(R"(Python\s+(\d+\.\d+\.\d+))");
            std::smatch match;
            if (std::regex_search(result.output, match, versionRegex)) {
                info.pythonVersion = match[1].str();
            }
        }

        // Get pip version
        auto pipPath = getPipExecutable(path);
        result = executeCommand("\"" + pipPath.string() + "\" --version",
                                std::chrono::seconds{10});
        if (result.exitCode == 0) {
            std::regex versionRegex(R"(pip\s+(\d+\.\d+(?:\.\d+)?))");
            std::smatch match;
            if (std::regex_search(result.output, match, versionRegex)) {
                info.pipVersion = match[1].str();
            }
        }

        // Count packages
        result = executeCommand("\"" + pipPath.string() + "\" list --format=json",
                                std::chrono::seconds{30});
        if (result.exitCode == 0) {
            auto packages = parsePipListJson(result.output);
            info.packageCount = packages.size();
        }

        return info;
    }

    bool isValidVenv(const std::filesystem::path& path) const {
        if (!std::filesystem::exists(path)) {
            return false;
        }

#ifdef _WIN32
        auto pythonExe = path / "Scripts" / "python.exe";
#else
        auto pythonExe = path / "bin" / "python";
#endif

        return std::filesystem::exists(pythonExe);
    }

    // Conda Management (delegates to CondaAdapter)
    VenvResult<VenvInfo> createCondaEnv(const CondaEnvConfig& config, ProgressCallback callback) {
        return condaAdapter_.createCondaEnv(config, callback);
    }

    VenvResult<VenvInfo> createCondaEnv(std::string_view name, std::string_view pythonVersion) {
        return condaAdapter_.createCondaEnv(name, pythonVersion);
    }

    VenvResult<void> activateCondaEnv(std::string_view name) {
        auto result = condaAdapter_.activateCondaEnv(name);
        if (result) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto envInfoResult = condaAdapter_.getCondaEnvInfo(name);
            if (envInfoResult) {
                activeVenvPath_ = envInfoResult->path;
                isVenvActive_ = true;
                activeCondaEnv_ = std::string(name);

                // Update pip executable for PackageManager
                auto pipPath = getPipExecutable(envInfoResult->path);
                packageManager_.setPipExecutable(pipPath);
            }
        }
        return result;
    }

    VenvResult<void> deactivateCondaEnv() {
        auto result = condaAdapter_.deactivateCondaEnv();
        if (result) {
            std::lock_guard<std::mutex> lock(mutex_);
            activeCondaEnv_.reset();
            activeVenvPath_.reset();
            isVenvActive_ = false;

            // Reset pip executable to default
            if (!defaultPython_.empty()) {
#ifdef _WIN32
                packageManager_.setPipExecutable(defaultPython_.parent_path() / "pip.exe");
#else
                packageManager_.setPipExecutable(defaultPython_.parent_path() / "pip");
#endif
            }
        }
        return result;
    }

    VenvResult<void> deleteCondaEnv(std::string_view name) {
        return condaAdapter_.deleteCondaEnv(name);
    }

    VenvResult<std::vector<VenvInfo>> listCondaEnvs() const {
        return condaAdapter_.listCondaEnvs();
    }

    VenvResult<VenvInfo> getCondaEnvInfo(std::string_view name) const {
        return condaAdapter_.getCondaEnvInfo(name);
    }

    bool isCondaAvailable() const {
        return condaAdapter_.isCondaAvailable();
    }

    // Package Management (delegates to PackageManager)
    VenvResult<void> installPackage(std::string_view package, bool upgrade, ProgressCallback callback) {
        return packageManager_.installPackage(package, upgrade, callback);
    }

    VenvResult<void> installPackages(const std::vector<std::string>& packages, bool upgrade, ProgressCallback callback) {
        return packageManager_.installPackages(packages, upgrade, callback);
    }

    VenvResult<void> installFromRequirements(const std::filesystem::path& requirementsFile, bool upgrade, ProgressCallback callback) {
        return packageManager_.installFromRequirements(requirementsFile, upgrade, callback);
    }

    VenvResult<void> uninstallPackage(std::string_view package) {
        return packageManager_.uninstallPackage(package);
    }

    VenvResult<std::vector<InstalledPackage>> listInstalledPackages() const {
        return packageManager_.listInstalledPackages();
    }

    VenvResult<InstalledPackage> getPackageInfo(std::string_view package) const {
        return packageManager_.getPackageInfo(package);
    }

    bool isPackageInstalled(std::string_view package) const {
        return packageManager_.isPackageInstalled(package);
    }

    VenvResult<void> upgradePip() {
        auto pythonPath = getPythonExecutable(activeVenvPath_);
        return packageManager_.upgradePip(pythonPath);
    }

    VenvResult<void> exportRequirements(const std::filesystem::path& outputFile, bool includeVersions) const {
        return packageManager_.exportRequirements(outputFile, includeVersions);
    }

    // State
    bool isVenvActive() const {
        return isVenvActive_;
    }

    std::optional<std::filesystem::path> getCurrentVenvPath() const {
        return activeVenvPath_;
    }

    std::optional<VenvInfo> getCurrentVenvInfo() const {
        if (!activeVenvPath_) {
            return std::nullopt;
        }
        auto result = getVenvInfo(*activeVenvPath_);
        if (result) {
            return *result;
        }
        return std::nullopt;
    }

    std::filesystem::path getPythonExecutable(const std::optional<std::filesystem::path>& venvPath = std::nullopt) const {
        std::filesystem::path basePath;

        if (venvPath) {
            basePath = *venvPath;
        } else if (activeVenvPath_) {
            basePath = *activeVenvPath_;
        } else {
            return defaultPython_;
        }

#ifdef _WIN32
        return basePath / "Scripts" / "python.exe";
#else
        return basePath / "bin" / "python";
#endif
    }

    std::filesystem::path getPipExecutable(const std::optional<std::filesystem::path>& venvPath = std::nullopt) const {
        std::filesystem::path basePath;

        if (venvPath) {
            basePath = *venvPath;
        } else if (activeVenvPath_) {
            basePath = *activeVenvPath_;
        } else {
            // Use system pip
#ifdef _WIN32
            return defaultPython_.parent_path() / "pip.exe";
#else
            return defaultPython_.parent_path() / "pip";
#endif
        }

#ifdef _WIN32
        return basePath / "Scripts" / "pip.exe";
#else
        return basePath / "bin" / "pip";
#endif
    }

    std::filesystem::path getScriptsPath(const std::filesystem::path& venvPath) const {
#ifdef _WIN32
        return venvPath / "Scripts";
#else
        return venvPath / "bin";
#endif
    }

    // Configuration
    void setDefaultPython(const std::filesystem::path& pythonPath) {
        defaultPython_ = pythonPath;
        packageManager_.setPipExecutable(pythonPath.parent_path() / "pip");
    }

    void setCondaPath(const std::filesystem::path& condaPath) {
        condaAdapter_.setCondaPath(condaPath);
    }

    void setOperationTimeout(std::chrono::seconds timeout) {
        operationTimeout_ = timeout;
        packageManager_.setOperationTimeout(timeout);
        condaAdapter_.setOperationTimeout(timeout);
    }

    std::vector<std::filesystem::path> discoverPythonInterpreters() const {
        std::vector<std::filesystem::path> interpreters;

#ifdef _WIN32
        auto result = executeCommand("where python", std::chrono::seconds{30});
#else
        auto result = executeCommand("which -a python3 python 2>/dev/null", std::chrono::seconds{30});
#endif

        if (result.exitCode == 0) {
            std::istringstream iss(result.output);
            std::string line;
            while (std::getline(iss, line)) {
                line.erase(line.find_last_not_of(" \r\n") + 1);
                if (!line.empty() && std::filesystem::exists(line)) {
                    interpreters.emplace_back(line);
                }
            }
        }

        return interpreters;
    }

    // Component access
    PackageManager& packages() {
        return packageManager_;
    }

    CondaAdapter& conda() {
        return condaAdapter_;
    }

private:
    mutable std::mutex mutex_;
    std::filesystem::path defaultPython_;
    bool isVenvActive_{false};
    std::optional<std::filesystem::path> activeVenvPath_;
    std::optional<std::string> activeCondaEnv_;
    std::chrono::seconds operationTimeout_{300};

    // Sub-components
    PackageManager packageManager_;
    CondaAdapter condaAdapter_;
};

// VenvManager implementation forwarding to Impl

VenvManager::VenvManager() : pImpl_(std::make_unique<Impl>()) {}
VenvManager::~VenvManager() = default;
VenvManager::VenvManager(VenvManager&&) noexcept = default;
VenvManager& VenvManager::operator=(VenvManager&&) noexcept = default;

// venv Management
VenvResult<VenvInfo> VenvManager::createVenv(const VenvConfig& config, ProgressCallback callback) {
    return pImpl_->createVenv(config, std::move(callback));
}

VenvResult<VenvInfo> VenvManager::createVenv(const std::filesystem::path& path, std::string_view pythonVersion) {
    VenvConfig config;
    config.path = path;
    if (!pythonVersion.empty()) {
        config.pythonVersion = std::string(pythonVersion);
    }
    return pImpl_->createVenv(config, nullptr);
}

VenvResult<void> VenvManager::activateVenv(const std::filesystem::path& path) {
    return pImpl_->activateVenv(path);
}

VenvResult<void> VenvManager::deactivateVenv() {
    return pImpl_->deactivateVenv();
}

VenvResult<void> VenvManager::deleteVenv(const std::filesystem::path& path) {
    return pImpl_->deleteVenv(path);
}

VenvResult<VenvInfo> VenvManager::getVenvInfo(const std::filesystem::path& path) const {
    return pImpl_->getVenvInfo(path);
}

bool VenvManager::isValidVenv(const std::filesystem::path& path) const {
    return pImpl_->isValidVenv(path);
}

// Conda Management
VenvResult<VenvInfo> VenvManager::createCondaEnv(const CondaEnvConfig& config, ProgressCallback callback) {
    return pImpl_->createCondaEnv(config, std::move(callback));
}

VenvResult<VenvInfo> VenvManager::createCondaEnv(std::string_view name, std::string_view pythonVersion) {
    return pImpl_->createCondaEnv(name, pythonVersion);
}

VenvResult<void> VenvManager::activateCondaEnv(std::string_view name) {
    return pImpl_->activateCondaEnv(name);
}

VenvResult<void> VenvManager::deactivateCondaEnv() {
    return pImpl_->deactivateCondaEnv();
}

VenvResult<void> VenvManager::deleteCondaEnv(std::string_view name) {
    return pImpl_->deleteCondaEnv(name);
}

VenvResult<std::vector<VenvInfo>> VenvManager::listCondaEnvs() const {
    return pImpl_->listCondaEnvs();
}

VenvResult<VenvInfo> VenvManager::getCondaEnvInfo(std::string_view name) const {
    return pImpl_->getCondaEnvInfo(name);
}

bool VenvManager::isCondaAvailable() const {
    return pImpl_->isCondaAvailable();
}

// Package Management
VenvResult<void> VenvManager::installPackage(std::string_view package, bool upgrade, ProgressCallback callback) {
    return pImpl_->installPackage(package, upgrade, std::move(callback));
}

VenvResult<void> VenvManager::installPackages(const std::vector<std::string>& packages, bool upgrade, ProgressCallback callback) {
    return pImpl_->installPackages(packages, upgrade, std::move(callback));
}

VenvResult<void> VenvManager::installFromRequirements(const std::filesystem::path& requirementsFile, bool upgrade, ProgressCallback callback) {
    return pImpl_->installFromRequirements(requirementsFile, upgrade, std::move(callback));
}

VenvResult<void> VenvManager::uninstallPackage(std::string_view package) {
    return pImpl_->uninstallPackage(package);
}

VenvResult<std::vector<InstalledPackage>> VenvManager::listInstalledPackages() const {
    return pImpl_->listInstalledPackages();
}

VenvResult<InstalledPackage> VenvManager::getPackageInfo(std::string_view package) const {
    return pImpl_->getPackageInfo(package);
}

bool VenvManager::isPackageInstalled(std::string_view package) const {
    return pImpl_->isPackageInstalled(package);
}

VenvResult<void> VenvManager::upgradePip() {
    return pImpl_->upgradePip();
}

VenvResult<void> VenvManager::exportRequirements(const std::filesystem::path& outputFile, bool includeVersions) const {
    return pImpl_->exportRequirements(outputFile, includeVersions);
}

// State
bool VenvManager::isVenvActive() const {
    return pImpl_->isVenvActive();
}

std::optional<std::filesystem::path> VenvManager::getCurrentVenvPath() const {
    return pImpl_->getCurrentVenvPath();
}

std::optional<VenvInfo> VenvManager::getCurrentVenvInfo() const {
    return pImpl_->getCurrentVenvInfo();
}

std::filesystem::path VenvManager::getPythonExecutable(const std::optional<std::filesystem::path>& venvPath) const {
    return pImpl_->getPythonExecutable(venvPath);
}

std::filesystem::path VenvManager::getPipExecutable(const std::optional<std::filesystem::path>& venvPath) const {
    return pImpl_->getPipExecutable(venvPath);
}

// Configuration
void VenvManager::setDefaultPython(const std::filesystem::path& pythonPath) {
    pImpl_->setDefaultPython(pythonPath);
}

void VenvManager::setCondaPath(const std::filesystem::path& condaPath) {
    pImpl_->setCondaPath(condaPath);
}

void VenvManager::setOperationTimeout(std::chrono::seconds timeout) {
    pImpl_->setOperationTimeout(timeout);
}

std::vector<std::filesystem::path> VenvManager::discoverPythonInterpreters() const {
    return pImpl_->discoverPythonInterpreters();
}

// Component access
PackageManager& VenvManager::packages() {
    return pImpl_->packages();
}

CondaAdapter& VenvManager::conda() {
    return pImpl_->conda();
}

}  // namespace lithium::venv
