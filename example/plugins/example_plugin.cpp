/*
 * example_plugin.cpp - Example Server Plugin
 *
 * Copyright (C) 2023-2024 Max Qian <lightapt.com>
 *
 * This is an example plugin demonstrating how to create a dynamic
 * command and controller plugin for the Lithium server.
 */

#include "server/plugin/base_plugin.hpp"
#include "server/command.hpp"
#include "atom/log/spdlog_logger.hpp"

namespace lithium::example {

using namespace lithium::server::plugin;
using json = nlohmann::json;

/**
 * @brief Example plugin that provides both commands and HTTP routes
 */
class ExamplePlugin : public BaseFullPlugin {
public:
    ExamplePlugin()
        : BaseFullPlugin(
              PluginMetadata{
                  .name = "example_plugin",
                  .version = "1.0.0",
                  .description = "Example plugin demonstrating the plugin system",
                  .author = "Max Qian",
                  .license = "GPL3",
                  .dependencies = {},
                  .tags = {"example", "demo"}
              },
              "/api/v1/example") {}

protected:
    auto onInitialize(const json& config) -> bool override {
        LOG_INFO("ExamplePlugin initializing with config: {}", config.dump());

        // Read configuration
        greeting_ = config.value("greeting", "Hello from ExamplePlugin!");

        return true;
    }

    void onShutdown() override {
        LOG_INFO("ExamplePlugin shutting down");
    }

    void onRegisterCommands(
        std::shared_ptr<app::CommandDispatcher> dispatcher) override {
        LOG_INFO("ExamplePlugin registering commands");

        // Register a simple echo command
        dispatcher->registerCommand<json>("example.echo", [this](json& payload) {
            std::string message = payload.value("message", "");
            payload = json{
                {"status", "success"},
                {"echo", message},
                {"greeting", greeting_}
            };
        });
        addCommandId("example.echo");

        // Register a status command
        dispatcher->registerCommand<json>("example.status", [this](json& payload) {
            payload = json{
                {"status", "success"},
                {"plugin", "example_plugin"},
                {"version", "1.0.0"},
                {"healthy", true}
            };
        });
        addCommandId("example.status");

        LOG_INFO("ExamplePlugin registered {} commands", getCommandIds().size());
    }

    void onRegisterRoutes(ServerApp& app) override {
        LOG_INFO("ExamplePlugin registering routes");

        // GET /api/v1/example/hello
        CROW_ROUTE(app, "/api/v1/example/hello")
            .methods("GET"_method)([this](const crow::request& req) {
                json response = {
                    {"status", "success"},
                    {"message", greeting_}
                };
                return crow::response(200, response.dump());
            });
        addRoutePath("/api/v1/example/hello");

        // POST /api/v1/example/echo
        CROW_ROUTE(app, "/api/v1/example/echo")
            .methods("POST"_method)([](const crow::request& req) {
                try {
                    auto body = json::parse(req.body);
                    std::string message = body.value("message", "");

                    json response = {
                        {"status", "success"},
                        {"echo", message}
                    };
                    return crow::response(200, response.dump());
                } catch (const std::exception& e) {
                    json error = {
                        {"status", "error"},
                        {"message", e.what()}
                    };
                    return crow::response(400, error.dump());
                }
            });
        addRoutePath("/api/v1/example/echo");

        // GET /api/v1/example/info
        CROW_ROUTE(app, "/api/v1/example/info")
            .methods("GET"_method)([this](const crow::request& req) {
                auto metadata = getMetadata();
                json response = {
                    {"status", "success"},
                    {"plugin", {
                        {"name", metadata.name},
                        {"version", metadata.version},
                        {"description", metadata.description},
                        {"author", metadata.author}
                    }}
                };
                return crow::response(200, response.dump());
            });
        addRoutePath("/api/v1/example/info");

        LOG_INFO("ExamplePlugin registered {} routes", getRoutePaths().size());
    }

private:
    std::string greeting_;
};

// Define plugin entry points
LITHIUM_DEFINE_PLUGIN(ExamplePlugin)

}  // namespace lithium::example
