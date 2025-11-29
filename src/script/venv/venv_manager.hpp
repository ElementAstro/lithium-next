/*
 * venv_manager.hpp
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SCRIPT_VENV_VENV_MANAGER_HPP
#define LITHIUM_SCRIPT_VENV_VENV_MANAGER_HPP

#include "types.hpp"
#include "package_manager.hpp"
#include "conda_adapter.hpp"
#include <filesystem>
#include <memory>
#include <optional>

namespace lithium::venv {

/**
 * @brief Unified Virtual Environment Manager Facade
 *
 * Provides a high-level API for Python venv and conda management.
 * Delegates to specialized components:
 * - PackageManager for package operations
 * - CondaAdapter for conda environment management
 */
class VenvManager {
public:
    VenvManager();
    ~VenvManager();

    VenvManager(const VenvManager&) = delete;
    VenvManager& operator=(const VenvManager&) = delete;
    VenvManager(VenvManager&&) noexcept;
    VenvManager& operator=(VenvManager&&) noexcept;

    // venv Management
    [[nodiscard]] VenvResult<VenvInfo> createVenv(const VenvConfig& config, ProgressCallback callback = nullptr);
    [[nodiscard]] VenvResult<VenvInfo> createVenv(const std::filesystem::path& path, std::string_view pythonVersion = "");
    [[nodiscard]] VenvResult<void> activateVenv(const std::filesystem::path& path);
    [[nodiscard]] VenvResult<void> deactivateVenv();
    [[nodiscard]] VenvResult<void> deleteVenv(const std::filesystem::path& path);
    [[nodiscard]] VenvResult<VenvInfo> getVenvInfo(const std::filesystem::path& path) const;
    [[nodiscard]] bool isValidVenv(const std::filesystem::path& path) const;

    // Conda (delegates to CondaAdapter)
    [[nodiscard]] VenvResult<VenvInfo> createCondaEnv(const CondaEnvConfig& config, ProgressCallback callback = nullptr);
    [[nodiscard]] VenvResult<VenvInfo> createCondaEnv(std::string_view name, std::string_view pythonVersion = "");
    [[nodiscard]] VenvResult<void> activateCondaEnv(std::string_view name);
    [[nodiscard]] VenvResult<void> deactivateCondaEnv();
    [[nodiscard]] VenvResult<void> deleteCondaEnv(std::string_view name);
    [[nodiscard]] VenvResult<std::vector<VenvInfo>> listCondaEnvs() const;
    [[nodiscard]] VenvResult<VenvInfo> getCondaEnvInfo(std::string_view name) const;
    [[nodiscard]] bool isCondaAvailable() const;

    // Package Management (delegates to PackageManager)
    [[nodiscard]] VenvResult<void> installPackage(std::string_view package, bool upgrade = false, ProgressCallback callback = nullptr);
    [[nodiscard]] VenvResult<void> installPackages(const std::vector<std::string>& packages, bool upgrade = false, ProgressCallback callback = nullptr);
    [[nodiscard]] VenvResult<void> installFromRequirements(const std::filesystem::path& requirementsFile, bool upgrade = false, ProgressCallback callback = nullptr);
    [[nodiscard]] VenvResult<void> uninstallPackage(std::string_view package);
    [[nodiscard]] VenvResult<std::vector<InstalledPackage>> listInstalledPackages() const;
    [[nodiscard]] VenvResult<InstalledPackage> getPackageInfo(std::string_view package) const;
    [[nodiscard]] bool isPackageInstalled(std::string_view package) const;
    [[nodiscard]] VenvResult<void> upgradePip();
    [[nodiscard]] VenvResult<void> exportRequirements(const std::filesystem::path& outputFile, bool includeVersions = true) const;

    // State
    [[nodiscard]] bool isVenvActive() const;
    [[nodiscard]] std::optional<std::filesystem::path> getCurrentVenvPath() const;
    [[nodiscard]] std::optional<VenvInfo> getCurrentVenvInfo() const;
    [[nodiscard]] std::filesystem::path getPythonExecutable(const std::optional<std::filesystem::path>& venvPath = std::nullopt) const;
    [[nodiscard]] std::filesystem::path getPipExecutable(const std::optional<std::filesystem::path>& venvPath = std::nullopt) const;

    // Configuration
    void setDefaultPython(const std::filesystem::path& pythonPath);
    void setCondaPath(const std::filesystem::path& condaPath);
    void setOperationTimeout(std::chrono::seconds timeout);
    [[nodiscard]] std::vector<std::filesystem::path> discoverPythonInterpreters() const;

    // Component access
    [[nodiscard]] PackageManager& packages();
    [[nodiscard]] CondaAdapter& conda();

private:
    class Impl;
    std::unique_ptr<Impl> pImpl_;
};

}  // namespace lithium::venv

#endif  // LITHIUM_SCRIPT_VENV_VENV_MANAGER_HPP
