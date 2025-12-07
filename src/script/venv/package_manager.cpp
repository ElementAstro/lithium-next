/*
 * package_manager.cpp
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "package_manager.hpp"

#include "process_utils.hpp"

#include <spdlog/spdlog.h>

#include <algorithm>
#include <fstream>
#include <mutex>
#include <regex>
#include <sstream>

namespace lithium::venv {

namespace {

/**
 * @brief Parse pip list output (JSON format)
 */
std::vector<InstalledPackage> parsePipListJson(const std::string& jsonOutput) {
    std::vector<InstalledPackage> packages;

    // Simple JSON parsing for pip list --format=json output
    // Format: [{"name": "package", "version": "1.0.0"}, ...]
    std::regex packageRegex(
        R"delim(\{\s*"name"\s*:\s*"([^"]+)"\s*,\s*"version"\s*:\s*"([^"]+)"\s*\})delim");

    auto begin = std::sregex_iterator(jsonOutput.begin(), jsonOutput.end(),
                                      packageRegex);
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
class PackageManager::Impl {
public:
    Impl() { spdlog::info("PackageManager initialized"); }

    ~Impl() { spdlog::info("PackageManager destroyed"); }

    void setPipExecutable(const std::filesystem::path& pipPath) {
        std::lock_guard<std::mutex> lock(mutex_);
        pipPath_ = pipPath;
        spdlog::info("Set pip executable to: {}", pipPath_.string());
    }

    VenvResult<void> installPackage(std::string_view package, bool upgrade,
                                    ProgressCallback callback) {
        if (pipPath_.empty()) {
            spdlog::error("Pip executable path not set");
            return std::unexpected(VenvError::InvalidPath);
        }

        std::ostringstream cmd;
        cmd << "\"" << pipPath_.string() << "\" install";
        if (upgrade) {
            cmd << " --upgrade";
        }
        cmd << " " << package;

        if (callback) {
            callback(0.1f, "Installing " + std::string(package) + "...");
        }

        spdlog::info("Installing package: {}", package);

        auto result = executeCommand(cmd.str(), operationTimeout_);
        if (result.exitCode != 0) {
            spdlog::error("Failed to install package {}: {}", package,
                          result.errorOutput);
            return std::unexpected(VenvError::PackageInstallFailed);
        }

        if (callback) {
            callback(1.0f, "Complete");
        }

        spdlog::info("Successfully installed package: {}", package);
        return {};
    }

    VenvResult<void> installPackages(const std::vector<std::string>& packages,
                                     bool upgrade, ProgressCallback callback) {
        if (packages.empty()) {
            return {};
        }

        if (pipPath_.empty()) {
            spdlog::error("Pip executable path not set");
            return std::unexpected(VenvError::InvalidPath);
        }

        std::ostringstream cmd;
        cmd << "\"" << pipPath_.string() << "\" install";
        if (upgrade) {
            cmd << " --upgrade";
        }

        for (const auto& pkg : packages) {
            cmd << " " << pkg;
        }

        if (callback) {
            callback(0.1f, "Installing packages...");
        }

        auto result = executeCommand(cmd.str(), operationTimeout_);
        if (result.exitCode != 0) {
            spdlog::error("Failed to install packages: {}", result.errorOutput);
            return std::unexpected(VenvError::PackageInstallFailed);
        }

        if (callback) {
            callback(1.0f, "Complete");
        }

        return {};
    }

    VenvResult<void> installFromRequirements(
        const std::filesystem::path& requirementsFile, bool upgrade,
        ProgressCallback callback) {
        if (!std::filesystem::exists(requirementsFile)) {
            spdlog::error("Requirements file not found: {}",
                          requirementsFile.string());
            return std::unexpected(VenvError::RequirementsNotFound);
        }

        if (pipPath_.empty()) {
            spdlog::error("Pip executable path not set");
            return std::unexpected(VenvError::InvalidPath);
        }

        std::ostringstream cmd;
        cmd << "\"" << pipPath_.string() << "\" install -r \""
            << requirementsFile.string() << "\"";
        if (upgrade) {
            cmd << " --upgrade";
        }

        if (callback) {
            callback(0.1f, "Installing from requirements...");
        }

        auto result = executeCommand(cmd.str(), operationTimeout_);
        if (result.exitCode != 0) {
            spdlog::error("Failed to install from requirements: {}",
                          result.errorOutput);
            return std::unexpected(VenvError::PackageInstallFailed);
        }

        if (callback) {
            callback(1.0f, "Complete");
        }

        return {};
    }

    VenvResult<void> uninstallPackage(std::string_view package) {
        if (pipPath_.empty()) {
            spdlog::error("Pip executable path not set");
            return std::unexpected(VenvError::InvalidPath);
        }

        std::ostringstream cmd;
        cmd << "\"" << pipPath_.string() << "\" uninstall " << package << " -y";

        auto result = executeCommand(cmd.str(), operationTimeout_);
        if (result.exitCode != 0) {
            spdlog::error("Failed to uninstall package {}: {}", package,
                          result.errorOutput);
            return std::unexpected(VenvError::PackageUninstallFailed);
        }

        spdlog::info("Successfully uninstalled package: {}", package);
        return {};
    }

    VenvResult<std::vector<InstalledPackage>> listInstalledPackages() const {
        if (pipPath_.empty()) {
            spdlog::error("Pip executable path not set");
            return std::unexpected(VenvError::InvalidPath);
        }

        auto result =
            executeCommand("\"" + pipPath_.string() + "\" list --format=json",
                           std::chrono::seconds{60});
        if (result.exitCode != 0) {
            spdlog::error("Failed to list installed packages: {}",
                          result.errorOutput);
            return std::unexpected(VenvError::UnknownError);
        }

        return parsePipListJson(result.output);
    }

    VenvResult<InstalledPackage> getPackageInfo(
        std::string_view package) const {
        if (pipPath_.empty()) {
            spdlog::error("Pip executable path not set");
            return std::unexpected(VenvError::InvalidPath);
        }

        std::ostringstream cmd;
        cmd << "\"" << pipPath_.string() << "\" show " << package;

        auto result = executeCommand(cmd.str(), std::chrono::seconds{30});
        if (result.exitCode != 0) {
            spdlog::warn("Failed to get package info for {}: {}", package,
                         result.errorOutput);
            return std::unexpected(VenvError::UnknownError);
        }

        InstalledPackage pkg;
        std::istringstream iss(result.output);
        std::string line;

        while (std::getline(iss, line)) {
            if (line.starts_with("Name: ")) {
                pkg.name = line.substr(6);
            } else if (line.starts_with("Version: ")) {
                pkg.version = line.substr(9);
            } else if (line.starts_with("Summary: ")) {
                pkg.summary = line.substr(9);
            } else if (line.starts_with("Location: ")) {
                pkg.location = line.substr(10);
            } else if (line.starts_with("Requires: ")) {
                std::string deps = line.substr(10);
                std::stringstream ss(deps);
                std::string dep;
                while (std::getline(ss, dep, ',')) {
                    dep.erase(0, dep.find_first_not_of(" "));
                    dep.erase(dep.find_last_not_of(" ") + 1);
                    if (!dep.empty()) {
                        pkg.dependencies.push_back(dep);
                    }
                }
            }
        }

        return pkg;
    }

    bool isPackageInstalled(std::string_view package) const {
        auto result = getPackageInfo(package);
        return result.has_value();
    }

    VenvResult<void> upgradePip(const std::filesystem::path& pythonPath) {
        if (!std::filesystem::exists(pythonPath)) {
            spdlog::error("Python executable not found: {}",
                          pythonPath.string());
            return std::unexpected(VenvError::InvalidPath);
        }

        std::ostringstream cmd;
        cmd << "\"" << pythonPath.string() << "\" -m pip install --upgrade pip";

        auto result = executeCommand(cmd.str(), operationTimeout_);
        if (result.exitCode != 0) {
            spdlog::error("Failed to upgrade pip: {}", result.errorOutput);
            return std::unexpected(VenvError::PackageInstallFailed);
        }

        spdlog::info("Successfully upgraded pip");
        return {};
    }

    VenvResult<void> exportRequirements(const std::filesystem::path& outputFile,
                                        bool includeVersions) const {
        if (pipPath_.empty()) {
            spdlog::error("Pip executable path not set");
            return std::unexpected(VenvError::InvalidPath);
        }

        std::ostringstream cmd;
        cmd << "\"" << pipPath_.string() << "\" freeze";

        auto result = executeCommand(cmd.str(), std::chrono::seconds{60});
        if (result.exitCode != 0) {
            spdlog::error("Failed to export requirements: {}",
                          result.errorOutput);
            return std::unexpected(VenvError::UnknownError);
        }

        std::ofstream file(outputFile);
        if (!file) {
            spdlog::error("Failed to open output file: {}",
                          outputFile.string());
            return std::unexpected(VenvError::PermissionDenied);
        }

        if (includeVersions) {
            file << result.output;
        } else {
            std::istringstream iss(result.output);
            std::string line;
            while (std::getline(iss, line)) {
                auto pos = line.find("==");
                if (pos != std::string::npos) {
                    file << line.substr(0, pos) << "\n";
                } else {
                    file << line << "\n";
                }
            }
        }

        spdlog::info("Successfully exported requirements to: {}",
                     outputFile.string());
        return {};
    }

    void setOperationTimeout(std::chrono::seconds timeout) {
        operationTimeout_ = timeout;
    }

private:
    mutable std::mutex mutex_;
    std::filesystem::path pipPath_;
    std::chrono::seconds operationTimeout_{300};
};

// PackageManager implementation forwarding to Impl

PackageManager::PackageManager() : pImpl_(std::make_unique<Impl>()) {}
PackageManager::~PackageManager() = default;
PackageManager::PackageManager(PackageManager&&) noexcept = default;
PackageManager& PackageManager::operator=(PackageManager&&) noexcept = default;

void PackageManager::setPipExecutable(const std::filesystem::path& pipPath) {
    pImpl_->setPipExecutable(pipPath);
}

VenvResult<void> PackageManager::installPackage(std::string_view package,
                                                bool upgrade,
                                                ProgressCallback callback) {
    return pImpl_->installPackage(package, upgrade, std::move(callback));
}

VenvResult<void> PackageManager::installPackages(
    const std::vector<std::string>& packages, bool upgrade,
    ProgressCallback callback) {
    return pImpl_->installPackages(packages, upgrade, std::move(callback));
}

VenvResult<void> PackageManager::installFromRequirements(
    const std::filesystem::path& requirementsFile, bool upgrade,
    ProgressCallback callback) {
    return pImpl_->installFromRequirements(requirementsFile, upgrade,
                                           std::move(callback));
}

VenvResult<void> PackageManager::uninstallPackage(std::string_view package) {
    return pImpl_->uninstallPackage(package);
}

VenvResult<std::vector<InstalledPackage>>
PackageManager::listInstalledPackages() const {
    return pImpl_->listInstalledPackages();
}

VenvResult<InstalledPackage> PackageManager::getPackageInfo(
    std::string_view package) const {
    return pImpl_->getPackageInfo(package);
}

bool PackageManager::isPackageInstalled(std::string_view package) const {
    return pImpl_->isPackageInstalled(package);
}

VenvResult<void> PackageManager::upgradePip(
    const std::filesystem::path& pythonPath) {
    return pImpl_->upgradePip(pythonPath);
}

VenvResult<void> PackageManager::exportRequirements(
    const std::filesystem::path& outputFile, bool includeVersions) const {
    return pImpl_->exportRequirements(outputFile, includeVersions);
}

void PackageManager::setOperationTimeout(std::chrono::seconds timeout) {
    pImpl_->setOperationTimeout(timeout);
}

}  // namespace lithium::venv
