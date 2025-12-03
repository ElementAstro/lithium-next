# Lithium Server Plugin System

This module provides a dynamic plugin loading system for extending the Lithium server with custom commands and HTTP controllers at runtime.

## Overview

The plugin system allows you to:

- **Load plugins dynamically** at runtime without recompiling the server
- **Register WebSocket commands** that extend the command dispatcher
- **Register HTTP routes** that extend the REST API
- **Hot-reload plugins** for development and updates
- **Manage plugin lifecycle** including enable/disable, configuration, and health monitoring

## Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                      PluginManager                          │
│  - Lifecycle management                                     │
│  - Event notifications                                      │
│  - Configuration persistence                                │
├─────────────────────────────────────────────────────────────┤
│                      PluginLoader                           │
│  - Dynamic library loading (wraps ModuleLoader)             │
│  - Plugin discovery                                         │
│  - Dependency resolution                                    │
├─────────────────────────────────────────────────────────────┤
│                    Plugin Interfaces                        │
│  ┌──────────────┐  ┌──────────────┐  ┌──────────────┐      │
│  │ ICommandPlugin│  │IControllerPlugin│ │ IFullPlugin  │      │
│  │  - Commands  │  │  - Routes    │  │ - Both      │      │
│  └──────────────┘  └──────────────┘  └──────────────┘      │
└─────────────────────────────────────────────────────────────┘
```

## Creating a Plugin

### 1. Choose Plugin Type

- **Command Plugin**: Extends WebSocket commands only
- **Controller Plugin**: Extends HTTP routes only
- **Full Plugin**: Extends both commands and routes

### 2. Implement the Plugin

```cpp
#include "server/plugin/base_plugin.hpp"

class MyPlugin : public BaseFullPlugin {
public:
    MyPlugin()
        : BaseFullPlugin(
              PluginMetadata{
                  .name = "my_plugin",
                  .version = "1.0.0",
                  .description = "My custom plugin",
                  .author = "Your Name",
                  .license = "MIT",
                  .dependencies = {},
                  .tags = {"custom"}
              },
              "/api/v1/myplugin") {}

protected:
    auto onInitialize(const json& config) -> bool override {
        // Initialize your plugin
        return true;
    }

    void onShutdown() override {
        // Cleanup
    }

    void onRegisterCommands(
        std::shared_ptr<app::CommandDispatcher> dispatcher) override {
        dispatcher->registerCommand<json>("myplugin.action", [](json& payload) {
            payload = json{{"status", "success"}};
        });
        addCommandId("myplugin.action");
    }

    void onRegisterRoutes(ServerApp& app) override {
        CROW_ROUTE(app, "/api/v1/myplugin/hello")
            .methods("GET"_method)([](const crow::request& req) {
                return crow::response(200, R"({"message": "Hello!"})");
            });
        addRoutePath("/api/v1/myplugin/hello");
    }
};

// Export plugin entry points
LITHIUM_DEFINE_PLUGIN(MyPlugin)
```

### 3. Build as Shared Library

```cmake
add_library(my_plugin SHARED my_plugin.cpp)
target_link_libraries(my_plugin PRIVATE lithium_server)
```

### 4. Deploy

Place the compiled library in `plugins/server/` directory.

## Plugin API

### REST Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/v1/plugins` | List all loaded plugins |
| GET | `/api/v1/plugins/available` | List discovered but not loaded plugins |
| GET | `/api/v1/plugins/<name>` | Get plugin details |
| POST | `/api/v1/plugins/load` | Load a plugin |
| POST | `/api/v1/plugins/unload` | Unload a plugin |
| POST | `/api/v1/plugins/reload` | Hot-reload a plugin |
| POST | `/api/v1/plugins/enable` | Enable a loaded plugin |
| POST | `/api/v1/plugins/disable` | Disable a plugin |
| GET | `/api/v1/plugins/<name>/config` | Get plugin configuration |
| PUT | `/api/v1/plugins/<name>/config` | Update plugin configuration |
| GET | `/api/v1/plugins/<name>/health` | Get plugin health status |
| GET | `/api/v1/plugins/status` | Get overall plugin system status |
| POST | `/api/v1/plugins/discover` | Discover and load all plugins |
| POST | `/api/v1/plugins/config/save` | Save plugin configuration |

### Example API Usage

```bash
# List plugins
curl http://localhost:8080/api/v1/plugins

# Load a plugin
curl -X POST http://localhost:8080/api/v1/plugins/load \
  -H "Content-Type: application/json" \
  -d '{"name": "my_plugin", "config": {"setting": "value"}}'

# Reload a plugin
curl -X POST http://localhost:8080/api/v1/plugins/reload \
  -H "Content-Type: application/json" \
  -d '{"name": "my_plugin"}'

# Get plugin health
curl http://localhost:8080/api/v1/plugins/my_plugin/health
```

## Configuration

### Server Configuration

```cpp
MainServer::Config config;
config.enable_plugins = true;
config.auto_load_plugins = true;
config.plugin_config.loaderConfig.pluginDirectory = "plugins/server";
```

### Plugin Configuration File

`config/plugins.json`:
```json
{
  "plugins": [
    {
      "name": "my_plugin",
      "path": "plugins/server/my_plugin.so",
      "config": {
        "setting": "value"
      },
      "enabled": true,
      "autoLoad": true
    }
  ]
}
```

## Plugin Lifecycle

1. **Discovery**: Scan plugin directories for shared libraries
2. **Loading**: Load the library and verify API version
3. **Initialization**: Call `initialize()` with configuration
4. **Registration**: Register commands and/or routes
5. **Running**: Plugin is active and handling requests
6. **Disabling**: Unregister commands (routes remain but inactive)
7. **Unloading**: Call `shutdown()` and unload library

## Best Practices

1. **Version your plugins** using semantic versioning
2. **Handle errors gracefully** and set meaningful error messages
3. **Implement health checks** for monitoring
4. **Document your commands and routes**
5. **Use configuration** for customizable behavior
6. **Clean up resources** in `onShutdown()`
7. **Test thoroughly** before deployment

## Files

- `plugin_interface.hpp/cpp` - Core interfaces and types
- `plugin_loader.hpp/cpp` - Dynamic library loading
- `plugin_manager.hpp/cpp` - Lifecycle management
- `base_plugin.hpp` - Base implementations for easy plugin creation
- `controller/plugin.hpp` - REST API controller

## Dependencies

- `lithium::ModuleLoader` from `src/components/loader.hpp`
- `atom::meta::DynamicLibrary` for FFI
- Crow for HTTP routing
- nlohmann/json for configuration
