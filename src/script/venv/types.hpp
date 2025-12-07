/*
 * types.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

/**
 * @file types.hpp
 * @brief Type definitions for Python Virtual Environment Manager
 * @date 2024
 * @version 1.0.0
 *
 * This module provides type definitions for virtual environment operations,
 * including error codes, configuration structures, and result types.
 */

#ifndef LITHIUM_SCRIPT_VENV_TYPES_HPP
#define LITHIUM_SCRIPT_VENV_TYPES_HPP

#include <chrono>
#include <expected>
#include <filesystem>
#include <functional>
#include <string>
#include <string_view>
#include <vector>

namespace lithium::venv {

/**
 * @brief Error codes for virtual environment operations
 */
enum class VenvError {
    Success = 0,
    PythonNotFound,
    CondaNotFound,
    VenvCreationFailed,
    VenvActivationFailed,
    VenvNotFound,
    PackageInstallFailed,
    PackageUninstallFailed,
    RequirementsNotFound,
    PermissionDenied,
    NetworkError,
    InvalidPath,
    AlreadyExists,
    InvalidEnvironment,
    TimeoutError,
    UnknownError
};

/**
 * @brief Get string representation of VenvError
 */
[[nodiscard]] constexpr std::string_view venvErrorToString(
    VenvError error) noexcept {
    switch (error) {
        case VenvError::Success:
            return "Success";
        case VenvError::PythonNotFound:
            return "Python interpreter not found";
        case VenvError::CondaNotFound:
            return "Conda not found";
        case VenvError::VenvCreationFailed:
            return "Virtual environment creation failed";
        case VenvError::VenvActivationFailed:
            return "Virtual environment activation failed";
        case VenvError::VenvNotFound:
            return "Virtual environment not found";
        case VenvError::PackageInstallFailed:
            return "Package installation failed";
        case VenvError::PackageUninstallFailed:
            return "Package uninstallation failed";
        case VenvError::RequirementsNotFound:
            return "Requirements file not found";
        case VenvError::PermissionDenied:
            return "Permission denied";
        case VenvError::NetworkError:
            return "Network error";
        case VenvError::InvalidPath:
            return "Invalid path";
        case VenvError::AlreadyExists:
            return "Environment already exists";
        case VenvError::InvalidEnvironment:
            return "Invalid environment";
        case VenvError::TimeoutError:
            return "Operation timed out";
        case VenvError::UnknownError:
            return "Unknown error";
    }
    return "Unknown error";
}

/**
 * @brief Information about an installed Python package
 */
struct InstalledPackage {
    std::string name;                       ///< Package name
    std::string version;                    ///< Installed version
    std::string location;                   ///< Installation location
    std::string summary;                    ///< Package summary/description
    std::vector<std::string> dependencies;  ///< Package dependencies
    bool isEditable{false};  ///< Whether installed in editable mode
};

/**
 * @brief Virtual environment information
 */
struct VenvInfo {
    std::filesystem::path path;  ///< Environment path
    std::string pythonVersion;   ///< Python version
    std::string pipVersion;      ///< Pip version
    bool isActive{false};        ///< Whether currently active
    bool isConda{false};         ///< Whether this is a conda environment
    std::string name;            ///< Environment name
    std::chrono::system_clock::time_point createdAt;  ///< Creation time
    size_t packageCount{0};  ///< Number of installed packages
};

/**
 * @brief Configuration for virtual environment creation
 */
struct VenvConfig {
    std::filesystem::path path;  ///< Environment path
    std::string
        pythonVersion;   ///< Desired Python version (empty = system default)
    bool withPip{true};  ///< Include pip
    bool withSetuptools{true};       ///< Include setuptools
    bool systemSitePackages{false};  ///< Access system site packages
    bool symlinks{true};  ///< Use symlinks (Unix) or copies (Windows)
    bool upgrade{false};  ///< Upgrade if exists
    bool clear{false};    ///< Clear existing environment
    std::vector<std::string>
        extraPackages;  ///< Packages to install after creation
};

/**
 * @brief Configuration for conda environment creation
 */
struct CondaEnvConfig {
    std::string name;                   ///< Environment name
    std::string pythonVersion;          ///< Python version (e.g., "3.11")
    std::vector<std::string> channels;  ///< Conda channels
    std::vector<std::string> packages;  ///< Initial packages to install
    bool useMamba{false};               ///< Use mamba instead of conda
};

/**
 * @brief Result type for virtual environment operations
 */
template <typename T>
using VenvResult = std::expected<T, VenvError>;

/**
 * @brief Progress callback for long-running operations
 */
using ProgressCallback =
    std::function<void(float progress, std::string_view message)>;

}  // namespace lithium::venv

#endif  // LITHIUM_SCRIPT_VENV_TYPES_HPP
