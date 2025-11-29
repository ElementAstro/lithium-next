/*
 * tool_registry.cpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "tool_registry.hpp"

#include <pybind11/embed.h>
#include <pybind11/stl.h>

#include <atomic>
#include <future>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_set>

#include "atom/log/loguru.hpp"

namespace py = pybind11;

namespace lithium::tools {

// =============================================================================
// Implementation
// =============================================================================

class PythonToolRegistry::Impl {
public:
    Impl(const ToolRegistryConfig& config)
        : config_(config),
          initialized_(false) {}

    ~Impl() {
        shutdown();
    }

    ToolResult<void> initialize() {
        std::unique_lock lock(mutex_);

        if (initialized_) {
            return {};
        }

        try {
            // Ensure Python interpreter is running
            if (!Py_IsInitialized()) {
                py::initialize_interpreter();
            }

            py::gil_scoped_acquire gil;

            // Import the discovery module
            try {
                discoveryModule_ = py::module_::import(
                    "python.tools.common.discovery");
                LOG_F(INFO, "Loaded Python tool discovery module");
            } catch (const py::error_already_set& e) {
                LOG_F(WARNING, "Could not import discovery module: %s", e.what());
                // Try to add the tools path to sys.path
                py::module_::import("sys").attr("path").attr("insert")(
                    0, config_.toolsDirectory.parent_path().string());

                // Retry import
                discoveryModule_ = py::module_::import(
                    "python.tools.common.discovery");
            }

            // Auto-discover if configured
            if (config_.autoDiscover) {
                auto result = discoverToolsImpl();
                if (!result) {
                    LOG_F(WARNING, "Auto-discovery failed: %s",
                          std::string(toolRegistryErrorToString(result.error())).c_str());
                }
            }

            initialized_ = true;
            LOG_F(INFO, "Python tool registry initialized");
            return {};

        } catch (const py::error_already_set& e) {
            LOG_F(ERROR, "Python error during initialization: %s", e.what());
            return std::unexpected(ToolRegistryError::PythonError);
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Error during initialization: %s", e.what());
            return std::unexpected(ToolRegistryError::UnknownError);
        }
    }

    void shutdown() {
        std::unique_lock lock(mutex_);

        if (!initialized_) {
            return;
        }

        try {
            py::gil_scoped_acquire gil;

            // Clear Python references
            discoveryModule_ = py::none();
            tools_.clear();
            categories_.clear();

        } catch (const std::exception& e) {
            LOG_F(WARNING, "Error during shutdown: %s", e.what());
        }

        initialized_ = false;
        LOG_F(INFO, "Python tool registry shutdown");
    }

    bool isInitialized() const noexcept {
        return initialized_.load();
    }

    ToolResult<std::vector<std::string>> discoverToolsImpl() {
        try {
            py::gil_scoped_acquire gil;

            // Call discover_tools function
            py::object result = discoveryModule_.attr("discover_tools")(
                config_.toolsDirectory.string(), true);

            // Parse result
            py::dict resultDict = result.cast<py::dict>();

            if (!resultDict["success"].cast<bool>()) {
                std::string error = resultDict.contains("error")
                    ? resultDict["error"].cast<std::string>()
                    : "Unknown error";
                LOG_F(ERROR, "Tool discovery failed: %s", error.c_str());
                return std::unexpected(ToolRegistryError::DiscoveryFailed);
            }

            // Get registered tools
            std::vector<std::string> registered;
            if (resultDict.contains("registered")) {
                for (const auto& name : resultDict["registered"]) {
                    registered.push_back(name.cast<std::string>());
                }
            }

            // Sync with internal registry
            if (resultDict.contains("registry")) {
                syncFromPython(resultDict["registry"].cast<py::dict>());
            }

            LOG_F(INFO, "Discovered %zu tools", registered.size());
            return registered;

        } catch (const py::error_already_set& e) {
            LOG_F(ERROR, "Python error during discovery: %s", e.what());
            return std::unexpected(ToolRegistryError::PythonError);
        }
    }

    ToolResult<void> discoverTool(std::string_view toolName) {
        std::unique_lock lock(mutex_);

        if (!initialized_) {
            return std::unexpected(ToolRegistryError::NotInitialized);
        }

        try {
            py::gil_scoped_acquire gil;

            // Create discovery instance and discover single tool
            py::object discovery = discoveryModule_.attr("ToolDiscovery")(
                config_.toolsDirectory.string());

            bool success = discovery.attr("discover_tool")(
                std::string(toolName)).cast<bool>();

            if (!success) {
                return std::unexpected(ToolRegistryError::DiscoveryFailed);
            }

            // Sync registry
            py::object registry = discoveryModule_.attr("get_tool_registry")();
            py::dict registryExport = registry.attr("export_registry")().cast<py::dict>();
            syncFromPython(registryExport);

            return {};

        } catch (const py::error_already_set& e) {
            LOG_F(ERROR, "Python error discovering tool %.*s: %s",
                  static_cast<int>(toolName.size()), toolName.data(), e.what());
            return std::unexpected(ToolRegistryError::PythonError);
        }
    }

    ToolResult<void> reloadTool(std::string_view toolName) {
        std::unique_lock lock(mutex_);

        if (!initialized_) {
            return std::unexpected(ToolRegistryError::NotInitialized);
        }

        try {
            py::gil_scoped_acquire gil;

            py::object discovery = discoveryModule_.attr("ToolDiscovery")(
                config_.toolsDirectory.string());

            bool success = discovery.attr("reload_tool")(
                std::string(toolName)).cast<bool>();

            if (!success) {
                return std::unexpected(ToolRegistryError::DiscoveryFailed);
            }

            // Sync registry
            py::object registry = discoveryModule_.attr("get_tool_registry")();
            syncFromPython(registry.attr("export_registry")().cast<py::dict>());

            return {};

        } catch (const py::error_already_set& e) {
            LOG_F(ERROR, "Python error reloading tool %.*s: %s",
                  static_cast<int>(toolName.size()), toolName.data(), e.what());
            return std::unexpected(ToolRegistryError::PythonError);
        }
    }

    ToolResult<void> registerTool(const ToolInfo& info) {
        std::unique_lock lock(mutex_);

        RegisteredTool tool;
        tool.name = info.name;
        tool.modulePath = info.modulePath;
        tool.info = info;
        tool.isLoaded = true;

        for (const auto& func : info.functions) {
            tool.functionNames.push_back(func.name);
        }

        tools_[info.name] = std::move(tool);

        // Update categories
        for (const auto& cat : info.categories) {
            categories_[cat].insert(info.name);
        }

        LOG_F(INFO, "Registered tool: %s", info.name.c_str());
        return {};
    }

    bool unregisterTool(std::string_view toolName) {
        std::unique_lock lock(mutex_);

        std::string name(toolName);
        auto it = tools_.find(name);
        if (it == tools_.end()) {
            return false;
        }

        // Remove from categories
        for (const auto& cat : it->second.info.categories) {
            if (auto catIt = categories_.find(cat); catIt != categories_.end()) {
                catIt->second.erase(name);
            }
        }

        tools_.erase(it);
        LOG_F(INFO, "Unregistered tool: %s", name.c_str());
        return true;
    }

    std::vector<std::string> getToolNames() const {
        std::shared_lock lock(mutex_);
        std::vector<std::string> names;
        names.reserve(tools_.size());
        for (const auto& [name, _] : tools_) {
            names.push_back(name);
        }
        return names;
    }

    bool hasTool(std::string_view toolName) const {
        std::shared_lock lock(mutex_);
        return tools_.contains(std::string(toolName));
    }

    std::optional<ToolInfo> getToolInfo(std::string_view toolName) const {
        std::shared_lock lock(mutex_);
        auto it = tools_.find(std::string(toolName));
        if (it != tools_.end()) {
            return it->second.info;
        }
        return std::nullopt;
    }

    std::optional<ToolFunctionInfo> getFunctionInfo(
        std::string_view toolName,
        std::string_view functionName
    ) const {
        std::shared_lock lock(mutex_);
        auto it = tools_.find(std::string(toolName));
        if (it == tools_.end()) {
            return std::nullopt;
        }

        for (const auto& func : it->second.info.functions) {
            if (func.name == functionName) {
                return func;
            }
        }
        return std::nullopt;
    }

    std::vector<std::string> getToolsByCategory(std::string_view category) const {
        std::shared_lock lock(mutex_);
        auto it = categories_.find(std::string(category));
        if (it != categories_.end()) {
            return {it->second.begin(), it->second.end()};
        }
        return {};
    }

    std::vector<std::string> getCategories() const {
        std::shared_lock lock(mutex_);
        std::vector<std::string> cats;
        cats.reserve(categories_.size());
        for (const auto& [cat, _] : categories_) {
            cats.push_back(cat);
        }
        return cats;
    }

    ToolResult<ToolInvocationResult> invoke(
        std::string_view toolName,
        std::string_view functionName,
        const nlohmann::json& args
    ) {
        if (!initialized_) {
            return std::unexpected(ToolRegistryError::NotInitialized);
        }

        auto start = std::chrono::steady_clock::now();

        {
            std::shared_lock lock(mutex_);
            if (!tools_.contains(std::string(toolName))) {
                return std::unexpected(ToolRegistryError::ToolNotFound);
            }
        }

        try {
            py::gil_scoped_acquire gil;

            // Call invoke_tool function
            std::string argsJson = args.dump();
            py::object result = discoveryModule_.attr("invoke_tool")(
                std::string(toolName),
                std::string(functionName),
                argsJson
            );

            // Parse result
            py::dict resultDict = result.cast<py::dict>();

            ToolInvocationResult invResult;
            invResult.success = resultDict["success"].cast<bool>();

            if (resultDict.contains("data")) {
                // Convert Python object to JSON
                py::module_ json = py::module_::import("json");
                std::string dataJson = json.attr("dumps")(
                    resultDict["data"]).cast<std::string>();
                invResult.data = nlohmann::json::parse(dataJson);
            }

            if (resultDict.contains("error")) {
                invResult.error = resultDict["error"].cast<std::string>();
            }
            if (resultDict.contains("error_type")) {
                invResult.errorType = resultDict["error_type"].cast<std::string>();
            }
            if (resultDict.contains("traceback")) {
                invResult.traceback = resultDict["traceback"].cast<std::string>();
            }

            auto end = std::chrono::steady_clock::now();
            invResult.executionTime = std::chrono::duration_cast<
                std::chrono::milliseconds>(end - start);

            // Update statistics
            stats_.totalInvocations++;
            if (invResult.success) {
                stats_.successfulInvocations++;
            } else {
                stats_.failedInvocations++;
            }
            stats_.totalExecutionTime += invResult.executionTime;

            return invResult;

        } catch (const py::error_already_set& e) {
            stats_.totalInvocations++;
            stats_.failedInvocations++;

            ToolInvocationResult errorResult;
            errorResult.success = false;
            errorResult.error = e.what();
            errorResult.errorType = "PythonError";

            auto end = std::chrono::steady_clock::now();
            errorResult.executionTime = std::chrono::duration_cast<
                std::chrono::milliseconds>(end - start);

            return errorResult;
        }
    }

    ToolResult<ToolInvocationResult> invokeWithTimeout(
        std::string_view toolName,
        std::string_view functionName,
        const nlohmann::json& args,
        std::chrono::milliseconds timeout
    ) {
        std::promise<ToolResult<ToolInvocationResult>> promise;
        auto future = promise.get_future();

        std::thread worker([this, toolName = std::string(toolName),
                           functionName = std::string(functionName),
                           args, &promise]() {
            try {
                auto result = invoke(toolName, functionName, args);
                promise.set_value(std::move(result));
            } catch (...) {
                promise.set_exception(std::current_exception());
            }
        });

        if (future.wait_for(timeout) == std::future_status::timeout) {
            worker.detach();  // Let it run in background, but we don't wait
            return std::unexpected(ToolRegistryError::Timeout);
        }

        worker.join();
        return future.get();
    }

    std::future<ToolResult<ToolInvocationResult>> invokeAsync(
        std::string_view toolName,
        std::string_view functionName,
        const nlohmann::json& args
    ) {
        return std::async(std::launch::async, [this, toolName = std::string(toolName),
                                               functionName = std::string(functionName),
                                               args]() {
            return invoke(toolName, functionName, args);
        });
    }

    void setEventCallback(ToolEventCallback callback) {
        std::unique_lock lock(mutex_);
        eventCallback_ = std::move(callback);
    }

    nlohmann::json exportToJson() const {
        std::shared_lock lock(mutex_);

        nlohmann::json j;
        j["tools"] = nlohmann::json::object();
        for (const auto& [name, tool] : tools_) {
            j["tools"][name] = tool.toJson();
        }

        j["categories"] = nlohmann::json::object();
        for (const auto& [cat, names] : categories_) {
            j["categories"][cat] = std::vector<std::string>(names.begin(), names.end());
        }

        j["count"] = tools_.size();
        return j;
    }

    Statistics getStatistics() const {
        return stats_;
    }

private:
    void syncFromPython(const py::dict& registryDict) {
        tools_.clear();
        categories_.clear();

        if (registryDict.contains("tools")) {
            py::dict toolsDict = registryDict["tools"].cast<py::dict>();
            for (auto& [name, toolObj] : toolsDict) {
                py::dict toolDict = toolObj.cast<py::dict>();

                RegisteredTool tool;
                tool.name = name.cast<std::string>();
                tool.modulePath = toolDict.contains("module_path")
                    ? toolDict["module_path"].cast<std::string>() : "";
                tool.isLoaded = toolDict.contains("is_loaded")
                    ? toolDict["is_loaded"].cast<bool>() : false;

                if (toolDict.contains("error")) {
                    tool.loadError = toolDict["error"].cast<std::string>();
                }

                if (toolDict.contains("info")) {
                    py::module_ json = py::module_::import("json");
                    std::string infoJson = json.attr("dumps")(
                        toolDict["info"]).cast<std::string>();
                    tool.info = ToolInfo::fromJson(nlohmann::json::parse(infoJson));
                }

                if (toolDict.contains("function_names")) {
                    for (const auto& fn : toolDict["function_names"]) {
                        tool.functionNames.push_back(fn.cast<std::string>());
                    }
                }

                // Update categories
                for (const auto& cat : tool.info.categories) {
                    categories_[cat].insert(tool.name);
                }

                tools_[tool.name] = std::move(tool);
            }
        }

        stats_.totalTools = tools_.size();
        stats_.loadedTools = 0;
        stats_.totalFunctions = 0;
        for (const auto& [_, tool] : tools_) {
            if (tool.isLoaded) {
                stats_.loadedTools++;
            }
            stats_.totalFunctions += tool.functionNames.size();
        }
    }

    ToolRegistryConfig config_;
    std::atomic<bool> initialized_;
    mutable std::shared_mutex mutex_;

    py::object discoveryModule_;
    std::unordered_map<std::string, RegisteredTool> tools_;
    std::unordered_map<std::string, std::unordered_set<std::string>> categories_;
    ToolEventCallback eventCallback_;

    mutable Statistics stats_;
};

// =============================================================================
// PythonToolRegistry Public API
// =============================================================================

std::unique_ptr<PythonToolRegistry> PythonToolRegistry::globalInstance_;
std::shared_mutex PythonToolRegistry::globalMutex_;

PythonToolRegistry::PythonToolRegistry()
    : pImpl_(std::make_unique<Impl>(ToolRegistryConfig{})) {}

PythonToolRegistry::PythonToolRegistry(const ToolRegistryConfig& config)
    : pImpl_(std::make_unique<Impl>(config)) {}

PythonToolRegistry::~PythonToolRegistry() = default;

PythonToolRegistry::PythonToolRegistry(PythonToolRegistry&&) noexcept = default;
PythonToolRegistry& PythonToolRegistry::operator=(PythonToolRegistry&&) noexcept = default;

ToolResult<void> PythonToolRegistry::initialize() {
    return pImpl_->initialize();
}

bool PythonToolRegistry::isInitialized() const noexcept {
    return pImpl_->isInitialized();
}

void PythonToolRegistry::shutdown() {
    pImpl_->shutdown();
}

ToolResult<std::vector<std::string>> PythonToolRegistry::discoverTools() {
    return pImpl_->discoverToolsImpl();
}

ToolResult<void> PythonToolRegistry::discoverTool(std::string_view toolName) {
    return pImpl_->discoverTool(toolName);
}

ToolResult<void> PythonToolRegistry::reloadTool(std::string_view toolName) {
    return pImpl_->reloadTool(toolName);
}

ToolResult<void> PythonToolRegistry::registerTool(const ToolInfo& info) {
    return pImpl_->registerTool(info);
}

bool PythonToolRegistry::unregisterTool(std::string_view toolName) {
    return pImpl_->unregisterTool(toolName);
}

std::vector<std::string> PythonToolRegistry::getToolNames() const {
    return pImpl_->getToolNames();
}

bool PythonToolRegistry::hasTool(std::string_view toolName) const {
    return pImpl_->hasTool(toolName);
}

std::optional<ToolInfo> PythonToolRegistry::getToolInfo(
    std::string_view toolName) const {
    return pImpl_->getToolInfo(toolName);
}

std::optional<ToolFunctionInfo> PythonToolRegistry::getFunctionInfo(
    std::string_view toolName,
    std::string_view functionName
) const {
    return pImpl_->getFunctionInfo(toolName, functionName);
}

std::vector<std::string> PythonToolRegistry::getToolsByCategory(
    std::string_view category
) const {
    return pImpl_->getToolsByCategory(category);
}

std::vector<std::string> PythonToolRegistry::getCategories() const {
    return pImpl_->getCategories();
}

ToolResult<ToolInvocationResult> PythonToolRegistry::invoke(
    std::string_view toolName,
    std::string_view functionName,
    const nlohmann::json& args
) {
    return pImpl_->invoke(toolName, functionName, args);
}

ToolResult<ToolInvocationResult> PythonToolRegistry::invokeWithTimeout(
    std::string_view toolName,
    std::string_view functionName,
    const nlohmann::json& args,
    std::chrono::milliseconds timeout
) {
    return pImpl_->invokeWithTimeout(toolName, functionName, args, timeout);
}

std::future<ToolResult<ToolInvocationResult>> PythonToolRegistry::invokeAsync(
    std::string_view toolName,
    std::string_view functionName,
    const nlohmann::json& args
) {
    return pImpl_->invokeAsync(toolName, functionName, args);
}

void PythonToolRegistry::setEventCallback(ToolEventCallback callback) {
    pImpl_->setEventCallback(std::move(callback));
}

nlohmann::json PythonToolRegistry::exportToJson() const {
    return pImpl_->exportToJson();
}

std::string PythonToolRegistry::exportToJsonString() const {
    return exportToJson().dump(2);
}

PythonToolRegistry::Statistics PythonToolRegistry::getStatistics() const {
    return pImpl_->getStatistics();
}

PythonToolRegistry& PythonToolRegistry::getInstance() {
    std::shared_lock lock(globalMutex_);
    if (!globalInstance_) {
        throw std::runtime_error("Global registry not initialized");
    }
    return *globalInstance_;
}

ToolResult<void> PythonToolRegistry::initializeGlobal(
    const ToolRegistryConfig& config
) {
    std::unique_lock lock(globalMutex_);
    if (globalInstance_) {
        return {};  // Already initialized
    }

    globalInstance_ = std::make_unique<PythonToolRegistry>(config);
    return globalInstance_->initialize();
}

// =============================================================================
// ToolInvocationGuard
// =============================================================================

class ToolInvocationGuard::Impl {
public:
    Impl() {
        // Acquire GIL
        gilState_ = PyGILState_Ensure();
    }

    ~Impl() {
        // Release GIL
        PyGILState_Release(gilState_);
    }

private:
    PyGILState_STATE gilState_;
};

ToolInvocationGuard::ToolInvocationGuard(PythonToolRegistry& /*registry*/)
    : pImpl_(std::make_unique<Impl>()) {}

ToolInvocationGuard::~ToolInvocationGuard() = default;

}  // namespace lithium::tools
