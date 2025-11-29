/*
 * config_discovery.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "config_discovery.hpp"

#include <spdlog/spdlog.h>

#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace lithium::isolated {

std::optional<std::filesystem::path> ConfigDiscovery::findPythonExecutable() {
#ifdef _WIN32
    // Try common Windows locations
    std::vector<std::filesystem::path> searchPaths = {
        "python.exe",
        "python3.exe",
        "C:\\Python312\\python.exe",
        "C:\\Python311\\python.exe",
        "C:\\Python310\\python.exe"
    };

    for (const auto& path : searchPaths) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    // Try to find via PATH
    char buffer[MAX_PATH];
    if (SearchPathA(nullptr, "python.exe", nullptr, MAX_PATH, buffer, nullptr)) {
        return std::filesystem::path(buffer);
    }
#else
    std::vector<std::filesystem::path> searchPaths = {
        "/usr/bin/python3",
        "/usr/local/bin/python3",
        "/usr/bin/python"
    };

    for (const auto& path : searchPaths) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }
#endif
    return std::nullopt;
}

std::optional<std::filesystem::path> ConfigDiscovery::findExecutorScript() {
    std::vector<std::filesystem::path> searchPaths = {
        std::filesystem::current_path() / "python" / "executor" / "isolated_executor.py",
        std::filesystem::current_path() / "scripts" / "isolated_executor.py",
#ifdef _WIN32
        "C:\\Program Files\\Lithium\\python\\isolated_executor.py"
#else
        "/usr/share/lithium/python/isolated_executor.py",
        "/usr/local/share/lithium/python/isolated_executor.py"
#endif
    };

    for (const auto& path : searchPaths) {
        if (std::filesystem::exists(path)) {
            return path;
        }
    }

    return std::nullopt;
}

Result<void> ConfigDiscovery::validateConfig(const IsolationConfig& config) {
    if (config.level == IsolationLevel::None) {
        return {};  // No validation needed for embedded
    }

    // Check Python executable
    auto pythonPath = config.pythonExecutable.empty() ?
        findPythonExecutable() : std::optional{config.pythonExecutable};

    if (!pythonPath || !std::filesystem::exists(*pythonPath)) {
        return std::unexpected(RunnerError::PythonNotFound);
    }

    // Check executor script
    auto executorPath = config.executorScript.empty() ?
        findExecutorScript() : std::optional{config.executorScript};

    if (!executorPath || !std::filesystem::exists(*executorPath)) {
        return std::unexpected(RunnerError::ExecutorNotFound);
    }

    return {};
}

std::optional<std::string> ConfigDiscovery::getPythonVersion(
    const std::filesystem::path& pythonPath) {

    if (!std::filesystem::exists(pythonPath)) {
        return std::nullopt;
    }

#ifdef _WIN32
    std::string cmd = "\"" + pythonPath.string() + "\" --version";
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    std::string cmd = pythonPath.string() + " --version";
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) return std::nullopt;

    char buffer[128];
    std::string result;
    while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        result += buffer;
    }

#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif

    // Parse "Python X.Y.Z"
    if (result.find("Python") != std::string::npos) {
        return result.substr(7);  // Skip "Python "
    }

    return std::nullopt;
}

bool ConfigDiscovery::isPathAllowed(
    const std::filesystem::path& path,
    const std::vector<std::filesystem::path>& allowedPaths) {

    if (allowedPaths.empty()) {
        return true;  // No restrictions
    }

    auto absPath = std::filesystem::absolute(path);
    for (const auto& allowed : allowedPaths) {
        auto absAllowed = std::filesystem::absolute(allowed);
        // Check if path starts with allowed path
        auto [iter1, iter2] = std::mismatch(
            absAllowed.begin(), absAllowed.end(),
            absPath.begin(), absPath.end());
        if (iter1 == absAllowed.end()) {
            return true;
        }
    }
    return false;
}

}  // namespace lithium::isolated
