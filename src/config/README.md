# Lithium Config

High-performance configuration management library with split architecture.

## Directory Structure

```text
src/config/
├── config.hpp              # Main aggregated header (entry point)
│
├── core/                   # Core manager submodule
│   ├── exception.hpp       # Configuration exception types
│   ├── manager.hpp/cpp     # ConfigManager implementation
│   └── types.hpp           # Aggregated header
│
├── components/             # Component submodule
│   ├── cache.hpp/cpp       # High-performance LRU cache with TTL
│   ├── validator.hpp/cpp   # JSON Schema validation engine
│   ├── serializer.hpp/cpp  # Multi-format serialization (JSON/JSON5/Binary)
│   ├── watcher.hpp/cpp     # File monitoring and auto-reload
│   └── components.hpp      # Aggregated header
│
├── utils/                  # Utilities submodule
│   ├── json5.hpp           # JSON5 parser utility
│   ├── macro.hpp           # Helper macros for type-safe access
│   └── utils.hpp           # Aggregated header
│
├── CMakeLists.txt
└── README.md
```

## Core Module

The `core/` submodule provides the main configuration manager:

### Usage

```cpp
#include "config/core/types.hpp"

using namespace lithium::config;

// Create ConfigManager
auto config = ConfigManager::createShared();

// Load configuration from file
config->loadFromFile("config.json");

// Get values
auto value = config->get("database/host");
auto port = config->get_as<int>("database/port");

// Set values
config->set("database/host", "localhost");
config->set_value("database/port", 5432);

// Save configuration
config->save("config.json");
```

### Module Components

| File | Description |
|------|-------------|
| `exception.hpp` | `BadConfigException`, `InvalidConfigException`, `ConfigNotFoundException`, `ConfigIOException` |
| `manager.hpp` | `ConfigManager` - main configuration manager with all features |
| `types.hpp` | Aggregated header with all core types |

## Components Module

The `components/` submodule provides specialized components:

### Cache (`components/cache.hpp`)

```cpp
#include "config/components/cache.hpp"

using namespace lithium::config;

ConfigCache::Config config;
config.maxSize = 1000;
config.defaultTtl = std::chrono::seconds(30);

ConfigCache cache(config);

// Store and retrieve values
cache.put("key", json_value);
auto value = cache.get("key");

// Get statistics
auto stats = cache.getStatistics();
double hitRatio = stats.getHitRatio();
```

### Validator (`components/validator.hpp`)

```cpp
#include "config/components/validator.hpp"

using namespace lithium::config;

ConfigValidator validator;

// Load JSON schema
validator.loadSchema("schema.json");

// Validate data
auto result = validator.validate(json_data);
if (!result.isValid) {
    std::cerr << result.getErrorMessage();
}

// Add custom validation rules
validator.addRule("custom_rule", [](const json& data, std::string_view path) {
    ValidationResult result;
    // Custom validation logic
    return result;
});
```

### Serializer (`components/serializer.hpp`)

```cpp
#include "config/components/serializer.hpp"

using namespace lithium::config;

ConfigSerializer serializer;

// Serialize to different formats
auto result = serializer.serialize(data, SerializationOptions::pretty());
auto compact = serializer.serialize(data, SerializationOptions::compact());
auto json5 = serializer.serialize(data, SerializationOptions::json5());

// Deserialize from file
auto loaded = serializer.deserializeFromFile("config.json");

// Batch processing
auto results = serializer.serializeBatch(dataList);
```

### Watcher (`components/watcher.hpp`)

```cpp
#include "config/components/watcher.hpp"

using namespace lithium::config;

ConfigWatcher::WatcherOptions options;
options.poll_interval = std::chrono::milliseconds(100);
options.debounce_delay = std::chrono::milliseconds(250);

ConfigWatcher watcher(options);

// Watch file for changes
watcher.watchFile("config.json", [](const fs::path& path, FileEvent event) {
    std::cout << "File changed: " << path << std::endl;
});

// Start watching
watcher.startWatching();

// Get statistics
auto stats = watcher.getStatistics();
```

## Utils Module

The `utils/` submodule provides helper utilities:

### JSON5 Parser (`utils/json5.hpp`)

```cpp
#include "config/utils/json5.hpp"

using namespace lithium::internal;

// Convert JSON5 to standard JSON
auto result = convertJSON5toJSON(json5_string);
if (result.has_value()) {
    std::string json = result.value();
}
```

### Helper Macros (`utils/macro.hpp`)

```cpp
#include "config/utils/macro.hpp"

// Type-safe config access
int port = GetIntConfig("database/port");
std::string host = GetStringConfig("database/host");
bool enabled = GetBoolConfig("feature/enabled");

// With error handling
GET_CONFIG_VALUE(configManager, "path", int, value);
SET_CONFIG_VALUE(configManager, "path", 42);
```

## Quick Start

For most use cases, include the main header:

```cpp
#include "config/config.hpp"

// All functionality is available under lithium::config namespace
using namespace lithium::config;

// Create and use ConfigManager
auto config = ConfigManager::createShared();
config->loadFromFile("app.json");

auto host = config->get_as<std::string>("server/host").value_or("localhost");
auto port = config->get_as<int>("server/port").value_or(8080);
```

## Building

The config library is built as part of the lithium-next project:

```bash
mkdir build && cd build
cmake ..
cmake --build . --target lithium_config
```

## Dependencies

- atom (type utilities, error handling, macros)
- spdlog (logging)
- nlohmann_json (JSON serialization)

## Features

- **High-performance caching** with LRU eviction and TTL support
- **JSON Schema validation** with custom rules
- **Multi-format serialization** (JSON, JSON5, Binary)
- **File watching** with auto-reload functionality
- **Thread-safe operations** with optimized locking
- **Performance monitoring** and metrics collection
- **Comprehensive error handling** and logging

## Thread Safety

All public operations in `ConfigManager` and components are thread-safe:
- Read operations use shared locks
- Write operations use exclusive locks
- Callbacks are invoked asynchronously
- Background saving is handled in a dedicated thread
