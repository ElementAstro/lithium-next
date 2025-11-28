# Script Engine Integration Points and Architecture

## Evidence Section

### Code Section: Global Pointer Injection Pattern

**File:** `src/server/controller/python.hpp`
**Lines:** 32-34, 128-129
**Purpose:** Dependency injection for centralized component management

```cpp
class PythonController : public Controller {
private:
    static std::weak_ptr<lithium::PythonWrapper> mPythonWrapper;

    void registerRoutes(lithium::server::ServerApp& app) override {
        GET_OR_CREATE_WEAK_PTR(mPythonWrapper, lithium::PythonWrapper,
                               Constants::PYTHON_WRAPPER);
        // Routes registered here
    }
};

// Instance initialization
inline std::weak_ptr<lithium::PythonWrapper>
    PythonController::mPythonWrapper;
```

**Key Details:**
- Weak pointer pattern for singleton-like access
- GET_OR_CREATE_WEAK_PTR macro from atom/function/global_ptr.hpp
- Constants::PYTHON_WRAPPER key for lookup
- Static initialization via inline definition

### Code Section: Python Controller HTTP Endpoints

**File:** `src/server/controller/python.hpp`
**Lines:** 128-282
**Purpose:** REST API mapping for Python operations

```cpp
void registerRoutes(lithium::server::ServerApp& app) override {
    // Script Management (4 routes)
    CROW_ROUTE(app, "/python/load").methods("POST"_method);
    CROW_ROUTE(app, "/python/unload").methods("POST"_method);
    CROW_ROUTE(app, "/python/reload").methods("POST"_method);
    CROW_ROUTE(app, "/python/list").methods("GET"_method);

    // Function & Variable Management (6 routes)
    CROW_ROUTE(app, "/python/call").methods("POST"_method);
    CROW_ROUTE(app, "/python/callAsync").methods("POST"_method);
    CROW_ROUTE(app, "/python/batchExecute").methods("POST"_method);
    CROW_ROUTE(app, "/python/getVariable").methods("POST"_method);
    CROW_ROUTE(app, "/python/setVariable").methods("POST"_method);
    CROW_ROUTE(app, "/python/functions").methods("POST"_method);

    // Expression & Code Execution (4 routes)
    CROW_ROUTE(app, "/python/eval").methods("POST"_method);
    CROW_ROUTE(app, "/python/inject").methods("POST"_method);
    CROW_ROUTE(app, "/python/executeWithLogging").methods("POST"_method);
    CROW_ROUTE(app, "/python/executeWithProfiling").methods("POST"_method);

    // Object-Oriented Support (4 routes)
    CROW_ROUTE(app, "/python/callMethod").methods("POST"_method);
    CROW_ROUTE(app, "/python/getObjectAttribute").methods("POST"_method);
    CROW_ROUTE(app, "/python/setObjectAttribute").methods("POST"_method);
    CROW_ROUTE(app, "/python/manageObjectLifecycle").methods("POST"_method);

    // System & Environment Management (3 routes)
    CROW_ROUTE(app, "/python/addSysPath").methods("POST"_method);
    CROW_ROUTE(app, "/python/syncVariableToGlobal").methods("POST"_method);
    CROW_ROUTE(app, "/python/syncVariableFromGlobal").methods("POST"_method);

    // Performance & Memory (4 routes)
    CROW_ROUTE(app, "/python/getMemoryUsage").methods("GET"_method);
    CROW_ROUTE(app, "/python/optimizeMemory").methods("POST"_method);
    CROW_ROUTE(app, "/python/clearUnusedResources").methods("POST"_method);
    CROW_ROUTE(app, "/python/configurePerformance").methods("POST"_method);

    // Package Management (2 routes)
    CROW_ROUTE(app, "/python/installPackage").methods("POST"_method);
    CROW_ROUTE(app, "/python/uninstallPackage").methods("POST"_method);

    // Virtual Environment (2 routes)
    CROW_ROUTE(app, "/python/createVenv").methods("POST"_method);
    CROW_ROUTE(app, "/python/activateVenv").methods("POST"_method);

    // Debugging (2 routes)
    CROW_ROUTE(app, "/python/enableDebug").methods("POST"_method);
    CROW_ROUTE(app, "/python/setBreakpoint").methods("POST"_method);

    // Advanced (2 routes)
    CROW_ROUTE(app, "/python/registerFunction").methods("POST"_method);
    CROW_ROUTE(app, "/python/setErrorHandlingStrategy").methods("POST"_method);
}
```

**Key Details:**
- 35 HTTP endpoints total
- Crow framework for routing
- POST for state-changing operations, GET for queries
- Consistent error response format

### Code Section: Script Controller HTTP Endpoints

**File:** `src/server/controller/script.hpp`
**Lines:** 159-208
**Purpose:** REST API for script and analysis operations

```cpp
void registerRoutes(lithium::server::ServerApp& app) override {
    // ScriptManager routes (9 routes)
    CROW_ROUTE(app, "/script/register").methods("POST"_method);
    CROW_ROUTE(app, "/script/delete").methods("POST"_method);
    CROW_ROUTE(app, "/script/update").methods("POST"_method);
    CROW_ROUTE(app, "/script/run").methods("POST"_method);
    CROW_ROUTE(app, "/script/runAsync").methods("POST"_method);
    CROW_ROUTE(app, "/script/output").methods("POST"_method);
    CROW_ROUTE(app, "/script/status").methods("POST"_method);
    CROW_ROUTE(app, "/script/logs").methods("POST"_method);
    CROW_ROUTE(app, "/script/list").methods("GET"_method);
    CROW_ROUTE(app, "/script/info").methods("POST"_method);

    // ScriptAnalyzer routes (8 routes)
    CROW_ROUTE(app, "/analyzer/analyze").methods("POST"_method);
    CROW_ROUTE(app, "/analyzer/analyzeWithOptions").methods("POST"_method);
    CROW_ROUTE(app, "/analyzer/updateConfig").methods("POST"_method);
    CROW_ROUTE(app, "/analyzer/addCustomPattern").methods("POST"_method);
    CROW_ROUTE(app, "/analyzer/validateScript").methods("POST"_method);
    CROW_ROUTE(app, "/analyzer/getSafeVersion").methods("POST"_method);
    CROW_ROUTE(app, "/analyzer/getTotalAnalyzed").methods("GET"_method);
    CROW_ROUTE(app, "/analyzer/getAverageAnalysisTime").methods("GET"_method);
}
```

**Key Details:**
- 17 HTTP endpoints total
- Two controller sections: /script/* and /analyzer/*
- Weak pointer pattern for ScriptManager and ScriptAnalyzer
- Exception conversion to JSON responses

### Code Section: Error Handling in HTTP Controllers

**File:** `src/server/controller/script.hpp`
**Lines:** 31-91
**Purpose:** Generic error handling pattern for API responses

```cpp
static auto handleScriptAction(
    const crow::request& req, const crow::json::rvalue& body,
    const std::string& command,
    std::function<bool(std::shared_ptr<lithium::ScriptManager>)> func) {

    crow::json::wvalue res;
    res["command"] = command;

    try {
        auto scriptManager = mScriptManager.lock();
        if (!scriptManager) {
            res["status"] = "error";
            res["code"] = 500;
            res["error"] =
                "Internal Server Error: ScriptManager instance is null.";
            return crow::response(500, res);
        } else {
            bool success = func(scriptManager);
            if (success) {
                res["status"] = "success";
                res["code"] = 200;
            } else {
                res["status"] = "error";
                res["code"] = 404;
            }
        }
    } catch (const std::invalid_argument& e) {
        res["status"] = "error";
        res["code"] = 400;
        res["error"] = std::string("Bad Request: Invalid argument - ") +
                       e.what();
    } catch (const std::runtime_error& e) {
        res["status"] = "error";
        res["code"] = 500;
        res["error"] = std::string("Internal Server Error: Runtime error - ") +
                       e.what();
    } catch (const std::exception& e) {
        res["status"] = "error";
        res["code"] = 500;
        res["error"] = std::string("Internal Server Error: Exception occurred - ") +
                       e.what();
    }

    return crow::response(200, res);
}
```

**Key Details:**
- Generic handler for all script operations
- Three exception types: invalid_argument (400), runtime_error (500), generic (500)
- Consistent response format: status, code, error
- Weak pointer validity check

### Code Section: Task System Integration

**File:** `src/task/custom/script_task.hpp`
**Lines:** 77-156
**Purpose:** Unified script execution within task framework

```cpp
class ScriptTask : public Task {
private:
    std::shared_ptr<ScriptManager> scriptManager_;
    std::unique_ptr<ScriptAnalyzer> scriptAnalyzer_;
    std::unique_ptr<PythonWrapper> pythonWrapper_;

    json config_;
    std::map<std::string, ScriptExecutionContext> executionContexts_;
    std::map<std::string, std::vector<std::string>> workflows_;
    std::map<std::string, std::vector<std::string>> dependencies_;
    std::map<std::string, std::function<void(const json&)>> eventHandlers_;
    std::map<std::string, std::string> scriptTemplates_;

public:
    void execute(const json& params) override;

    // Workflow management
    void createWorkflow(const std::string& workflowName,
                        const std::vector<std::string>& scriptSequence);
    void executeWorkflow(const std::string& workflowName,
                         const json& params = {});

    // Dependency management
    void addScriptDependency(const std::string& scriptName,
                             const std::string& dependencyName);
    bool checkDependencies(const std::string& scriptName);

    // Python integration
    template <typename ReturnType, typename... Args>
    ReturnType callPythonFunction(const std::string& alias,
                                  const std::string& functionName,
                                  Args... args);
};
```

**Key Details:**
- Composition of ScriptManager, ScriptAnalyzer, PythonWrapper
- Workflow pipeline support
- Dependency graph management
- Event-driven architecture
- Template methods for Python function calls
- Inherits from Task base class

### Code Section: JSON/Python Type Conversion

**File:** `src/server/controller/python.hpp`
**Lines:** 39-70
**Purpose:** Bridge between Crow JSON and pybind11 objects

```cpp
static py::object jsonToPy(const crow::json::rvalue& val) {
    switch (val.t()) {
        case crow::json::type::String:
            return py::str(std::string(val.s()));
        case crow::json::type::Number:
            if (std::floor(val.d()) == val.d()) {
                return py::int_(static_cast<int64_t>(val.d()));
            }
            return py::float_(val.d());
        case crow::json::type::True:
            return py::bool_(true);
        case crow::json::type::False:
            return py::bool_(false);
        case crow::json::type::List: {
            py::list l;
            for (const auto& item : val) {
                l.append(jsonToPy(item));
            }
            return l;
        }
        case crow::json::type::Object: {
            py::dict d;
            for (const auto& key : val.keys()) {
                d[key.c_str()] = jsonToPy(val[key]);
            }
            return d;
        }
        default:
            return py::none();
    }
}
```

**Key Details:**
- Recursive type mapping
- Handles: string, number (int/float), bool, list, dict, null
- pybind11 casting with type inference
- Used in callMethod endpoint (line 555: TODO comment for JSON args conversion)

### Code Section: Component Manager Integration

**File:** `src/script/CMakeLists.txt` (referenced in search results)
**Purpose:** Build system integration

**Key Details:**
- Script module includes: python_caller, sheller, check
- Links against: pybind11, spdlog, atom libraries
- Part of main lithium component tree

## Findings Section

### HTTP API Architecture

The script engine exposes **52+ HTTP endpoints** across two controllers:

**PythonController** (35 endpoints):
- Manages Python script lifecycle (load/unload/reload/list)
- Function execution: sync, async, batch
- Variable management: get/set
- Object manipulation: method calls, attribute access, lifecycle
- Environment: sys.path manipulation, global variable sync
- Performance: memory introspection, optimization, configuration
- Package management: install/uninstall
- Virtual environments: create/activate
- Debugging: breakpoints, debug mode

**ScriptController** (17 endpoints):
- Shell script management: register/delete/update/list
- Execution: sync/async/sequential/concurrent
- Output/status/logs retrieval
- Script analysis: validate, analyze, get safe version
- Custom patterns: add danger detection patterns
- Statistics: total analyzed, average analysis time

### Component Integration Pattern

The system follows a **composition over inheritance** approach:

```
HTTP Controllers (PythonController, ScriptController)
    ↓
Global Pointer Injection (GET_OR_CREATE_WEAK_PTR)
    ↓
Core Components:
    - PythonWrapper (Python integration)
    - ScriptManager (Shell script execution)
    - ScriptAnalyzer (Security analysis)
    ↓
Task System (ScriptTask)
    ↓
Component Manager
```

### GIL (Global Interpreter Lock) Management

**Acquisition Points:**
1. Worker thread entry (python_caller.cpp:194): `py::gil_scoped_acquire gil`
2. PythonObjectDeleter (python_caller.hpp:476): `py::gil_scoped_acquire gil`
3. Implicit in pybind11 function calls

**Management Strategy:**
- GIL acquired per-worker-thread basis
- No explicit release/reacquisition during long operations
- Semaphore (limit 10) controls concurrent Python task submission
- Thread pool size configurable (default 4)

**Limitations:**
- GIL prevents true parallelism for CPU-bound Python code
- No explicit GIL optimization (enable_gil_optimization flag unused in cpp)
- Async calls still block on GIL

### Configuration System

**Python Wrapper Configuration:**
- Loaded via PerformanceConfig structure
- Set via `configure_performance()` method
- Applies to thread pool and caching behavior

**Script Manager Configuration:**
- Inline defaults: maxVersions=10, cache_timeout=3600s
- Per-script: environment variables, retry strategy, timeouts
- Hooks: pre/post execution registered per-script

**Script Analyzer Configuration:**
- JSON-based pattern definitions
- Platform-specific danger patterns
- Loaded at construction, updated via updateConfig()
- Thread pool and timeout configurable via AnalyzerOptions

### Security Boundary Considerations

**No Sandboxing:**
- Shell scripts execute in calling process environment
- Python scripts execute in embedded interpreter (no isolation)
- No resource limits enforced (configured but not applied)

**Input Validation:**
- Script content validated for dangerous patterns only
- Arguments passed directly to popen() without escaping
- Environment variables concatenated without validation

**Module Loading:**
- Python modules loaded via import() (unrestricted)
- sys.path modification allowed via add_sys_path()
- PowerShell modules imported without validation

### Data Flow in Script Execution

```
HTTP Request (JSON)
    ↓
Controller (type conversion via jsonToPy)
    ↓
PythonWrapper/ScriptManager (execution)
    ↓
popen() / py::object operations
    ↓
Subprocess output / Python return value
    ↓
Controller (response serialization)
    ↓
HTTP Response (JSON)
```

### Error Handling Across Layers

1. **Script execution layer**: Throws exceptions
2. **Manager layer**: Catches and logs via spdlog
3. **HTTP controller**: Catches typed exceptions, converts to JSON
4. **HTTP response**: Consistent format with status, code, error

### Threading Model

**Python Wrapper:**
- Main thread: Owns py::scoped_interpreter
- Worker threads: jthread pool, GIL acquired per operation
- Coordination: semaphore (10 tasks), barrier, latch

**Script Manager:**
- Main thread: Manages script storage
- Execution threads: std::async per script
- No explicit coordination (unbounded concurrency)

**Task System:**
- Inherits coordination from underlying managers
- Workflow execution sequential or parallel per design

### Async Patterns

1. **Python**: std::async(launch::async) → std::future<T>
2. **Shell**: std::async(launch::async) → std::future<optional<pair>>
3. **Task**: async_execute() → AsyncGenerator<py::object>

All use deferred execution model.
