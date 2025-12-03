/*
 * VenvController.hpp
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 */

#ifndef LITHIUM_SERVER_CONTROLLER_VENV_HPP
#define LITHIUM_SERVER_CONTROLLER_VENV_HPP

#include "../../utils/response.hpp"
#include "../controller.hpp"

#include <memory>
#include <string>

#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "constant/constant.hpp"
#include "script/venv/venv_manager.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

/**
 * @brief Controller for virtual environment management via HTTP API
 *
 * Provides REST endpoints for:
 * - Creating and managing Python virtual environments
 * - Installing and uninstalling packages
 * - Environment activation and discovery
 */
class VenvController : public Controller {
private:
    static std::weak_ptr<lithium::venv::VenvManager> mManager;

    static auto handleVenvAction(
        const crow::request& req, const nlohmann::json& body,
        const std::string& command,
        std::function<crow::response(std::shared_ptr<lithium::venv::VenvManager>)>
            func) -> crow::response {
        try {
            auto manager = mManager.lock();
            if (!manager) {
                spdlog::error(
                    "VenvManager instance is null. Unable to proceed with "
                    "command: {}",
                    command);
                return ResponseBuilder::internalError(
                    "VenvManager instance is null.");
            }
            return func(manager);
        } catch (const std::invalid_argument& e) {
            spdlog::error(
                "Invalid argument while executing command: {}. Exception: {}",
                command, e.what());
            return ResponseBuilder::badRequest(e.what());
        } catch (const std::runtime_error& e) {
            spdlog::error(
                "Runtime error while executing command: {}. Exception: {}",
                command, e.what());
            return ResponseBuilder::internalError(e.what());
        } catch (const std::exception& e) {
            spdlog::error(
                "Exception occurred while executing command: {}. Exception: {}",
                command, e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

public:
    void registerRoutes(lithium::server::ServerApp& app) override {
        GET_OR_CREATE_WEAK_PTR(mManager, lithium::venv::VenvManager,
                               Constants::VENV_MANAGER);

        // Environment management
        CROW_ROUTE(app, "/venv/create")
            .methods("POST"_method)(&VenvController::createEnv, this);
        CROW_ROUTE(app, "/venv/delete")
            .methods("POST"_method)(&VenvController::deleteEnv, this);
        CROW_ROUTE(app, "/venv/list")
            .methods("GET"_method)(&VenvController::listEnvs, this);
        CROW_ROUTE(app, "/venv/info")
            .methods("POST"_method)(&VenvController::getEnvInfo, this);

        // Activation
        CROW_ROUTE(app, "/venv/activate")
            .methods("POST"_method)(&VenvController::activateEnv, this);
        CROW_ROUTE(app, "/venv/deactivate")
            .methods("POST"_method)(&VenvController::deactivateEnv, this);
        CROW_ROUTE(app, "/venv/current")
            .methods("GET"_method)(&VenvController::getCurrentEnv, this);

        // Package management
        CROW_ROUTE(app, "/venv/install")
            .methods("POST"_method)(&VenvController::installPackage, this);
        CROW_ROUTE(app, "/venv/uninstall")
            .methods("POST"_method)(&VenvController::uninstallPackage, this);
        CROW_ROUTE(app, "/venv/packages")
            .methods("POST"_method)(&VenvController::listPackages, this);
        CROW_ROUTE(app, "/venv/requirements")
            .methods("POST"_method)(&VenvController::installRequirements, this);

        // Discovery
        CROW_ROUTE(app, "/venv/discover")
            .methods("POST"_method)(&VenvController::discoverEnvs, this);
    }

    // Create a new virtual environment
    void createEnv(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleVenvAction(
                req, body, "createEnv",
                [&](auto manager) -> crow::response {
                    lithium::venv::VenvConfig config;
                    config.name = body["name"].get<std::string>();
                    config.basePath = body.value("basePath", "venvs");
                    config.systemSitePackages = body.value("systemSitePackages", false);
                    config.clear = body.value("clear", false);
                    config.upgrade = body.value("upgrade", false);

                    if (body.contains("pythonPath")) {
                        config.pythonPath = body["pythonPath"].get<std::string>();
                    }

                    auto result = manager->createVenv(config);
                    if (!result) {
                        return ResponseBuilder::internalError(
                            "Failed to create virtual environment: " +
                            std::string(lithium::venv::venvErrorToString(result.error())));
                    }

                    nlohmann::json data = {
                        {"created", true},
                        {"path", result->path.string()},
                        {"pythonVersion", result->pythonVersion}
                    };
                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Delete a virtual environment
    void deleteEnv(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleVenvAction(
                req, body, "deleteEnv",
                [&](auto manager) -> crow::response {
                    std::string envPath = body["path"].get<std::string>();
                    auto result = manager->deleteVenv(envPath);

                    if (!result) {
                        return ResponseBuilder::internalError(
                            "Failed to delete virtual environment");
                    }

                    return ResponseBuilder::success(nlohmann::json{{"deleted", true}});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // List all virtual environments
    void listEnvs(const crow::request& req, crow::response& res) {
        res = handleVenvAction(
            req, nlohmann::json{}, "listEnvs",
            [&](auto manager) -> crow::response {
                auto envs = manager->listVenvs();
                nlohmann::json envList = nlohmann::json::array();
                for (const auto& env : envs) {
                    envList.push_back({
                        {"path", env.path.string()},
                        {"pythonVersion", env.pythonVersion},
                        {"isActive", env.isActive}
                    });
                }
                nlohmann::json data = {{"environments", envList}};
                return ResponseBuilder::success(data);
            });
    }

    // Get environment info
    void getEnvInfo(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleVenvAction(
                req, body, "getEnvInfo",
                [&](auto manager) -> crow::response {
                    std::string envPath = body["path"].get<std::string>();
                    auto envInfo = manager->getVenvInfo(envPath);

                    if (!envInfo) {
                        return ResponseBuilder::notFound("Virtual environment");
                    }

                    nlohmann::json packages = nlohmann::json::array();
                    for (const auto& pkg : envInfo->packages) {
                        packages.push_back({
                            {"name", pkg.name},
                            {"version", pkg.version}
                        });
                    }

                    nlohmann::json data = {
                        {"path", envInfo->path.string()},
                        {"pythonVersion", envInfo->pythonVersion},
                        {"isActive", envInfo->isActive},
                        {"packages", packages}
                    };
                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Activate environment
    void activateEnv(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleVenvAction(
                req, body, "activateEnv",
                [&](auto manager) -> crow::response {
                    std::string envPath = body["path"].get<std::string>();
                    auto result = manager->activate(envPath);

                    if (!result) {
                        return ResponseBuilder::internalError(
                            "Failed to activate environment");
                    }

                    return ResponseBuilder::success(nlohmann::json{{"activated", true}});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Deactivate environment
    void deactivateEnv(const crow::request& req, crow::response& res) {
        res = handleVenvAction(
            req, nlohmann::json{}, "deactivateEnv",
            [&](auto manager) -> crow::response {
                manager->deactivate();
                return ResponseBuilder::success(nlohmann::json{{"deactivated", true}});
            });
    }

    // Get current active environment
    void getCurrentEnv(const crow::request& req, crow::response& res) {
        res = handleVenvAction(
            req, nlohmann::json{}, "getCurrentEnv",
            [&](auto manager) -> crow::response {
                auto current = manager->getActiveVenv();
                if (!current) {
                    nlohmann::json data = {
                        {"active", false},
                        {"path", ""}
                    };
                    return ResponseBuilder::success(data);
                }

                nlohmann::json data = {
                    {"active", true},
                    {"path", current->path.string()},
                    {"pythonVersion", current->pythonVersion}
                };
                return ResponseBuilder::success(data);
            });
    }

    // Install package
    void installPackage(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleVenvAction(
                req, body, "installPackage",
                [&](auto manager) -> crow::response {
                    std::string packageName = body["package"].get<std::string>();
                    std::string envPath = body.value("envPath", "");
                    std::string version = body.value("version", "");

                    std::string fullPackage = packageName;
                    if (!version.empty()) {
                        fullPackage += "==" + version;
                    }

                    auto result = manager->installPackage(envPath, fullPackage);

                    if (!result) {
                        return ResponseBuilder::internalError(
                            "Failed to install package: " +
                            std::string(lithium::venv::venvErrorToString(result.error())));
                    }

                    return ResponseBuilder::success(nlohmann::json{{"installed", true}});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Uninstall package
    void uninstallPackage(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleVenvAction(
                req, body, "uninstallPackage",
                [&](auto manager) -> crow::response {
                    std::string packageName = body["package"].get<std::string>();
                    std::string envPath = body.value("envPath", "");

                    auto result = manager->uninstallPackage(envPath, packageName);

                    if (!result) {
                        return ResponseBuilder::internalError(
                            "Failed to uninstall package");
                    }

                    return ResponseBuilder::success(nlohmann::json{{"uninstalled", true}});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // List installed packages
    void listPackages(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleVenvAction(
                req, body, "listPackages",
                [&](auto manager) -> crow::response {
                    std::string envPath = body["path"].get<std::string>();
                    auto packages = manager->listPackages(envPath);

                    if (!packages) {
                        return ResponseBuilder::internalError("Failed to list packages");
                    }

                    nlohmann::json packageList = nlohmann::json::array();
                    for (const auto& pkg : *packages) {
                        packageList.push_back({
                            {"name", pkg.name},
                            {"version", pkg.version}
                        });
                    }

                    nlohmann::json data = {{"packages", packageList}};
                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Install from requirements.txt
    void installRequirements(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleVenvAction(
                req, body, "installRequirements",
                [&](auto manager) -> crow::response {
                    std::string requirementsPath = body["requirements"].get<std::string>();
                    std::string envPath = body.value("envPath", "");

                    auto result = manager->installRequirements(envPath, requirementsPath);

                    if (!result) {
                        return ResponseBuilder::internalError(
                            "Failed to install requirements");
                    }

                    return ResponseBuilder::success(nlohmann::json{{"installed", true}});
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }

    // Discover virtual environments
    void discoverEnvs(const crow::request& req, crow::response& res) {
        try {
            auto body = nlohmann::json::parse(req.body);
            res = handleVenvAction(
                req, body, "discoverEnvs",
                [&](auto manager) -> crow::response {
                    std::string searchPath = body.value("path", ".");
                    bool recursive = body.value("recursive", true);

                    auto result = manager->discoverVenvs(searchPath, recursive);

                    if (!result) {
                        return ResponseBuilder::internalError(
                            "Failed to discover environments");
                    }

                    nlohmann::json data = {{"discovered", *result}};
                    return ResponseBuilder::success(data);
                });
        } catch (const nlohmann::json::exception& e) {
            res = ResponseBuilder::invalidJson(e.what());
        }
    }
};

inline std::weak_ptr<lithium::venv::VenvManager> VenvController::mManager;

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_VENV_HPP
