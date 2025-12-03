// language: cpp

#ifndef LITHIUM_SERVER_CONTROLLER_MODULE_CONTROLLER_HPP
#define LITHIUM_SERVER_CONTROLLER_MODULE_CONTROLLER_HPP

#include "../utils/response.hpp"
#include "../controller.hpp"

#include <functional>
#include <memory>
#include <string>

#include "atom/function/global_ptr.hpp"
#include "atom/log/spdlog_logger.hpp"
#include "components/loader.hpp"
#include "constant/constant.hpp"

namespace lithium::server::controller {

using ResponseBuilder = utils::ResponseBuilder;

class ModuleController : public Controller {
private:
    static std::weak_ptr<lithium::ModuleLoader> mModuleLoader;

    // Utility function to handle all module actions - moved to top with
    // explicit return type
    static crow::response handleModuleAction(
        const crow::request& req, const nlohmann::json& body,
        const std::string& command,
        std::function<bool(std::shared_ptr<lithium::ModuleLoader>)> func) {
        LOG_INFO("Handling module action: {}", command);

        try {
            auto moduleLoader = mModuleLoader.lock();
            if (!moduleLoader) {
                LOG_ERROR("ModuleLoader is not available.");
                return ResponseBuilder::internalError(
                    "ModuleLoader is not available.");
            }

            // Fix: Unwrap ModuleResult<bool> returned by ModuleLoader
            // methods
            bool success = func(moduleLoader);
            if (success) {
                LOG_INFO("Module action '{}' succeeded.", command);
                nlohmann::json responseData = {};
                LOG_INFO("Finished handling module action: {}", command);
                return ResponseBuilder::success(responseData);
            } else {
                LOG_WARN("Module action '{}' failed.", command);
                LOG_INFO("Finished handling module action: {}", command);
                return ResponseBuilder::badRequest("Operation failed.");
            }
        } catch (const std::invalid_argument& e) {
            LOG_ERROR("Invalid argument exception: {}", e.what());
            return ResponseBuilder::badRequest(e.what());
        } catch (const std::runtime_error& e) {
            LOG_ERROR("Runtime error exception: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        } catch (const std::exception& e) {
            LOG_ERROR("Exception: {}", e.what());
            return ResponseBuilder::internalError(e.what());
        }
    }

public:
    void registerRoutes(lithium::server::ServerApp& app) override {
        LOG_INFO("Registering module controller routes.");

        // Create a weak pointer to the ModuleLoader
        GET_OR_CREATE_WEAK_PTR(mModuleLoader, lithium::ModuleLoader,
                               Constants::MODULE_LOADER);

        // Define the routes using lambdas instead of member function pointers
        CROW_ROUTE(app, "/module/load")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->loadModule(req, res);
                });
        CROW_ROUTE(app, "/module/unload")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->unloadModule(req, res);
                });
        CROW_ROUTE(app, "/module/unloadAll")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->unloadAllModules(req, res);
                });
        CROW_ROUTE(app, "/module/has")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->hasModule(req, res);
                });
        CROW_ROUTE(app, "/module/get")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->getModule(req, res);
                });
        CROW_ROUTE(app, "/module/enable")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->enableModule(req, res);
                });
        CROW_ROUTE(app, "/module/disable")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->disableModule(req, res);
                });
        CROW_ROUTE(app, "/module/isEnabled")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->isModuleEnabled(req, res);
                });
        CROW_ROUTE(app, "/module/list")
            .methods("GET"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->getAllModules(req, res);
                });
        CROW_ROUTE(app, "/module/hasFunction")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->hasFunction(req, res);
                });
        CROW_ROUTE(app, "/module/reload")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->reloadModule(req, res);
                });
        CROW_ROUTE(app, "/module/status")
            .methods("POST"_method)(
                [this](const crow::request& req, crow::response& res) {
                    this->getModuleStatus(req, res);
                });

        LOG_INFO("Module controller routes registered.");
    }

    // Endpoint to load a module
    void loadModule(const crow::request& req, crow::response& res) {
        LOG_INFO("Received request to load module.");
        auto body = nlohmann::json::parse(req.body);
        std::string path = body["path"].get<std::string>();
        std::string name = body["name"].get<std::string>();
        if (path.empty() || name.empty()) {
            res = ResponseBuilder::missingField(path.empty() ? "path" : "name");
            return;
        }
        LOG_INFO("Loading module: Name='{}', Path='{}'", name, path);

        res = handleModuleAction(
            req, body, "loadModule",
            [path, name](std::shared_ptr<lithium::ModuleLoader> moduleLoader) {
                auto result = moduleLoader->loadModule(path, name);
                return result && result.value();
            });
    }

    // Endpoint to unload a module
    void unloadModule(const crow::request& req, crow::response& res) {
        LOG_INFO("Received request to unload module.");
        auto body = nlohmann::json::parse(req.body);
        std::string name = body["name"].get<std::string>();
        if (name.empty()) {
            res = ResponseBuilder::missingField("name");
            return;
        }
        LOG_INFO("Unloading module: Name='{}'", name);

        res = handleModuleAction(
            req, body, "unloadModule",
            [name](std::shared_ptr<lithium::ModuleLoader> moduleLoader) {
                auto result = moduleLoader->unloadModule(name);
                return result && result.value();
            });
    }

    // Endpoint to unload all modules
    void unloadAllModules(const crow::request& req, crow::response& res) {
        LOG_INFO("Received request to unload all modules.");
        auto body = nlohmann::json::parse(req.body);

        res = handleModuleAction(
            req, body, "unloadAllModules",
            [](std::shared_ptr<lithium::ModuleLoader> moduleLoader) {
                auto result = moduleLoader->unloadAllModules();
                return result && result.value();
            });
    }

    // Endpoint to check if a module exists
    void hasModule(const crow::request& req, crow::response& res) {
        LOG_INFO("Received request to check if module exists.");
        auto body = nlohmann::json::parse(req.body);
        std::string name = body["name"].get<std::string>();
        if (name.empty()) {
            res = ResponseBuilder::missingField("name");
            return;
        }
        LOG_INFO("Checking existence of module: Name='{}'", name);

        res = handleModuleAction(
            req, body, "hasModule",
            [name](std::shared_ptr<lithium::ModuleLoader> moduleLoader)
                -> bool { return moduleLoader->hasModule(name); });
    }

    // Endpoint to get module information
    void getModule(const crow::request& req, crow::response& res) {
        LOG_INFO("Received request to get module information.");
        auto body = nlohmann::json::parse(req.body);
        std::string name = body["name"].get<std::string>();
        if (name.empty()) {
            res = ResponseBuilder::missingField("name");
            return;
        }
        LOG_INFO("Getting information for module: Name='{}'", name);

        // For getModule, we need special handling to return module details
        auto moduleLoader = mModuleLoader.lock();
        if (!moduleLoader) {
            LOG_ERROR("ModuleLoader is not available.");
            res = ResponseBuilder::internalError(
                "ModuleLoader is not available.");
            return;
        }

        auto module = moduleLoader->getModule(name);
        if (module) {
            LOG_INFO("Module found: Name='{}', Enabled={}, Status={}", name,
                     module->enabled.load(),
                     static_cast<int>(module->currentStatus));
            nlohmann::json moduleData = {
                {"name", name},
                {"enabled", module->enabled.load()},
                {"status", static_cast<int>(module->currentStatus)}};
            res = ResponseBuilder::success(moduleData);
        } else {
            LOG_WARN("Module not found: Name='{}'", name);
            res = ResponseBuilder::notFound("Module " + name);
        }
    }

    // Endpoint to enable a module
    void enableModule(const crow::request& req, crow::response& res) {
        LOG_INFO("Received request to enable module.");
        auto body = nlohmann::json::parse(req.body);
        std::string name = body["name"].get<std::string>();
        if (name.empty()) {
            res = ResponseBuilder::missingField("name");
            return;
        }
        LOG_INFO("Enabling module: Name='{}'", name);

        res = handleModuleAction(
            req, body, "enableModule",
            [name](std::shared_ptr<lithium::ModuleLoader> moduleLoader) {
                auto result = moduleLoader->enableModule(name);
                return result && result.value();
            });
    }

    // Endpoint to disable a module
    void disableModule(const crow::request& req, crow::response& res) {
        LOG_INFO("Received request to disable module.");
        auto body = nlohmann::json::parse(req.body);
        std::string name = body["name"].get<std::string>();
        if (name.empty()) {
            res = ResponseBuilder::missingField("name");
            return;
        }
        LOG_INFO("Disabling module: Name='{}'", name);

        res = handleModuleAction(
            req, body, "disableModule",
            [name](std::shared_ptr<lithium::ModuleLoader> moduleLoader) {
                auto result = moduleLoader->disableModule(name);
                return result && result.value();
            });
    }

    // Endpoint to check if a module is enabled
    void isModuleEnabled(const crow::request& req, crow::response& res) {
        LOG_INFO("Received request to check if module is enabled.");
        auto body = nlohmann::json::parse(req.body);
        std::string name = body["name"].get<std::string>();
        if (name.empty()) {
            res = ResponseBuilder::missingField("name");
            return;
        }
        LOG_INFO("Checking if module is enabled: Name='{}'", name);

        // Special handling for isModuleEnabled to return the enabled status
        auto moduleLoader = mModuleLoader.lock();
        if (!moduleLoader) {
            LOG_ERROR("ModuleLoader is not available.");
            res = ResponseBuilder::internalError(
                "ModuleLoader is not available.");
            return;
        }

        bool enabled = moduleLoader->isModuleEnabled(name);
        LOG_INFO("Module '{}' enabled status: {}", name, enabled);
        nlohmann::json responseData = {{"enabled", enabled}};
        res = ResponseBuilder::success(responseData);
    }

    // Endpoint to list all modules
    void getAllModules(const crow::request& req, crow::response& res) {
        LOG_INFO("Received request to list all modules.");
        auto moduleLoader = mModuleLoader.lock();
        if (!moduleLoader) {
            LOG_ERROR("ModuleLoader is not available for listing modules.");
            res = ResponseBuilder::internalError(
                "ModuleLoader is not available.");
            return;
        }

        auto modules = moduleLoader->getAllExistedModules();
        LOG_INFO("Listing all modules. Count: {}", modules.size());
        nlohmann::json responseData = {{"modules", modules}};
        res = ResponseBuilder::success(responseData);
    }

    // Endpoint to check if a module has a specific function
    void hasFunction(const crow::request& req, crow::response& res) {
        LOG_INFO("Received request to check if module has a function.");
        auto body = nlohmann::json::parse(req.body);
        std::string name = body["name"].get<std::string>();
        std::string functionName = body["functionName"].get<std::string>();
        if (name.empty() || functionName.empty()) {
            res = ResponseBuilder::missingField(name.empty() ? "name"
                                                             : "functionName");
            return;
        }
        LOG_INFO("Checking if module '{}' has function '{}'", name,
                 functionName);

        // Special handling for hasFunction to return the function existence
        // status
        auto moduleLoader = mModuleLoader.lock();
        if (!moduleLoader) {
            LOG_ERROR("ModuleLoader is not available.");
            res = ResponseBuilder::internalError(
                "ModuleLoader is not available.");
            return;
        }

        bool hasFunc = moduleLoader->hasFunction(name, functionName);
        LOG_INFO("Module '{}' has function '{}': {}", name, functionName,
                 hasFunc);
        nlohmann::json responseData = {{"has_function", hasFunc}};
        res = ResponseBuilder::success(responseData);
    }

    // Endpoint to reload a module
    void reloadModule(const crow::request& req, crow::response& res) {
        LOG_INFO("Received request to reload module.");
        auto body = nlohmann::json::parse(req.body);
        std::string name = body["name"].get<std::string>();
        if (name.empty()) {
            res = ResponseBuilder::missingField("name");
            return;
        }
        LOG_INFO("Reloading module: Name='{}'", name);

        res = handleModuleAction(
            req, body, "reloadModule",
            [name](std::shared_ptr<lithium::ModuleLoader> moduleLoader) {
                auto result = moduleLoader->reloadModule(name);
                return result && result.value();
            });
    }

    // Endpoint to get module status
    void getModuleStatus(const crow::request& req, crow::response& res) {
        LOG_INFO("Received request to get module status.");
        auto body = nlohmann::json::parse(req.body);
        std::string name = body["name"].get<std::string>();
        if (name.empty()) {
            res = ResponseBuilder::missingField("name");
            return;
        }
        LOG_INFO("Getting status for module: Name='{}'", name);

        // Special handling for getModuleStatus to return the status
        auto moduleLoader = mModuleLoader.lock();
        if (!moduleLoader) {
            LOG_ERROR("ModuleLoader is not available.");
            res = ResponseBuilder::internalError(
                "ModuleLoader is not available.");
            return;
        }

        auto status = moduleLoader->getModuleStatus(name);
        LOG_INFO("Module '{}' status: {}", name, static_cast<int>(status));
        nlohmann::json responseData = {{"status", static_cast<int>(status)}};
        res = ResponseBuilder::success(responseData);
    }
};

inline std::weak_ptr<lithium::ModuleLoader> ModuleController::mModuleLoader;

}  // namespace lithium::server::controller

#endif  // LITHIUM_SERVER_CONTROLLER_MODULE_CONTROLLER_HPP
