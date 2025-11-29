/*
 * package_manager.hpp
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SCRIPT_VENV_PACKAGE_MANAGER_HPP
#define LITHIUM_SCRIPT_VENV_PACKAGE_MANAGER_HPP

#include "types.hpp"
#include <filesystem>
#include <memory>

namespace lithium::venv {

/**
 * @brief Manages Python package installation and listing
 */
class PackageManager {
public:
    PackageManager();
    ~PackageManager();

    PackageManager(const PackageManager&) = delete;
    PackageManager& operator=(const PackageManager&) = delete;
    PackageManager(PackageManager&&) noexcept;
    PackageManager& operator=(PackageManager&&) noexcept;

    // Set the pip executable path
    void setPipExecutable(const std::filesystem::path& pipPath);

    // Package operations
    [[nodiscard]] VenvResult<void> installPackage(
        std::string_view package,
        bool upgrade = false,
        ProgressCallback callback = nullptr);

    [[nodiscard]] VenvResult<void> installPackages(
        const std::vector<std::string>& packages,
        bool upgrade = false,
        ProgressCallback callback = nullptr);

    [[nodiscard]] VenvResult<void> installFromRequirements(
        const std::filesystem::path& requirementsFile,
        bool upgrade = false,
        ProgressCallback callback = nullptr);

    [[nodiscard]] VenvResult<void> uninstallPackage(std::string_view package);

    [[nodiscard]] VenvResult<std::vector<InstalledPackage>> listInstalledPackages() const;

    [[nodiscard]] VenvResult<InstalledPackage> getPackageInfo(std::string_view package) const;

    [[nodiscard]] bool isPackageInstalled(std::string_view package) const;

    [[nodiscard]] VenvResult<void> upgradePip(const std::filesystem::path& pythonPath);

    [[nodiscard]] VenvResult<void> exportRequirements(
        const std::filesystem::path& outputFile,
        bool includeVersions = true) const;

    void setOperationTimeout(std::chrono::seconds timeout);

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::venv

#endif
