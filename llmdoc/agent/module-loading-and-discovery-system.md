# Module Loading and Discovery System

## Overview

Lithium-Next provides two parallel module loading systems: one for Python modules (via PythonWrapper) and one for native C++ modules (via ModuleLoader). Both systems support discovery, dependency management, and lifecycle control.

## Python Module Loading (PythonWrapper)

### Script Loading Workflow

**Code Section: Script Loading Mechanism**

**File:** `src/script/python_caller.cpp`
**Lines:** 53-64

```cpp
void PythonWrapper::Impl::load_script(const std::string& script_name,
                                     const std::string& alias) {
    spdlog::info("Loading script '{}' with alias '{}'", script_name, alias);
    try {
        scripts_.emplace(alias, py::module::import(script_name.c_str()));
        spdlog::info("Script '{}' loaded successfully", script_name);
    } catch (const py::error_already_set& e) {
        spdlog::error("Error loading script '{}': {}", script_name, e.what());
        throw std::runtime_error("Failed to import script '" + script_name +
                                 "': " + e.what());
    }
}
```

**Key Details:**

- Uses pybind11's `py::module::import()` to load Python modules
- Maps module to an alias for later reference in function calls
- Stores loaded modules in `std::unordered_map<std::string, py::module> scripts_`
- Logs all operations via spdlog for debugging
- Propagates pybind11 exceptions as std::runtime_error

### Script Discovery

Available loaded scripts can be listed:

```cpp
std::vector<std::string> PythonWrapper::list_scripts() const;
```

Returns all registered script aliases. Discovery is implicit - tools must first be loaded via `load_script()` before they appear in the list.

### Function Discovery

Once a script is loaded, available functions can be queried:

**Code Section: Function List Extraction**

**File:** `src/script/python_caller.hpp`
**Lines:** 113-113

```cpp
std::vector<std::string> get_function_list(const std::string& alias);
```

This uses Python introspection to extract callable functions from a loaded module. Implementation filters out:
- Private functions (starting with `_`)
- Built-in functions
- Non-callable attributes

### Script Loading via REST API

**Code Section: HTTP Load Endpoint**

**File:** `src/server/controller/python.hpp`
**Lines:** 132-143

```cpp
CROW_ROUTE(app, "/python/load")
    .methods("POST"_method)([this](const crow::request& req, crow::response& res) {
        this->loadScript(req, res);
    });

void loadScript(const crow::request& req, crow::response& res) {
    auto body = crow::json::load(req.body);
    res = handlePythonAction(
        req, body, "loadScript", [&](auto pythonWrapper) {
            pythonWrapper->load_script(std::string(body["script_name"].s()),
                                      std::string(body["alias"].s()));
            return true;
        });
}
```

**Request Format:**

```json
{
    "script_name": "my_astronomy_tool",
    "alias": "astro"
}
```

**Response Format:**

```json
{
    "status": "success",
    "code": 200,
    "command": "loadScript"
}
```

### Script Reload Mechanism

Scripts can be reloaded without unloading:

```cpp
void PythonWrapper::Impl::reload_script(const std::string& alias) {
    auto iter = scripts_.find(alias);
    if (iter == scripts_.end()) {
        throw std::runtime_error("Alias not found");
    }
    py::module script = iter->second;
    py::module::import("importlib").attr("reload")(script);
}
```

Uses Python's `importlib.reload()` to refresh module code without restarting the interpreter.

## Native Module Loading (ModuleLoader)

### Module Directory Structure

**Location:** `./modules/` (configurable via `Constants::COMPONENT_PATH`)

```
modules/
├── my_module/
│   ├── my_module.so        # Compiled shared library
│   ├── module.json         # Metadata file
│   └── config.json         # Configuration file
└── another_module/
    ├── another_module.dll
    ├── module.json
    └── config.json
```

### Module Loading Process

**Code Section: Module Loading**

**File:** `src/components/loader.hpp`
**Lines:** 105-106

```cpp
auto loadModule(std::string_view path, std::string_view name)
    -> ModuleResult<bool>;
```

Returns `std::expected<bool, std::string>` indicating success or error.

**Steps:**

1. Verify module exists at path
2. Compute module hash for change detection
3. Load dynamic library via FFI
4. Extract function/symbol information via ELF parsing
5. Store in `modules_` map by name
6. Update module statistics

### Module Metadata (module.json)

**Code Section: Module Information Structure**

**File:** `src/components/module.hpp`
**Lines:** 45-102

```cpp
struct ModuleInfo {
    std::string name;              // Unique identifier
    std::string description;       // Human-readable description
    std::string version;           // Semantic version
    std::string status;            // Current status string
    std::string type;              // "native" or "python"
    std::string author;            // Module author
    std::string license;           // License type
    std::string path;              // File path to module
    std::string configPath;        // Configuration directory
    std::vector<std::string> dependencies;  // Dependency list
    std::chrono::system_clock::time_point loadTime;
    ModuleInfo::Status currentStatus;  // UNLOADED, LOADING, LOADED, ERROR
    std::string lastError;
    int priority;
    ModuleInfo::Statistics stats;  // Usage statistics
};
```

**Example module.json:**

```json
{
    "name": "camera_driver",
    "version": "1.0.0",
    "description": "Camera control module",
    "type": "native",
    "author": "Max Qian",
    "license": "AGPL-3",
    "path": "./modules/camera_driver/camera_driver.so",
    "dependencies": ["device_base"],
    "configFile": "./modules/camera_driver/config.json",
    "enabled": true,
    "priority": 10
}
```

### Dependency Resolution

**Code Section: Dependency Graph**

**File:** `src/components/loader.hpp`
**Lines:** 260-279

```cpp
auto loadModulesInOrder() -> ModuleResult<bool>;
auto getDependencies(std::string_view name) const
    -> std::vector<std::string>;
auto validateDependencies(std::string_view name) const -> bool;
```

**Dependency Validation Steps:**

1. Build directed graph of module dependencies
2. Detect circular dependencies
3. Perform topological sort
4. Load modules in dependency order
5. Validate all dependencies are satisfied before loading

### Function Access from Modules

**Code Section: Function Extraction**

**File:** `src/components/loader.hpp`
**Lines:** 172-188

```cpp
template <ModuleFunction T>
auto getFunction(std::string_view name, std::string_view functionName)
    -> ModuleResult<std::function<T>>;

template <typename T>
auto getInstance(std::string_view name, const json& config,
                 std::string_view symbolName)
    -> ModuleResult<std::shared_ptr<T>>;
```

**Function Retrieval Process:**

1. Lookup module by name in `modules_` map
2. Get dynamic library handle
3. Use FFI to resolve function symbol
4. Wrap in std::function with proper signature
5. Return via `std::expected` for error handling

### Dynamic Module Discovery

**Code Section: Module Scanning**

**File:** `src/components/manager/manager_impl.hpp`
**Lines:** 52-52

```cpp
auto scanComponents(const std::string& path) -> std::vector<std::string>;
```

Scans directory for new or modified modules by:

1. Listing all `.so`/`.dll` files in module directory
2. Computing hash of each module file
3. Comparing hash with stored value to detect changes
4. Returning list of new/modified modules

### Enable/Disable Modules

**Code Section: Module Control**

**File:** `src/components/loader.hpp`
**Lines:** 142-156

```cpp
auto enableModule(std::string_view name) -> ModuleResult<bool>;
auto disableModule(std::string_view name) -> ModuleResult<bool>;
auto isModuleEnabled(std::string_view name) const -> bool;
```

Modules can be dynamically enabled/disabled without unloading, allowing runtime configuration changes.

### Module Priority and Ordering

**Code Section: Priority-Based Loading**

**File:** `src/components/module.hpp`
**Lines:** 86-86

```cpp
int priority{0};  // The priority of the module.
```

Modules with higher priority load first. Useful for core infrastructure modules that other modules depend on.

## Component Manager Integration

### Component Lifecycle

**Code Section: Component State Management**

**File:** `src/components/manager/manager.hpp`
**Lines:** 176-205

```cpp
auto startComponent(const std::string& name) -> bool;
auto stopComponent(const std::string& name) -> bool;
auto pauseComponent(const std::string& name) -> bool;
auto resumeComponent(const std::string& name) -> bool;
auto getComponentState(const std::string& name) -> ComponentState;
```

**Component States:**

```cpp
enum class ComponentState {
    Unloaded,    // Module not loaded
    Loading,     // Module currently loading
    Loaded,      // Module loaded but not started
    Running,     // Module actively running
    Paused,      // Module execution paused
    Stopping,    // Module being shut down
    Stopped,     // Module shut down
    Error        // Error occurred
};
```

### Event System

**Code Section: Component Event Notifications**

**File:** `src/components/manager/manager.hpp`
**Lines:** 213-222

```cpp
using EventCallback = std::function<void(const std::string&,
                                        ComponentEvent, const json&)>;
void addEventListener(ComponentEvent event, EventCallback callback);
void removeEventListener(ComponentEvent event);
```

Listeners receive notifications when:

- Module loads/unloads
- State changes
- Configuration updates
- Errors occur

## Global Initialization

**Code Section: PythonWrapper Registration**

**File:** `src/app.cpp`
**Lines:** 110-112

```cpp
AddPtr<lithium::PythonWrapper>(
    Constants::PYTHON_WRAPPER,
    atom::memory::makeShared<lithium::PythonWrapper>());
```

The global PythonWrapper is initialized once at application startup and registered via the global pointer injection system using:

- Key: `Constants::PYTHON_WRAPPER` (hashed string constant)
- Value: Shared pointer to singleton PythonWrapper instance

**Lookup Pattern:**

```cpp
GET_OR_CREATE_WEAK_PTR(mPythonWrapper, lithium::PythonWrapper,
                       Constants::PYTHON_WRAPPER);
```

## Summary

**Python Module Loading:**
- Uses pybind11's `py::module::import()`
- Alias-based registration for function calling
- Introspection-based function discovery
- Reload via `importlib.reload()`
- HTTP API for remote access

**Native Module Loading:**
- File-based discovery from `./modules/` directory
- JSON metadata for module information
- Dependency graph validation and topological sort
- Priority-based ordering
- Dynamic enable/disable without unloading
- FFI-based symbol resolution

**Common Features:**
- Event notification system
- Lifecycle management
- Error tracking and reporting
- Configuration support
- Statistics collection
