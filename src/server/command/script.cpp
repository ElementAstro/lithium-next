/*
 * script.cpp - Script Command Handlers
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#include "script.hpp"

#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "atom/type/json.hpp"
#include "constant/constant.hpp"
#include "response.hpp"
#include "server/command.hpp"

#include "script/isolated/runner.hpp"
#include "script/shell/script_manager.hpp"
#include "script/tools/tool_registry.hpp"
#include "script/venv/venv_manager.hpp"

namespace lithium::app {

using json = nlohmann::json;
using command::CommandResponse;

namespace {

auto getIsolatedRunner() -> std::shared_ptr<isolated::PythonRunner> {
    return GET_OR_CREATE_WEAK_PTR(ISOLATED_PYTHON_RUNNER,
                                  isolated::PythonRunner);
}

auto getToolRegistry() -> std::shared_ptr<tools::PythonToolRegistry> {
    return GET_OR_CREATE_WEAK_PTR(PYTHON_TOOL_REGISTRY,
                                  tools::PythonToolRegistry);
}

auto getVenvManager() -> std::shared_ptr<venv::VenvManager> {
    return GET_OR_CREATE_WEAK_PTR(VENV_MANAGER, venv::VenvManager);
}

auto getScriptManager() -> std::shared_ptr<shell::ScriptManager> {
    return GET_OR_CREATE_WEAK_PTR(SCRIPT_MANAGER, shell::ScriptManager);
}

}  // namespace

void registerScript(std::shared_ptr<CommandDispatcher> dispatcher) {
    // =========================================================================
    // Isolated Python Execution Commands
    // =========================================================================

    // script.execute - Execute Python script content
    dispatcher->registerCommand<json>("script.execute", [](json& payload) {
        if (!payload.contains("code") || !payload["code"].is_string()) {
            LOG_WARN("script.execute: missing code");
            payload = CommandResponse::missingParameter("code");
            return;
        }
        std::string code = payload["code"].get<std::string>();
        json args = payload.value("args", json::object());

        LOG_INFO("Executing script.execute with {} bytes of code", code.size());

        try {
            auto runner = getIsolatedRunner();
            if (!runner) {
                payload = CommandResponse::serviceUnavailable(
                    "IsolatedPythonRunner");
                return;
            }

            auto result = runner->execute(code, args);
            if (result.success) {
                payload = CommandResponse::success({
                    {"result", result.result},
                    {"stdout", result.stdoutOutput},
                    {"stderr", result.stderrOutput},
                    {"exitCode", result.exitCode},
                    {"executionTime", result.executionTimeMs}
                });
            } else {
                payload = CommandResponse::operationFailed("execute",
                    result.errorMessage.value_or("Unknown error"));
            }
        } catch (const std::exception& e) {
            LOG_ERROR("script.execute exception: {}", e.what());
            payload = CommandResponse::operationFailed("execute", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'script.execute'");

    // script.executeFile - Execute Python script from file
    dispatcher->registerCommand<json>("script.executeFile", [](json& payload) {
        if (!payload.contains("path") || !payload["path"].is_string()) {
            LOG_WARN("script.executeFile: missing path");
            payload = CommandResponse::missingParameter("path");
            return;
        }
        std::string path = payload["path"].get<std::string>();
        json args = payload.value("args", json::object());

        LOG_INFO("Executing script.executeFile: {}", path);

        try {
            auto runner = getIsolatedRunner();
            if (!runner) {
                payload = CommandResponse::serviceUnavailable(
                    "IsolatedPythonRunner");
                return;
            }

            auto result = runner->executeFile(path, args);
            if (result.success) {
                payload = CommandResponse::success({
                    {"result", result.result},
                    {"stdout", result.stdoutOutput},
                    {"stderr", result.stderrOutput},
                    {"exitCode", result.exitCode},
                    {"executionTime", result.executionTimeMs}
                });
            } else {
                payload = CommandResponse::operationFailed("executeFile",
                    result.errorMessage.value_or("Unknown error"));
            }
        } catch (const std::exception& e) {
            LOG_ERROR("script.executeFile exception: {}", e.what());
            payload = CommandResponse::operationFailed("executeFile", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'script.executeFile'");

    // script.executeFunction - Execute specific function from module
    dispatcher->registerCommand<json>(
        "script.executeFunction", [](json& payload) {
            if (!payload.contains("module") ||
                !payload["module"].is_string()) {
                LOG_WARN("script.executeFunction: missing module");
                payload = CommandResponse::missingParameter("module");
                return;
            }
            if (!payload.contains("function") ||
                !payload["function"].is_string()) {
                LOG_WARN("script.executeFunction: missing function");
                payload = CommandResponse::missingParameter("function");
                return;
            }

            std::string moduleName = payload["module"].get<std::string>();
            std::string functionName = payload["function"].get<std::string>();
            json args = payload.value("args", json::object());

            LOG_INFO("Executing script.executeFunction: {}.{}", moduleName,
                     functionName);

            try {
                auto runner = getIsolatedRunner();
                if (!runner) {
                    payload = CommandResponse::serviceUnavailable(
                        "IsolatedPythonRunner");
                    return;
                }

                auto result =
                    runner->executeFunction(moduleName, functionName, args);
                if (result.success) {
                    payload = CommandResponse::success({
                        {"result", result.result},
                        {"stdout", result.stdoutOutput},
                        {"stderr", result.stderrOutput},
                        {"exitCode", result.exitCode},
                        {"executionTime", result.executionTimeMs}
                    });
                } else {
                    payload = CommandResponse::operationFailed(
                        "executeFunction",
                        result.errorMessage.value_or("Unknown error"));
                }
            } catch (const std::exception& e) {
                LOG_ERROR("script.executeFunction exception: {}", e.what());
                payload =
                    CommandResponse::operationFailed("executeFunction", e.what());
            }
        });
    LOG_INFO("Registered command handler for 'script.executeFunction'");

    // script.cancel - Cancel running script execution
    dispatcher->registerCommand<json>("script.cancel", [](json& payload) {
        LOG_INFO("Executing script.cancel");

        try {
            auto runner = getIsolatedRunner();
            if (!runner) {
                payload =
                    CommandResponse::serviceUnavailable("IsolatedPythonRunner");
                return;
            }

            bool cancelled = runner->cancel();
            if (cancelled) {
                payload = CommandResponse::success({{"cancelled", true}});
            } else {
                payload = CommandResponse::operationFailed(
                    "cancel", "No script is currently running");
            }
        } catch (const std::exception& e) {
            LOG_ERROR("script.cancel exception: {}", e.what());
            payload = CommandResponse::operationFailed("cancel", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'script.cancel'");

    // script.status - Query execution status
    dispatcher->registerCommand<json>("script.status", [](json& payload) {
        LOG_DEBUG("Executing script.status");

        try {
            auto runner = getIsolatedRunner();
            if (!runner) {
                payload =
                    CommandResponse::serviceUnavailable("IsolatedPythonRunner");
                return;
            }

            json status;
            status["running"] = runner->isRunning();

            auto pid = runner->getProcessId();
            if (pid) {
                status["processId"] = *pid;
            }

            auto mem = runner->getCurrentMemoryUsage();
            if (mem) {
                status["memoryUsage"] = *mem;
            }

            auto cpu = runner->getCurrentCpuUsage();
            if (cpu) {
                status["cpuUsage"] = *cpu;
            }

            payload = CommandResponse::success(status);
        } catch (const std::exception& e) {
            LOG_ERROR("script.status exception: {}", e.what());
            payload = CommandResponse::operationFailed("status", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'script.status'");

    // =========================================================================
    // Shell Script Commands
    // =========================================================================

    // script.shell.execute - Execute shell script
    dispatcher->registerCommand<json>(
        "script.shell.execute", [](json& payload) {
            if (!payload.contains("name") || !payload["name"].is_string()) {
                LOG_WARN("script.shell.execute: missing name");
                payload = CommandResponse::missingParameter("name");
                return;
            }

            std::string name = payload["name"].get<std::string>();
            bool safe = payload.value("safe", true);

            std::unordered_map<std::string, std::string> args;
            if (payload.contains("args") && payload["args"].is_object()) {
                for (auto& [key, val] : payload["args"].items()) {
                    if (val.is_string()) {
                        args[key] = val.get<std::string>();
                    }
                }
            }

            LOG_INFO("Executing script.shell.execute: {}", name);

            try {
                auto manager = getScriptManager();
                if (!manager) {
                    payload =
                        CommandResponse::serviceUnavailable("ScriptManager");
                    return;
                }

                auto result = manager->runScript(name, args, safe);
                if (result) {
                    payload = CommandResponse::success({
                        {"output", result->first},
                        {"exitCode", result->second}
                    });
                } else {
                    payload = CommandResponse::operationFailed(
                        "shell.execute", "Script not found or execution failed");
                }
            } catch (const std::exception& e) {
                LOG_ERROR("script.shell.execute exception: {}", e.what());
                payload =
                    CommandResponse::operationFailed("shell.execute", e.what());
            }
        });
    LOG_INFO("Registered command handler for 'script.shell.execute'");

    // script.shell.list - List registered shell scripts
    dispatcher->registerCommand<json>("script.shell.list", [](json& payload) {
        LOG_DEBUG("Executing script.shell.list");

        try {
            auto manager = getScriptManager();
            if (!manager) {
                payload = CommandResponse::serviceUnavailable("ScriptManager");
                return;
            }

            auto scripts = manager->getAllScripts();
            json scriptList = json::array();
            for (const auto& [name, script] : scripts) {
                scriptList.push_back(name);
            }

            payload = CommandResponse::success({{"scripts", scriptList}});
        } catch (const std::exception& e) {
            LOG_ERROR("script.shell.list exception: {}", e.what());
            payload = CommandResponse::operationFailed("shell.list", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'script.shell.list'");

    // =========================================================================
    // Tool Registry Commands
    // =========================================================================

    // script.tool.list - List available Python tools
    dispatcher->registerCommand<json>("script.tool.list", [](json& payload) {
        LOG_DEBUG("Executing script.tool.list");

        try {
            auto registry = getToolRegistry();
            if (!registry) {
                payload =
                    CommandResponse::serviceUnavailable("PythonToolRegistry");
                return;
            }

            auto tools = registry->getToolNames();
            payload = CommandResponse::success({{"tools", tools}});
        } catch (const std::exception& e) {
            LOG_ERROR("script.tool.list exception: {}", e.what());
            payload = CommandResponse::operationFailed("tool.list", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'script.tool.list'");

    // script.tool.info - Get tool information
    dispatcher->registerCommand<json>("script.tool.info", [](json& payload) {
        if (!payload.contains("name") || !payload["name"].is_string()) {
            LOG_WARN("script.tool.info: missing name");
            payload = CommandResponse::missingParameter("name");
            return;
        }

        std::string toolName = payload["name"].get<std::string>();
        LOG_DEBUG("Executing script.tool.info for: {}", toolName);

        try {
            auto registry = getToolRegistry();
            if (!registry) {
                payload =
                    CommandResponse::serviceUnavailable("PythonToolRegistry");
                return;
            }

            auto info = registry->getToolInfo(toolName);
            if (info) {
                json toolJson;
                toolJson["name"] = info->name;
                toolJson["version"] = info->version;
                toolJson["category"] = info->category;
                toolJson["description"] = info->description;

                json funcs = json::array();
                for (const auto& func : info->functions) {
                    funcs.push_back({
                        {"name", func.name},
                        {"description", func.description},
                        {"returnType", func.returnType}
                    });
                }
                toolJson["functions"] = funcs;

                payload = CommandResponse::success(toolJson);
            } else {
                payload = CommandResponse::error("tool_not_found",
                                                  "Tool not found: " + toolName);
            }
        } catch (const std::exception& e) {
            LOG_ERROR("script.tool.info exception: {}", e.what());
            payload = CommandResponse::operationFailed("tool.info", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'script.tool.info'");

    // script.tool.invoke - Invoke Python tool function
    dispatcher->registerCommand<json>("script.tool.invoke", [](json& payload) {
        if (!payload.contains("tool") || !payload["tool"].is_string()) {
            LOG_WARN("script.tool.invoke: missing tool");
            payload = CommandResponse::missingParameter("tool");
            return;
        }
        if (!payload.contains("function") ||
            !payload["function"].is_string()) {
            LOG_WARN("script.tool.invoke: missing function");
            payload = CommandResponse::missingParameter("function");
            return;
        }

        std::string toolName = payload["tool"].get<std::string>();
        std::string functionName = payload["function"].get<std::string>();
        json args = payload.value("args", json::object());

        LOG_INFO("Executing script.tool.invoke: {}.{}", toolName, functionName);

        try {
            auto registry = getToolRegistry();
            if (!registry) {
                payload =
                    CommandResponse::serviceUnavailable("PythonToolRegistry");
                return;
            }

            auto result = registry->invoke(toolName, functionName, args);
            if (result) {
                payload = CommandResponse::success({
                    {"result", result->result},
                    {"executionTime", result->executionTimeMs}
                });
            } else {
                payload = CommandResponse::operationFailed(
                    "tool.invoke",
                    "Invocation failed: " + std::string(result.error().message));
            }
        } catch (const std::exception& e) {
            LOG_ERROR("script.tool.invoke exception: {}", e.what());
            payload = CommandResponse::operationFailed("tool.invoke", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'script.tool.invoke'");

    // script.tool.discover - Discover new tools
    dispatcher->registerCommand<json>("script.tool.discover", [](json& payload) {
        LOG_INFO("Executing script.tool.discover");

        try {
            auto registry = getToolRegistry();
            if (!registry) {
                payload =
                    CommandResponse::serviceUnavailable("PythonToolRegistry");
                return;
            }

            auto result = registry->discoverTools();
            if (result) {
                payload = CommandResponse::success({{"discovered", *result}});
            } else {
                payload = CommandResponse::operationFailed(
                    "tool.discover",
                    "Discovery failed: " + std::string(result.error().message));
            }
        } catch (const std::exception& e) {
            LOG_ERROR("script.tool.discover exception: {}", e.what());
            payload =
                CommandResponse::operationFailed("tool.discover", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'script.tool.discover'");

    // =========================================================================
    // Virtual Environment Commands
    // =========================================================================

    // script.venv.list - List virtual environments
    dispatcher->registerCommand<json>("script.venv.list", [](json& payload) {
        LOG_DEBUG("Executing script.venv.list");

        try {
            auto manager = getVenvManager();
            if (!manager) {
                payload = CommandResponse::serviceUnavailable("VenvManager");
                return;
            }

            // Check for conda environments
            auto condaEnvs = manager->listCondaEnvs();
            json envList = json::array();

            if (condaEnvs) {
                for (const auto& env : *condaEnvs) {
                    envList.push_back({
                        {"name", env.name},
                        {"path", env.path.string()},
                        {"pythonVersion", env.pythonVersion},
                        {"type", env.type == venv::VenvType::Conda ? "conda" : "venv"}
                    });
                }
            }

            // Add current venv if active
            auto current = manager->getCurrentVenvInfo();
            if (current) {
                json currentEnv;
                currentEnv["name"] = current->name;
                currentEnv["path"] = current->path.string();
                currentEnv["pythonVersion"] = current->pythonVersion;
                currentEnv["active"] = true;

                // Check if already in list
                bool found = false;
                for (auto& e : envList) {
                    if (e["path"] == currentEnv["path"]) {
                        e["active"] = true;
                        found = true;
                        break;
                    }
                }
                if (!found) {
                    envList.push_back(currentEnv);
                }
            }

            payload = CommandResponse::success({{"environments", envList}});
        } catch (const std::exception& e) {
            LOG_ERROR("script.venv.list exception: {}", e.what());
            payload = CommandResponse::operationFailed("venv.list", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'script.venv.list'");

    // script.venv.packages - List packages in venv
    dispatcher->registerCommand<json>("script.venv.packages", [](json& payload) {
        LOG_DEBUG("Executing script.venv.packages");

        try {
            auto manager = getVenvManager();
            if (!manager) {
                payload = CommandResponse::serviceUnavailable("VenvManager");
                return;
            }

            auto packages = manager->listInstalledPackages();
            if (packages) {
                json pkgList = json::array();
                for (const auto& pkg : *packages) {
                    pkgList.push_back({
                        {"name", pkg.name},
                        {"version", pkg.version},
                        {"location", pkg.location.string()}
                    });
                }
                payload = CommandResponse::success({{"packages", pkgList}});
            } else {
                payload = CommandResponse::operationFailed(
                    "venv.packages",
                    "Failed to list packages: " +
                        std::string(packages.error().message));
            }
        } catch (const std::exception& e) {
            LOG_ERROR("script.venv.packages exception: {}", e.what());
            payload =
                CommandResponse::operationFailed("venv.packages", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'script.venv.packages'");

    // script.venv.install - Install a package
    dispatcher->registerCommand<json>("script.venv.install", [](json& payload) {
        if (!payload.contains("package") || !payload["package"].is_string()) {
            LOG_WARN("script.venv.install: missing package");
            payload = CommandResponse::missingParameter("package");
            return;
        }

        std::string package = payload["package"].get<std::string>();
        bool upgrade = payload.value("upgrade", false);

        LOG_INFO("Executing script.venv.install: {}", package);

        try {
            auto manager = getVenvManager();
            if (!manager) {
                payload = CommandResponse::serviceUnavailable("VenvManager");
                return;
            }

            auto result = manager->installPackage(package, upgrade);
            if (result) {
                payload = CommandResponse::success(
                    {{"installed", package}, {"upgrade", upgrade}});
            } else {
                payload = CommandResponse::operationFailed(
                    "venv.install",
                    "Installation failed: " + std::string(result.error().message));
            }
        } catch (const std::exception& e) {
            LOG_ERROR("script.venv.install exception: {}", e.what());
            payload = CommandResponse::operationFailed("venv.install", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'script.venv.install'");

    // script.venv.uninstall - Uninstall a package
    dispatcher->registerCommand<json>(
        "script.venv.uninstall", [](json& payload) {
            if (!payload.contains("package") ||
                !payload["package"].is_string()) {
                LOG_WARN("script.venv.uninstall: missing package");
                payload = CommandResponse::missingParameter("package");
                return;
            }

            std::string package = payload["package"].get<std::string>();

            LOG_INFO("Executing script.venv.uninstall: {}", package);

            try {
                auto manager = getVenvManager();
                if (!manager) {
                    payload = CommandResponse::serviceUnavailable("VenvManager");
                    return;
                }

                auto result = manager->uninstallPackage(package);
                if (result) {
                    payload =
                        CommandResponse::success({{"uninstalled", package}});
                } else {
                    payload = CommandResponse::operationFailed(
                        "venv.uninstall",
                        "Uninstallation failed: " +
                            std::string(result.error().message));
                }
            } catch (const std::exception& e) {
                LOG_ERROR("script.venv.uninstall exception: {}", e.what());
                payload =
                    CommandResponse::operationFailed("venv.uninstall", e.what());
            }
        });
    LOG_INFO("Registered command handler for 'script.venv.uninstall'");

    // script.venv.create - Create a virtual environment
    dispatcher->registerCommand<json>("script.venv.create", [](json& payload) {
        if (!payload.contains("path") || !payload["path"].is_string()) {
            LOG_WARN("script.venv.create: missing path");
            payload = CommandResponse::missingParameter("path");
            return;
        }

        std::string path = payload["path"].get<std::string>();
        std::string pythonVersion = payload.value("pythonVersion", "");

        LOG_INFO("Executing script.venv.create: {}", path);

        try {
            auto manager = getVenvManager();
            if (!manager) {
                payload = CommandResponse::serviceUnavailable("VenvManager");
                return;
            }

            auto result = manager->createVenv(path, pythonVersion);
            if (result) {
                payload = CommandResponse::success({
                    {"path", result->path.string()},
                    {"pythonVersion", result->pythonVersion},
                    {"created", true}
                });
            } else {
                payload = CommandResponse::operationFailed(
                    "venv.create",
                    "Creation failed: " + std::string(result.error().message));
            }
        } catch (const std::exception& e) {
            LOG_ERROR("script.venv.create exception: {}", e.what());
            payload = CommandResponse::operationFailed("venv.create", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'script.venv.create'");

    // script.venv.activate - Activate a virtual environment
    dispatcher->registerCommand<json>("script.venv.activate", [](json& payload) {
        if (!payload.contains("path") || !payload["path"].is_string()) {
            LOG_WARN("script.venv.activate: missing path");
            payload = CommandResponse::missingParameter("path");
            return;
        }

        std::string path = payload["path"].get<std::string>();

        LOG_INFO("Executing script.venv.activate: {}", path);

        try {
            auto manager = getVenvManager();
            if (!manager) {
                payload = CommandResponse::serviceUnavailable("VenvManager");
                return;
            }

            auto result = manager->activateVenv(path);
            if (result) {
                payload = CommandResponse::success(
                    {{"path", path}, {"activated", true}});
            } else {
                payload = CommandResponse::operationFailed(
                    "venv.activate",
                    "Activation failed: " + std::string(result.error().message));
            }
        } catch (const std::exception& e) {
            LOG_ERROR("script.venv.activate exception: {}", e.what());
            payload =
                CommandResponse::operationFailed("venv.activate", e.what());
        }
    });
    LOG_INFO("Registered command handler for 'script.venv.activate'");

    // script.venv.deactivate - Deactivate current virtual environment
    dispatcher->registerCommand<json>(
        "script.venv.deactivate", [](json& payload) {
            LOG_INFO("Executing script.venv.deactivate");

            try {
                auto manager = getVenvManager();
                if (!manager) {
                    payload = CommandResponse::serviceUnavailable("VenvManager");
                    return;
                }

                auto result = manager->deactivateVenv();
                if (result) {
                    payload =
                        CommandResponse::success({{"deactivated", true}});
                } else {
                    payload = CommandResponse::operationFailed(
                        "venv.deactivate",
                        "Deactivation failed: " +
                            std::string(result.error().message));
                }
            } catch (const std::exception& e) {
                LOG_ERROR("script.venv.deactivate exception: {}", e.what());
                payload =
                    CommandResponse::operationFailed("venv.deactivate", e.what());
            }
        });
    LOG_INFO("Registered command handler for 'script.venv.deactivate'");
}

}  // namespace lithium::app
