# Script Engine Known Issues, TODOs, and Missing Features

## Evidence Section

### Code Section: Incomplete Method Implementations

**File:** `src/script/python_caller.cpp`
**Lines:** 147-150
**Purpose:** Template instantiation with gaps

```cpp
// Implementation for other methods...

void PythonWrapper::handle_exception(const py::error_already_set& e) {
    spdlog::error("Python Exception: {}", e.what());

    py::module_ traceback = py::module_::import("traceback");
    py::object tb = traceback.attr("format_exc")();
    spdlog::error("Traceback: {}", py::cast<std::string>(tb));
}
```

**Key Details:**
- Comment "Implementation for other methods..." (line 147) indicates incomplete implementation
- Only three template functions explicitly instantiated (lines 273-278)
- Many methods in header only declared, implementation placeholder

### Code Section: Method Declarations Without Implementation

**File:** `src/script/python_caller.hpp`
**Lines:** 61-177
**Purpose:** Public interface with incomplete backing

```cpp
void load_script(const std::string& script_name, const std::string& alias);
void unload_script(const std::string& alias);
void reload_script(const std::string& alias);

std::vector<std::string> get_function_list(const std::string& alias);

py::object call_method(const std::string& alias,
                       const std::string& class_name,
                       const std::string& method_name,
                       const py::args& args);

py::object eval_expression(const std::string& alias,
                           const std::string& expression);

std::vector<int> call_function_with_list_return(
    const std::string& alias, const std::string& function_name,
    const std::vector<int>& input_list);

// Lines 183-242: add_sys_path through execute_script_with_logging
// Lines 244-273: Error handling and performance configuration

// Lines 276-284: Async and batch operations
// Lines 299-348: Object lifecycle, debugging, environment management
// Lines 350-373: Package and virtual environment management
```

**Key Details:**
- 40+ public methods declared in header
- Only forwarding implementations provided in cpp
- Many rely on Impl class methods not shown in cpp excerpt
- Full implementations likely in cpp but not included in read output

### Code Section: Shell Script Progress Tracking TODO

**File:** `src/script/sheller.cpp`
**Lines:** 444-459
**Purpose:** Progress tracking implementation

```cpp
// Use pipe for progress tracking and abort capability
FILE* pipe = popen(scriptCmd.c_str(), "r");
if (!pipe) {
    throw ScriptException("Failed to create pipe for script execution");
}

std::string output;
char buffer[128];
while (!abortFlags_[nameStr].load() &&
       fgets(buffer, sizeof(buffer), pipe) != nullptr) {
    output += buffer;
    // Update progress (example implementation)
    if (output.find("PROGRESS:") != std::string::npos) {
        float progress = std::stof(output.substr(output.find(":") + 1));
        progressTrackers_[nameStr].store(progress);
    }
}
```

**Key Details:**
- Comment "example implementation" indicates incomplete logic
- Progress parsing from output string fragile (no validation)
- stof() can throw on malformed input
- No error recovery for progress parsing

### Code Section: JSON Args Conversion TODO

**File:** `src/server/controller/python.hpp`
**Lines:** 552-568
**Purpose:** Object method calling with argument conversion

```cpp
void callMethod(const crow::request& req, crow::response& res) {
    auto body = crow::json::load(req.body);
    res = handlePythonAction(req, body, "callMethod", [&](auto pythonWrapper) {
        py::args args;  // TODO: Convert from JSON args
        auto result = pythonWrapper->call_method(
            std::string(body["alias"].s()),
            std::string(body["class_name"].s()),
            std::string(body["method_name"].s()),
            args);

        crow::json::wvalue response;
        response["status"] = "success";
        response["code"] = 200;
        response["result"] = std::string(py::str(result));
        res.write(response.dump());
        return true;
    });
}
```

**Key Details:**
- Explicit TODO comment for JSON to py::args conversion
- Empty args passed regardless (always nullptr)
- Method calls will fail if arguments required
- Similar pattern may exist in other endpoints

### Code Section: Missing Implementations in ScriptTask

**File:** `src/task/custom/script_task.hpp`
**Lines:** 78-300
**Purpose:** Extensive method declarations without shown implementation

```cpp
class ScriptTask : public Task {
    // Core methods
    void execute(const json& params) override;

    // Script management
    void registerScript(const std::string& name, const std::string& content);
    void updateScript(const std::string& name, const std::string& content);
    void deleteScript(const std::string& name);
    bool validateScript(const std::string& content);
    ScriptAnalysisResult analyzeScript(const std::string& content);

    // Execution control
    void setScriptTimeout(std::chrono::seconds timeout);
    void setScriptRetryCount(int count);

    // ... 50+ more methods

    // Internal helper methods (lines 268-301)
    void setupDefaults();
    void validateParameters(const json& params);
    void handleScriptError(const std::string& scriptName,
                           const std::string& error);
    // ... more helpers

    // Template implementations
    template <typename ReturnType, typename... Args>
    ReturnType callPythonFunction(const std::string& alias,
                                  const std::string& functionName,
                                  Args... args);
};
```

**Key Details:**
- 50+ method signatures declared
- No implementation file referenced in codebase search
- Template implementations provide partial code (lines 304-338)
- Many methods likely unimplemented stubs

### Code Section: ResourcePool Synchronization

**File:** `src/task/custom/script_task.hpp`
**Lines:** 249-256
**Purpose:** Resource pool with incomplete synchronization

```cpp
struct ResourcePool {
    size_t maxConcurrentScripts;
    size_t totalMemoryLimit;
    size_t usedMemory;
    std::queue<std::string> waitingQueue;
    std::condition_variable resourceAvailable;
    std::mutex resourceMutex;
} resourcePool_;
```

**Key Details:**
- condition_variable declared but no waiters implemented
- usedMemory tracked but not enforced
- Methods setResourcePool() and reserveResources() declared but implementation unclear
- No callback when resources become available

### Code Section: Unused GIL Optimization Flag

**File:** `src/script/python_caller.hpp`
**Lines:** 262-267
**Purpose:** Performance configuration with unused flag

```cpp
struct PerformanceConfig {
    bool enable_threading;         ///< Whether to use threading
    bool enable_gil_optimization;  ///< Whether to optimize GIL usage
    size_t thread_pool_size;       ///< Size of thread pool
    bool enable_caching;           ///< Whether to use caching
};
```

**File:** `src/script/python_caller.cpp`
**Lines:** 224-227
**Purpose:** Default config with flag

```cpp
PerformanceConfig config_{.enable_threading = false,
                          .enable_gil_optimization = true,
                          .thread_pool_size = 4,
                          .enable_caching = true};
```

**Key Details:**
- Flag `enable_gil_optimization` declared
- No code checks or uses this flag
- Configuration interface exists but not implemented
- Suggests incomplete feature

### Code Section: Async Generator Not Used

**File:** `src/script/python_caller.hpp`
**Lines:** 376-432
**Purpose:** Coroutine-based async pattern (declared but not used)

```cpp
template <typename T>
struct AsyncGenerator {
    struct promise_type {
        T value;
        AsyncGenerator get_return_object() {
            return AsyncGenerator{handle_type::from_promise(*this)};
        }
        std::suspend_always initial_suspend() { return {}; }
        std::suspend_always final_suspend() noexcept { return {}; }
        void return_void() {}
        std::suspend_always yield_value(T v) {
            value = v;
            return {};
        }
        void unhandled_exception() { throw; }
    };

    using handle_type = std::coroutine_handle<promise_type>;
    handle_type handle;

    AsyncGenerator(handle_type h) : handle(h) {}
    ~AsyncGenerator() {
        if (handle)
            handle.destroy();
    }

    T current_value() { return handle.promise().value; }
    bool move_next() {
        handle.resume();
        return !handle.done();
    }
};

// Method declared (line 458)
AsyncGenerator<py::object> async_execute(const std::string& script);
```

**Key Details:**
- Coroutine-based generator pattern fully defined
- No implementation shown in cpp file
- One method uses it (async_execute) but implementation missing
- Suggests aspirational feature not completed

### Code Section: Dangerous Pattern Database Not Included

**File:** `src/script/check.hpp` and `src/script/check.cpp`
**Lines:** 88-123
**Purpose:** Configuration file loading

```cpp
static auto loadConfig(const std::string& config_file) -> json {
    if (!atom::io::isFileExists(config_file)) {
        THROW_FILE_NOT_FOUND("Config file not found: " + config_file);
    }
    std::ifstream file(config_file);
    if (!file.is_open()) {
        THROW_FAIL_TO_OPEN_FILE("Unable to open config file: " +
                                config_file);
    }
    json config;
    try {
        file >> config;
    } catch (const json::parse_error& e) {
        THROW_INVALID_FORMAT("Invalid JSON format in config file: " +
                             config_file);
    }
    return config;
}
```

**Key Details:**
- Configuration loaded from external JSON file
- File path required at analyzer construction
- No default patterns provided in code
- Pattern definitions location not specified
- Database structure not defined in codebase

### Code Section: Subprocess Execution Lacks Escaping

**File:** `src/script/sheller.cpp`
**Lines:** 425-442
**Purpose:** Command building without input validation

```cpp
std::string scriptCmd;
{
    std::shared_lock lock(mSharedMutex_);
    if (scripts_.contains(nameStr)) {
        scriptCmd = envVarsCmd + "sh -c \"" + scripts_[nameStr] + "\"";
    } else if (powerShellScripts_.contains(nameStr)) {
        scriptCmd = "powershell.exe -Command \"" + envVarsCmd +
                    powershellSetup + powerShellScripts_[nameStr] +
                    "\"";
    } else {
        throw ScriptException("Script not found: " + nameStr);
    }
}

// Add parameters
for (const auto& [key, value] : args) {
    scriptCmd += " \"" + key + "=" + value + "\"";
}
```

**Key Details:**
- String concatenation without escaping
- Script content inserted directly into command string
- Arguments joined without shell escaping
- Potential command injection vulnerability
- No validation of special characters

## Findings Section

### Incomplete Implementations

**PythonWrapper (python_caller):**
- 40+ public methods declared, few with complete implementations
- Template methods partially instantiated (only int, string, double)
- Missing implementations: batch_process, process_data, get_shared_resource
- Coroutine-based async_execute declared but not implemented
- Many header-only declarations lack cpp backing

**ScriptTask:**
- 50+ methods declared in header
- No corresponding .cpp file located in codebase
- Template methods have partial implementation
- Methods: createWorkflow, executeWorkflow, pauseWorkflow incomplete
- Resource pool management not fully wired

**ScriptAnalyzer:**
- Configuration file required but location undefined
- Default pattern database not provided in code
- Pattern format not documented

### Known TODOs

**Code Level TODOs:**
1. **Line 555 (python.hpp)**: "TODO: Convert from JSON args" in callMethod endpoint
2. **Line 455 (sheller.cpp)**: "example implementation" for progress parsing
3. **Implicit**: Enable threading flag set to false (unimplemented threading?)

### Missing Features or Incomplete

1. **GIL Optimization**: Flag exists but not used
2. **Resource Limits**: Declared but not enforced
3. **Subprocess Sandboxing**: None implemented
4. **Input Validation**: Pattern-based validation only, no escaping
5. **Coroutine Support**: AsyncGenerator defined but not used
6. **Profiling**: ProfilingData struct in ScriptTask but no implementation
7. **Debugging**: Breakpoints declared but implementation unclear
8. **Script Caching**: Enabled but no cache eviction beyond TTL
9. **Template Scripts**: Methods exist but implementation details missing
10. **Language Bridging**: bridgeLanguages() declared but not implemented

### Security Gaps

1. **No Input Escaping**: Script args passed directly to popen()
2. **No Module Sandboxing**: Python modules loaded without restriction
3. **No Subprocess Isolation**: Shell scripts run in calling process environment
4. **No Resource Enforcement**: Memory/CPU limits configured but not applied
5. **Pattern Database Unknown**: No default dangerous patterns in codebase
6. **Weak Progress Parsing**: Relies on string search, no validation

### Configuration Issues

1. **Analyzer Config Path Required**: No fallback or default
2. **Pattern Definition Location Unknown**: Expects external JSON file
3. **Enable Threading Default False**: Suggests incomplete implementation
4. **GIL Optimization Unused**: Configuration option without effect

### Threading/Async Issues

1. **Unbounded Concurrency**: Script execution has no limit per controller
2. **Semaphore Not Enforced**: Python wrapper semaphore initialized but usage unclear
3. **No Timeout Enforcement**: Timeout handler called but subprocess not killed
4. **GIL Blocking**: No GIL optimization despite flag

### Performance Issues

1. **String-Based Progress Parsing**: Each line parsed for "PROGRESS:" marker
2. **Regex Per-Line**: Security analysis regex compiled per line
3. **No Caching**: Script analysis results not cached
4. **Memory Pool Unused**: Thread-local pool declared but not used in Impl

### Testing Gaps

Given file deletions in git status, testing framework may be incomplete:
- Deleted: `tests/components/test_*.cpp` files
- Deleted: `src/task/custom/camera/*.cpp` files (unrelated but indicates refactoring)
- No test files for script engine located

### Compilation Status

- Files compile (referenced in cmake)
- Linker may catch undefined symbols from stub implementations
- Template instantiation may fail at link time for unimplemented methods
