/**
 * @file integration_utils.cpp
 * @brief Implementation of integration utilities for task system components
 *
 * @date 2024-12-26
 * @author Max Qian <lightapt.com>
 * @copyright Copyright (C) 2023-2024 Max Qian
 */

#include "integration_utils.hpp"
#include "imagepath.hpp"
#include "task.hpp"

#include <spdlog/spdlog.h>
#include <chrono>
#include <fstream>
#include <regex>
#include <sstream>

#ifdef _WIN32
#include <psapi.h>
#include <windows.h>
#else
#include <sys/statvfs.h>
#include <sys/sysinfo.h>
#endif

namespace lithium::task::integration {

// ============================================================================
// ImagePathHelper Implementation
// ============================================================================

std::string ImagePathHelper::defaultPattern_ =
    "{target}_{filter}_{type}_{exposure}s_{temp}C_{gain}_{datetime}_{seq:04d}";

std::filesystem::path ImagePathHelper::generateOutputPath(
    const std::filesystem::path& basePath, const std::string& taskName,
    const json& params, int sequence) {
    std::string filename;

    try {
        // Extract common parameters
        std::string target = params.value("target_name", taskName);
        std::string filter = params.value("filter", "L");
        std::string type = params.value("type", "light");
        double exposure = params.value("exposure", 0.0);
        double temp = params.value("temperature", -999.0);
        int gain = params.value("gain", 0);

        // Generate timestamp
        auto now = std::chrono::system_clock::now();
        auto time_t = std::chrono::system_clock::to_time_t(now);
        std::tm tm = *std::localtime(&time_t);

        char datetime[32];
        std::strftime(datetime, sizeof(datetime), "%Y%m%d_%H%M%S", &tm);

        // Build filename
        std::ostringstream oss;
        oss << target << "_" << filter << "_" << type << "_"
            << static_cast<int>(exposure) << "s_";

        if (temp > -999.0) {
            oss << static_cast<int>(temp) << "C_";
        }

        oss << gain << "_" << datetime << "_" << std::setfill('0')
            << std::setw(4) << sequence << ".fits";

        filename = oss.str();

        // Sanitize filename (remove invalid characters)
        std::regex invalid_chars(R"([<>:"/\\|?*])");
        filename = std::regex_replace(filename, invalid_chars, "_");

    } catch (const std::exception& e) {
        spdlog::error("Failed to generate output path: {}", e.what());
        filename = taskName + "_" + std::to_string(sequence) + ".fits";
    }

    return basePath / filename;
}

std::optional<ImageInfo> ImagePathHelper::parseImagePath(
    const std::filesystem::path& imagePath) {
    try {
        auto& parser = getParser();
        return parser.parse(imagePath.string());
    } catch (const std::exception& e) {
        spdlog::error("Failed to parse image path '{}': {}", imagePath.string(),
                      e.what());
        return std::nullopt;
    }
}

ImagePatternParser& ImagePathHelper::getParser(const std::string& pattern) {
    static ImagePatternParser parser(pattern.empty() ? defaultPattern_
                                                     : pattern);
    return parser;
}

void ImagePathHelper::setDefaultPattern(const std::string& pattern) {
    defaultPattern_ = pattern;
}

// ============================================================================
// ScriptHelper Implementation
// ============================================================================

json ScriptHelper::executeScript(const std::filesystem::path& scriptPath,
                                 const json& params, int timeout) {
    if (!std::filesystem::exists(scriptPath)) {
        throw std::runtime_error("Script file not found: " +
                                 scriptPath.string());
    }

    json result;
    result["success"] = false;
    result["output"] = "";
    result["error"] = "";

    try {
        // Prepare parameters file
        auto paramsPath =
            std::filesystem::temp_directory_path() /
            ("params_" + std::to_string(std::time(nullptr)) + ".json");

        std::ofstream paramsFile(paramsPath);
        if (!paramsFile) {
            throw std::runtime_error("Failed to create parameters file");
        }
        paramsFile << params.dump(2);
        paramsFile.close();

        // Build command
        std::string command;
        std::string extension = scriptPath.extension().string();

        if (extension == ".py") {
            command = "python \"" + scriptPath.string() + "\" \"" +
                      paramsPath.string() + "\"";
        } else if (extension == ".sh" || extension == ".bash") {
            command = "bash \"" + scriptPath.string() + "\" \"" +
                      paramsPath.string() + "\"";
        } else if (extension == ".ps1") {
            command = "powershell -ExecutionPolicy Bypass -File \"" +
                      scriptPath.string() + "\" -ParamsFile \"" +
                      paramsPath.string() + "\"";
        } else {
            throw std::runtime_error("Unsupported script type: " + extension);
        }

        spdlog::info("Executing script: {}", command);

        // Execute with timeout (simplified implementation)
        FILE* pipe = popen(command.c_str(), "r");
        if (!pipe) {
            throw std::runtime_error("Failed to execute script");
        }

        std::string output;
        char buffer[256];
        while (fgets(buffer, sizeof(buffer), pipe) != nullptr) {
            output += buffer;
        }

        int exitCode = pclose(pipe);

        result["success"] = (exitCode == 0);
        result["output"] = output;
        result["exit_code"] = exitCode;

        // Cleanup
        std::filesystem::remove(paramsPath);

    } catch (const std::exception& e) {
        result["error"] = e.what();
        spdlog::error("Script execution failed: {}", e.what());
    }

    return result;
}

bool ScriptHelper::validateScriptParams(const json& params,
                                        const json& schema) {
    if (schema.empty()) {
        return true;
    }

    // Basic JSON schema validation
    try {
        for (auto& [key, value] : schema.items()) {
            if (value.contains("required") && value["required"].get<bool>()) {
                if (!params.contains(key)) {
                    spdlog::error("Missing required parameter: {}", key);
                    return false;
                }
            }

            if (params.contains(key)) {
                if (value.contains("type")) {
                    std::string expectedType = value["type"];
                    // Type checking logic here
                }
            }
        }
        return true;
    } catch (const std::exception& e) {
        spdlog::error("Parameter validation failed: {}", e.what());
        return false;
    }
}

json ScriptHelper::convertTaskOutputToScriptInput(const json& taskOutput) {
    json scriptInput;

    try {
        // Extract relevant data from task output
        if (taskOutput.contains("result")) {
            scriptInput["data"] = taskOutput["result"];
        }

        if (taskOutput.contains("metadata")) {
            scriptInput["metadata"] = taskOutput["metadata"];
        }

        if (taskOutput.contains("files")) {
            scriptInput["files"] = taskOutput["files"];
        }

        // Add timestamp
        scriptInput["timestamp"] = std::time(nullptr);

    } catch (const std::exception& e) {
        spdlog::error("Failed to convert task output: {}", e.what());
    }

    return scriptInput;
}

// ============================================================================
// DeviceHelper Implementation
// ============================================================================

bool DeviceHelper::waitForDevice(const std::string& deviceName, int timeout) {
    auto startTime = std::chrono::steady_clock::now();
    int elapsed = 0;

    while (elapsed < timeout) {
        if (isDeviceConnected(deviceName)) {
            spdlog::info("Device '{}' is ready", deviceName);
            return true;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(500));

        auto now = std::chrono::steady_clock::now();
        elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                      now - startTime)
                      .count();
    }

    spdlog::warn("Device '{}' timeout after {}ms", deviceName, timeout);
    return false;
}

bool DeviceHelper::isDeviceConnected(const std::string& deviceName) {
    // This is a placeholder - actual implementation would check
    // device registry or hardware manager
    try {
        auto prop = getDeviceProperty(deviceName, "connected");
        return prop.value("connected", false);
    } catch (...) {
        return false;
    }
}

json DeviceHelper::getDeviceProperty(const std::string& deviceName,
                                     const std::string& propertyName) {
    // Placeholder implementation
    // In real implementation, this would query INDI/ASCOM/device manager
    json result;
    result["device"] = deviceName;
    result["property"] = propertyName;
    result["value"] = nullptr;

    spdlog::debug("Getting property '{}' from device '{}'", propertyName,
                  deviceName);

    return result;
}

bool DeviceHelper::setDeviceProperty(const std::string& deviceName,
                                     const std::string& propertyName,
                                     const json& value) {
    // Placeholder implementation
    spdlog::info("Setting property '{}' on device '{}' to: {}", propertyName,
                 deviceName, value.dump());

    return true;
}

// ============================================================================
// ValidationHelper Implementation
// ============================================================================

thread_local std::string ValidationHelper::lastError_ = "";

bool ValidationHelper::validateRange(double value, double min, double max) {
    if (value < min || value > max) {
        lastError_ = "Value " + std::to_string(value) + " is outside range [" +
                     std::to_string(min) + ", " + std::to_string(max) + "]";
        return false;
    }
    return true;
}

bool ValidationHelper::validateRequiredParams(
    const json& params, const std::vector<std::string>& required) {
    for (const auto& param : required) {
        if (!params.contains(param)) {
            lastError_ = "Missing required parameter: " + param;
            spdlog::error(lastError_);
            return false;
        }
    }
    return true;
}

bool ValidationHelper::validateAgainstSchema(const json& param,
                                             const json& schema) {
    try {
        // Basic schema validation
        if (schema.contains("type")) {
            std::string expectedType = schema["type"];
            // Type checking based on schema
        }

        if (schema.contains("minimum") && param.is_number()) {
            double min = schema["minimum"];
            if (param.get<double>() < min) {
                lastError_ = "Value below minimum: " + std::to_string(min);
                return false;
            }
        }

        if (schema.contains("maximum") && param.is_number()) {
            double max = schema["maximum"];
            if (param.get<double>() > max) {
                lastError_ = "Value above maximum: " + std::to_string(max);
                return false;
            }
        }

        return true;
    } catch (const std::exception& e) {
        lastError_ = std::string("Schema validation error: ") + e.what();
        return false;
    }
}

std::string ValidationHelper::getLastError() { return lastError_; }

// ============================================================================
// TaskChainHelper Implementation
// ============================================================================

std::vector<std::string> TaskChainHelper::createDependencyChain(
    const std::vector<json>& tasks) {
    std::vector<std::string> chain;
    std::unordered_map<std::string, json> taskMap;

    // Build task map
    for (const auto& task : tasks) {
        if (task.contains("name")) {
            std::string name = task["name"];
            taskMap[name] = task;
        }
    }

    // Check for circular dependencies
    if (hasCircularDependency(taskMap)) {
        spdlog::error("Circular dependency detected in task chain");
        return chain;
    }

    // Topological sort
    std::unordered_set<std::string> visited;
    std::function<void(const std::string&)> visit;

    visit = [&](const std::string& taskName) {
        if (visited.count(taskName))
            return;
        visited.insert(taskName);

        if (taskMap.count(taskName)) {
            const auto& task = taskMap[taskName];
            if (task.contains("dependencies")) {
                for (const auto& dep : task["dependencies"]) {
                    if (dep.is_string()) {
                        visit(dep.get<std::string>());
                    }
                }
            }
        }

        chain.push_back(taskName);
    };

    for (const auto& [name, _] : taskMap) {
        visit(name);
    }

    return chain;
}

std::vector<std::string> TaskChainHelper::resolveDependencies(
    const std::string& taskName,
    const std::unordered_map<std::string, json>& allTasks) {
    std::vector<std::string> dependencies;
    std::unordered_set<std::string> visited;

    std::function<void(const std::string&)> resolve;
    resolve = [&](const std::string& name) {
        if (visited.count(name))
            return;
        visited.insert(name);

        if (allTasks.count(name)) {
            const auto& task = allTasks.at(name);
            if (task.contains("dependencies")) {
                for (const auto& dep : task["dependencies"]) {
                    if (dep.is_string()) {
                        std::string depName = dep.get<std::string>();
                        resolve(depName);
                        dependencies.push_back(depName);
                    }
                }
            }
        }
    };

    resolve(taskName);
    return dependencies;
}

bool TaskChainHelper::hasCircularDependency(
    const std::unordered_map<std::string, json>& tasks) {
    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> recursionStack;

    std::function<bool(const std::string&)> hasCycle;
    hasCycle = [&](const std::string& taskName) -> bool {
        visited.insert(taskName);
        recursionStack.insert(taskName);

        if (tasks.count(taskName)) {
            const auto& task = tasks.at(taskName);
            if (task.contains("dependencies")) {
                for (const auto& dep : task["dependencies"]) {
                    if (dep.is_string()) {
                        std::string depName = dep.get<std::string>();

                        if (!visited.count(depName)) {
                            if (hasCycle(depName))
                                return true;
                        } else if (recursionStack.count(depName)) {
                            return true;
                        }
                    }
                }
            }
        }

        recursionStack.erase(taskName);
        return false;
    };

    for (const auto& [name, _] : tasks) {
        if (!visited.count(name)) {
            if (hasCycle(name))
                return true;
        }
    }

    return false;
}

// ============================================================================
// ResourceHelper Implementation
// ============================================================================

bool ResourceHelper::checkDiskSpace(const std::filesystem::path& path,
                                    size_t requiredMB) {
    try {
#ifdef _WIN32
        ULARGE_INTEGER freeBytesAvailable;
        if (GetDiskFreeSpaceExA(path.string().c_str(), &freeBytesAvailable,
                                nullptr, nullptr)) {
            size_t availableMB = freeBytesAvailable.QuadPart / (1024 * 1024);
            return availableMB >= requiredMB;
        }
#else
        struct statvfs stat;
        if (statvfs(path.c_str(), &stat) == 0) {
            size_t availableMB =
                (stat.f_bavail * stat.f_frsize) / (1024 * 1024);
            return availableMB >= requiredMB;
        }
#endif
    } catch (const std::exception& e) {
        spdlog::error("Failed to check disk space: {}", e.what());
    }

    return false;
}

bool ResourceHelper::checkMemory(size_t requiredMB) {
    try {
#ifdef _WIN32
        MEMORYSTATUSEX memStatus;
        memStatus.dwLength = sizeof(memStatus);
        if (GlobalMemoryStatusEx(&memStatus)) {
            size_t availableMB = memStatus.ullAvailPhys / (1024 * 1024);
            return availableMB >= requiredMB;
        }
#else
        struct sysinfo info;
        if (sysinfo(&info) == 0) {
            size_t availableMB = info.freeram / (1024 * 1024);
            return availableMB >= requiredMB;
        }
#endif
    } catch (const std::exception& e) {
        spdlog::error("Failed to check memory: {}", e.what());
    }

    return false;
}

size_t ResourceHelper::estimateImageSize(int width, int height, int bitDepth,
                                         bool compression) {
    // Calculate raw size
    size_t bitsPerPixel = bitDepth;
    size_t totalBits = static_cast<size_t>(width) * height * bitsPerPixel;
    size_t totalBytes = (totalBits + 7) / 8;  // Round up to nearest byte

    // Add FITS header overhead (typically ~2880 bytes per HDU)
    size_t headerSize = 2880;

    // Apply compression estimate (if applicable)
    if (compression) {
        // Typical FITS compression ratios: 2-4x for astronomical images
        totalBytes = totalBytes / 3;
    }

    return totalBytes + headerSize;
}

}  // namespace lithium::task::integration
