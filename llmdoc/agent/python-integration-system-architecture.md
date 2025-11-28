# Python Integration System Architecture

## Overview

Lithium-Next implements comprehensive Python integration through a multi-layered system combining pybind11 bindings, a PythonWrapper class for script execution, a component/plugin system for module management, and REST API controllers for remote access.

## Architecture Layers

### Layer 1: Core Python Wrapper (PythonWrapper)

**File:** `src/script/python_caller.hpp/.cpp`

The `PythonWrapper` class manages Python script execution and provides C++ to Python interoperability using pybind11.

#### Key Capabilities

- Script loading/unloading with aliases
- Function calling with typed return values
- Variable get/set operations
- Expression evaluation
- Python code injection
- Module reloading
- Asynchronous execution
- Batch processing
- Thread pool management with GIL optimization

#### Core Methods

```cpp
void load_script(const std::string& script_name, const std::string& alias);
void unload_script(const std::string& alias);
void reload_script(const std::string& alias);

template<typename ReturnType, typename... Args>
ReturnType call_function(const std::string& alias,
                        const std::string& function_name, Args... args);

template<typename T>
T get_variable(const std::string& alias, const std::string& variable_name);

void set_variable(const std::string& alias, const std::string& variable_name,
                  const py::object& value);

std::vector<std::string> list_scripts() const;
std::vector<std::string> get_function_list(const std::string& alias);
```

#### GIL Management

Uses RAII pattern with `GILManager` class:
- Automatically acquires/releases Python Global Interpreter Lock
- Thread-safe function execution in thread pool
- Semaphore-limited concurrent task execution

### Layer 2: HTTP REST API (PythonController)

**File:** `src/server/controller/python.hpp`

Exposes PythonWrapper functionality via HTTP endpoints for remote script management and execution.

#### Endpoint Categories

1. **Basic Script Management** (`/python/load`, `/python/unload`, `/python/reload`, `/python/list`)
2. **Function/Variable Operations** (`/python/call`, `/python/getVariable`, `/python/setVariable`)
3. **Object-Oriented** (`/python/callMethod`, `/python/getObjectAttribute`, `/python/setObjectAttribute`)
4. **Code Execution** (`/python/eval`, `/python/inject`, `/python/executeWithLogging`)
5. **Performance** (`/python/configurePerformance`, `/python/getMemoryUsage`, `/python/optimizeMemory`)
6. **Package Management** (`/python/installPackage`, `/python/uninstallPackage`)
7. **Virtual Environments** (`/python/createVenv`, `/python/activateVenv`)

#### Request/Response Pattern

All endpoints use a standardized `handlePythonAction` method:

```cpp
static auto handlePythonAction(
    const crow::request& req,
    const crow::json::rvalue& body,
    const std::string& command,
    std::function<bool(std::shared_ptr<lithium::PythonWrapper>)> func)
```

### Layer 3: Script Task Integration

**Files:** `src/task/custom/script/base.hpp`, `src/task/custom/script/python.hpp`

Integrates Python execution into the task system for sequencing and automation.

#### Task Hierarchy

```
Task (base)
  └── BaseScriptTask (abstract)
      └── PythonScriptTask (concrete)
```

#### PythonScriptTask Features

- Embeds PythonWrapper instance
- Integrates with task execution pipeline
- Supports parameter validation through task API
- Caches compiled Python scripts
- History tracking for debugging

#### Task API Integration

```cpp
class PythonScriptTask : public BaseScriptTask {
    void loadPythonModule(const std::string& moduleName,
                         const std::string& alias = "");

    template<typename ReturnType, typename... Args>
    ReturnType callPythonFunction(const std::string& alias,
                                 const std::string& functionName,
                                 Args... args);
};
```

### Layer 4: Component System (for Native Modules)

**Files:** `src/components/manager/manager.hpp`, `src/components/loader.hpp`

Manages plugin/component loading via dynamic library mechanism. While distinct from Python integration, it provides parallel capability for compiled modules.

#### Component Manager Features

- Dynamic module loading from `./modules` directory
- Dependency graph management
- Component lifecycle (load, unload, start, stop, pause, resume)
- Event system with component state notifications
- Batch loading/unloading
- Performance monitoring

### Layer 5: Global Pointer Injection

**Mechanism:** `atom/function/global_ptr.hpp`

The system uses a global pointer injection pattern to expose the PythonWrapper:

```cpp
// In app.cpp
AddPtr<lithium::PythonWrapper>(
    Constants::PYTHON_WRAPPER,
    atom::memory::makeShared<lithium::PythonWrapper>());

// In controllers
GET_OR_CREATE_WEAK_PTR(mPythonWrapper, lithium::PythonWrapper,
                       Constants::PYTHON_WRAPPER);
```

This allows any controller to access the same PythonWrapper instance without explicit dependency injection.

## Data Flow Diagram

```
HTTP Request
    ↓
[PythonController]
    ↓
[Global Pointer Lookup: PYTHON_WRAPPER]
    ↓
[PythonWrapper Instance]
    ├─→ Script Manager (alias → py::module mapping)
    ├─→ Function Cache (avoid re-lookup)
    └─→ Python Interpreter (py::scoped_interpreter)
         ↓
    Python Code Execution
         ↓
    pybind11 Type Conversion
         ↓
HTTP Response (JSON)
```

## Performance Features

1. **Function Caching**: Caches function call results with 1-hour TTL
2. **Thread Pooling**: Configurable thread pool for concurrent execution
3. **GIL Optimization**: Smart GIL acquisition/release in worker threads
4. **Memory Management**: Pool-based allocation for reducing fragmentation
5. **Semaphore Limiting**: Bounds concurrent task execution to prevent thrashing

## Error Handling Strategies

```cpp
enum class ErrorHandlingStrategy {
    THROW_EXCEPTION,   // Propagate exceptions
    RETURN_DEFAULT,    // Return default values silently
    LOG_AND_CONTINUE   // Log errors but continue execution
};
```

## Type Conversion

pybind11 provides automatic conversion between C++ and Python:

```cpp
// C++ → Python
py::object cpp_val = py::cast(c_value);

// Python → C++
T cpp_val = py::cast<T>(py_object);
```

Helper methods for JSON conversion:

```cpp
static py::object jsonToPy(const crow::json::rvalue& val);
```

## Summary

The architecture provides three integration points:
1. **Direct PythonWrapper API** for C++ code
2. **HTTP REST API** for remote/web-based access
3. **Task System Integration** for automation workflows

All layers share a common PythonWrapper instance via global pointer injection, ensuring consistent state and single Python interpreter lifetime.
