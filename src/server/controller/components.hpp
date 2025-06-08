// language: cpp

#ifndef LITHIUM_SERVER_CONTROLLER_MODULE_CONTROLLER_HPP
#define LITHIUM_SERVER_CONTROLLER_MODULE_CONTROLLER_HPP

#include "controller.hpp"

#include <functional>
#include <memory>
#include <string>

#include "atom/function/global_ptr.hpp"
#include "atom/log/loguru.hpp"
#include "components/loader.hpp"
#include "constant/constant.hpp"

class ModuleController : public Controller {
private:
    static std::weak_ptr<lithium::ModuleLoader> mModuleLoader;

    // Utility function to handle all module actions - moved to top with
    // explicit return type
    static crow::response handleModuleAction(
        const crow::request& req, const crow::json::rvalue& body,
        const std::string& command,
        std::function<bool(std::shared_ptr<lithium::ModuleLoader>)> func) {
        LOG_F(INFO, "Handling module action: {}", command);
        crow::json::wvalue res;
        res["command"] = command;

        try {
            auto moduleLoader = mModuleLoader.lock();
            if (!moduleLoader) {
                LOG_F(ERROR, "ModuleLoader is not available.");
                res["status"] = "error";
                res["message"] = "ModuleLoader is not available.";
            } else {
                // Fix: Unwrap ModuleResult<bool> returned by ModuleLoader
                // methods
                bool success = func(moduleLoader);
                if (success) {
                    LOG_F(INFO, "Module action '{}' succeeded.", command);
                    res["status"] = "success";
                    res["message"] = "Operation completed successfully.";
                } else {
                    LOG_F(WARNING, "Module action '{}' failed.", command);
                    res["status"] = "failure";
                    res["message"] = "Operation failed.";
                }
            }
        } catch (const std::invalid_argument& e) {
            LOG_F(ERROR, "Invalid argument exception: {}", e.what());
            res["status"] = "error";
            res["message"] = e.what();
        } catch (const std::runtime_error& e) {
            LOG_F(ERROR, "Runtime error exception: {}", e.what());
            res["status"] = "error";
            res["message"] = e.what();
        } catch (const std::exception& e) {
            LOG_F(ERROR, "Exception: {}", e.what());
            res["status"] = "error";
            res["message"] = e.what();
        }

        LOG_F(INFO, "Finished handling module action: {}", command);
        return crow::response(200, res);
    }

public:
    void registerRoutes(crow::SimpleApp& app) override {
        LOG_F(INFO, "Registering module controller routes.");

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

        LOG_F(INFO, "Module controller routes registered.");
    }

    // Endpoint to load a module
    void loadModule(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "Received request to load module.");
        auto body = crow::json::load(req.body);
        if (!body) {
            LOG_F(ERROR, "Invalid JSON body for loadModule.");
            res = crow::response(400, "Invalid JSON");
            return;
        }
        std::string path = body["path"].s();
        std::string name = body["name"].s();
        LOG_F(INFO, "Loading module: Name='{}', Path='{}'", name, path);

        res = handleModuleAction(
            req, body, "loadModule",
            [path, name](std::shared_ptr<lithium::ModuleLoader> moduleLoader) {
                auto result = moduleLoader->loadModule(path, name);
                return result && result.value();
            });
    }

    // Endpoint to unload a module
    void unloadModule(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "Received request to unload module.");
        auto body = crow::json::load(req.body);
        if (!body) {
            LOG_F(ERROR, "Invalid JSON body for unloadModule.");
            res = crow::response(400, "Invalid JSON");
            return;
        }
        std::string name = body["name"].s();
        LOG_F(INFO, "Unloading module: Name='{}'", name);

        res = handleModuleAction(
            req, body, "unloadModule",
            [name](std::shared_ptr<lithium::ModuleLoader> moduleLoader) {
                auto result = moduleLoader->unloadModule(name);
                return result && result.value();
            });
    }

    // Endpoint to unload all modules
    void unloadAllModules(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "Received request to unload all modules.");
        auto body = crow::json::load(req.body);
        if (!body) {
            LOG_F(ERROR, "Invalid JSON body for unloadAllModules.");
            res = crow::response(400, "Invalid JSON");
            return;
        }

        res = handleModuleAction(
            req, body, "unloadAllModules",
            [](std::shared_ptr<lithium::ModuleLoader> moduleLoader) {
                auto result = moduleLoader->unloadAllModules();
                return result && result.value();
            });
    }

    // Endpoint to check if a module exists
    void hasModule(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "Received request to check if module exists.");
        auto body = crow::json::load(req.body);
        if (!body) {
            LOG_F(ERROR, "Invalid JSON body for hasModule.");
            res = crow::response(400, "Invalid JSON");
            return;
        }
        std::string name = body["name"].s();
        LOG_F(INFO, "Checking existence of module: Name='{}'", name);

        res = handleModuleAction(
            req, body, "hasModule",
            [name](std::shared_ptr<lithium::ModuleLoader> moduleLoader)
                -> bool { return moduleLoader->hasModule(name); });
    }

    // Endpoint to get module information
    void getModule(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "Received request to get module information.");
        auto body = crow::json::load(req.body);
        if (!body) {
            LOG_F(ERROR, "Invalid JSON body for getModule.");
            res = crow::response(400, "Invalid JSON");
            return;
        }
        std::string name = body["name"].s();
        LOG_F(INFO, "Getting information for module: Name='{}'", name);

        // For getModule, we need special handling to return module details
        auto moduleLoader = mModuleLoader.lock();
        if (!moduleLoader) {
            LOG_F(ERROR, "ModuleLoader is not available.");
            crow::json::wvalue resJson;
            resJson["command"] = "getModule";
            resJson["status"] = "error";
            resJson["message"] = "ModuleLoader is not available.";
            res = crow::response(500, resJson);
            return;
        }

        auto module = moduleLoader->getModule(name);
        if (module) {
            LOG_F(INFO, "Module found: Name='{}', Enabled={}, Status={}", name,
                  module->enabled.load(),
                  static_cast<int>(module->currentStatus));
            crow::json::wvalue jsonModule;
            jsonModule["command"] = "getModule";
            jsonModule["status"] = "success";
            jsonModule["name"] = name;
            jsonModule["enabled"] = module->enabled.load();
            jsonModule["moduleStatus"] =
                static_cast<int>(module->currentStatus);
            res = crow::response(200, jsonModule);
        } else {
            LOG_F(WARNING, "Module not found: Name='{}'", name);
            crow::json::wvalue resJson;
            resJson["command"] = "getModule";
            resJson["status"] = "failure";
            resJson["message"] = "Module not found.";
            res = crow::response(404, resJson);
        }
    }

    // Endpoint to enable a module
    void enableModule(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "Received request to enable module.");
        auto body = crow::json::load(req.body);
        if (!body) {
            LOG_F(ERROR, "Invalid JSON body for enableModule.");
            res = crow::response(400, "Invalid JSON");
            return;
        }
        std::string name = body["name"].s();
        LOG_F(INFO, "Enabling module: Name='{}'", name);

        res = handleModuleAction(
            req, body, "enableModule",
            [name](std::shared_ptr<lithium::ModuleLoader> moduleLoader) {
                auto result = moduleLoader->enableModule(name);
                return result && result.value();
            });
    }

    // Endpoint to disable a module
    void disableModule(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "Received request to disable module.");
        auto body = crow::json::load(req.body);
        if (!body) {
            LOG_F(ERROR, "Invalid JSON body for disableModule.");
            res = crow::response(400, "Invalid JSON");
            return;
        }
        std::string name = body["name"].s();
        LOG_F(INFO, "Disabling module: Name='{}'", name);

        res = handleModuleAction(
            req, body, "disableModule",
            [name](std::shared_ptr<lithium::ModuleLoader> moduleLoader) {
                auto result = moduleLoader->disableModule(name);
                return result && result.value();
            });
    }

    // Endpoint to check if a module is enabled
    void isModuleEnabled(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "Received request to check if module is enabled.");
        auto body = crow::json::load(req.body);
        if (!body) {
            LOG_F(ERROR, "Invalid JSON body for isModuleEnabled.");
            res = crow::response(400, "Invalid JSON");
            return;
        }
        std::string name = body["name"].s();
        LOG_F(INFO, "Checking if module is enabled: Name='{}'", name);

        // Special handling for isModuleEnabled to return the enabled status
        auto moduleLoader = mModuleLoader.lock();
        if (!moduleLoader) {
            LOG_F(ERROR, "ModuleLoader is not available.");
            crow::json::wvalue resJson;
            resJson["command"] = "isModuleEnabled";
            resJson["status"] = "error";
            resJson["message"] = "ModuleLoader is not available.";
            res = crow::response(500, resJson);
            return;
        }

        bool enabled = moduleLoader->isModuleEnabled(name);
        LOG_F(INFO, "Module '{}' enabled status: {}", name, enabled);
        crow::json::wvalue resJson;
        resJson["command"] = "isModuleEnabled";
        resJson["status"] = "success";
        resJson["moduleEnabled"] = enabled;
        res = crow::response(200, resJson);
    }

    // Endpoint to list all modules
    void getAllModules(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "Received request to list all modules.");
        auto moduleLoader = mModuleLoader.lock();
        if (!moduleLoader) {
            LOG_F(ERROR, "ModuleLoader is not available for listing modules.");
            crow::json::wvalue resJson;
            resJson["status"] = "error";
            resJson["message"] = "ModuleLoader is not available.";
            res = crow::response(500, resJson);
            return;
        }

        auto modules = moduleLoader->getAllExistedModules();
        LOG_F(INFO, "Listing all modules. Count: {}", modules.size());
        crow::json::wvalue resJson;
        resJson["status"] = "success";
        resJson["modules"] = modules;
        res = crow::response(200, resJson);
    }

    // Endpoint to check if a module has a specific function
    void hasFunction(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "Received request to check if module has a function.");
        auto body = crow::json::load(req.body);
        if (!body) {
            LOG_F(ERROR, "Invalid JSON body for hasFunction.");
            res = crow::response(400, "Invalid JSON");
            return;
        }
        std::string name = body["name"].s();
        std::string functionName = body["functionName"].s();
        LOG_F(INFO, "Checking if module '{}' has function '{}'", name,
              functionName);

        // Special handling for hasFunction to return the function existence
        // status
        auto moduleLoader = mModuleLoader.lock();
        if (!moduleLoader) {
            LOG_F(ERROR, "ModuleLoader is not available.");
            crow::json::wvalue resJson;
            resJson["command"] = "hasFunction";
            resJson["status"] = "error";
            resJson["message"] = "ModuleLoader is not available.";
            res = crow::response(500, resJson);
            return;
        }

        bool hasFunc = moduleLoader->hasFunction(name, functionName);
        LOG_F(INFO, "Module '{}' has function '{}': {}", name, functionName,
              hasFunc);
        crow::json::wvalue resJson;
        resJson["command"] = "hasFunction";
        resJson["status"] = "success";
        resJson["hasFunction"] = hasFunc;
        res = crow::response(200, resJson);
    }

    // Endpoint to reload a module
    void reloadModule(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "Received request to reload module.");
        auto body = crow::json::load(req.body);
        if (!body) {
            LOG_F(ERROR, "Invalid JSON body for reloadModule.");
            res = crow::response(400, "Invalid JSON");
            return;
        }
        std::string name = body["name"].s();
        LOG_F(INFO, "Reloading module: Name='{}'", name);

        res = handleModuleAction(
            req, body, "reloadModule",
            [name](std::shared_ptr<lithium::ModuleLoader> moduleLoader) {
                auto result = moduleLoader->reloadModule(name);
                return result && result.value();
            });
    }

    // Endpoint to get module status
    void getModuleStatus(const crow::request& req, crow::response& res) {
        LOG_F(INFO, "Received request to get module status.");
        auto body = crow::json::load(req.body);
        if (!body) {
            LOG_F(ERROR, "Invalid JSON body for getModuleStatus.");
            res = crow::response(400, "Invalid JSON");
            return;
        }
        std::string name = body["name"].s();
        LOG_F(INFO, "Getting status for module: Name='{}'", name);

        // Special handling for getModuleStatus to return the status
        auto moduleLoader = mModuleLoader.lock();
        if (!moduleLoader) {
            LOG_F(ERROR, "ModuleLoader is not available.");
            crow::json::wvalue resJson;
            resJson["command"] = "getModuleStatus";
            resJson["status"] = "error";
            resJson["message"] = "ModuleLoader is not available.";
            res = crow::response(500, resJson);
            return;
        }

        auto status = moduleLoader->getModuleStatus(name);
        LOG_F(INFO, "Module '{}' status: {}", name, static_cast<int>(status));
        crow::json::wvalue resJson;
        resJson["command"] = "getModuleStatus";
        resJson["status"] = "success";
        resJson["moduleStatus"] = static_cast<int>(status);
        res = crow::response(200, resJson);
    }
};

inline std::weak_ptr<lithium::ModuleLoader> ModuleController::mModuleLoader;

#endif  // LITHIUM_SERVER_CONTROLLER_MODULE_CONTROLLER_HPP
