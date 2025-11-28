# Script Engine Implementation Overview

## Evidence Section

### Code Section: Python Integration via pybind11

**File:** `src/script/python_caller.hpp`
**Lines:** 1-50, 375-502
**Purpose:** Defines PythonWrapper class for Python script management and integration

```cpp
class PythonWrapper {
public:
    void load_script(const std::string& script_name, const std::string& alias);
    void unload_script(const std::string& alias);
    void reload_script(const std::string& alias);

    template <typename ReturnType, typename... Args>
    ReturnType call_function(const std::string& alias,
                             const std::string& function_name, Args... args);

    template <typename T>
    T get_variable(const std::string& alias, const std::string& variable_name);

    void set_variable(const std::string& alias,
                      const std::string& variable_name,
                      const py::object& value);

    // Async support
    template <typename ReturnType>
    std::future<ReturnType> async_call_function(
        const std::string& alias, const std::string& function_name);

    // Concurrency control members
    mutable std::shared_mutex resource_mutex_;
    std::counting_semaphore<> task_semaphore_{10};
    std::barrier<> sync_point_{2};
    std::latch completion_latch_{1};
};
```

**Key Details:**
- Uses pybind11 for Python/C++ interoperability
- Header includes: `<pybind11/embed.h>`, `<pybind11/pybind11.h>`, `<pybind11/stl.h>`
- Implements PythonConvertible concept for type safety
- Thread-safe with shared_mutex and semaphore synchronization
- GILManager class (lines 484-498) handles Python Global Interpreter Lock

### Code Section: Shell Script Execution Manager

**File:** `src/script/sheller.hpp`
**Lines:** 45-150
**Purpose:** ScriptManager for shell/PowerShell script execution

```cpp
class ScriptManager {
public:
    void registerScript(std::string_view name, const Script& script);
    void registerPowerShellScript(std::string_view name, const Script& script);

    auto runScript(std::string_view name,
                   const std::unordered_map<std::string, std::string>& args,
                   bool safe = true,
                   std::optional<int> timeoutMs = std::nullopt,
                   int retryCount = 0)
        -> std::optional<std::pair<std::string, int>>;

    auto runScriptAsync(std::string_view name,
                        const std::unordered_map<std::string, std::string>& args,
                        bool safe = true)
        -> std::future<std::optional<std::pair<std::string, int>>>;

    void setRetryStrategy(std::string_view name, RetryStrategy strategy);
    auto rollbackScript(std::string_view name, int version) -> bool;
};
```

**Key Details:**
- Manages both bash shell and PowerShell scripts
- Version control for scripts with rollback capability
- Pre/post-execution hooks
- Timeout and retry mechanisms with strategies: Linear, Exponential, None, Custom
- Environment variable injection

### Code Section: Script Validation and Security Analysis

**File:** `src/script/check.hpp` and `src/script/check.cpp`
**Lines:** 28-67, 469-530
**Purpose:** ScriptAnalyzer for script security analysis

```cpp
class ScriptAnalyzer {
public:
    AnalysisResult analyzeWithOptions(const std::string& script,
                                      const AnalyzerOptions& options);

    bool validateScript(const std::string& script);
    std::string getSafeVersion(const std::string& script);

    // Callback for danger detection
    void setCallback(std::function<void(const DangerItem&)> callback);
};

struct DangerItem {
    std::string category;
    std::string command;
    std::string reason;
    int line;
    std::optional<std::string> context;
};

struct AnalysisResult {
    int complexity;
    std::vector<DangerItem> dangers;
    double execution_time;
    bool timeout_occurred;
};
```

**Key Details:**
- Detects script type: PowerShell, Python, Ruby, Bash
- Pattern-based danger detection with regex
- Security issue categories: Unsafe Commands, External Commands, Environment Variables
- Safe replacements available for dangerous patterns
- Thread pool for parallel analysis

### Code Section: Python Wrapper Implementation Details

**File:** `src/script/python_caller.cpp`
**Lines:** 40-245
**Purpose:** Implementation class with threading and caching

```cpp
class PythonWrapper::Impl {
    static thread_local std::pmr::synchronized_pool_resource memory_pool;
    std::vector<std::jthread> worker_threads_;

    std::unordered_map<std::string, CacheEntry> function_cache_;
    std::chrono::seconds cache_timeout_{3600};

    PerformanceConfig config_{.enable_threading = false,
                              .enable_gil_optimization = true,
                              .thread_pool_size = 4,
                              .enable_caching = true};

    ErrorHandlingStrategy error_strategy_ =
        ErrorHandlingStrategy::THROW_EXCEPTION;

    py::scoped_interpreter guard_;
    std::unordered_map<std::string, py::module> scripts_;
};
```

**Key Details:**
- Performance configuration enables/disables threading and caching
- Function result caching with 1-hour timeout
- Thread pool setup (line 152-167)
- GIL acquisition in worker threads (line 194)
- Error handling strategies: THROW_EXCEPTION, RETURN_DEFAULT, LOG_AND_CONTINUE

### Code Section: Shell Script Execution Implementation

**File:** `src/script/sheller.cpp`
**Lines:** 386-481
**Purpose:** Runtime script execution with progress tracking

```cpp
auto ScriptManagerImpl::runScriptImpl(
    std::string_view name,
    const std::unordered_map<std::string, std::string>& args,
    bool safe, std::optional<int> timeoutMs, int retryCount)
    -> std::optional<std::pair<std::string, int>> {

    // Pre-execution hooks
    for (const auto& hook : preHooks_[nameStr]) {
        hook(nameStr);
    }

    // Progress tracking
    progressTrackers_[nameStr].store(0.0f);
    abortFlags_[nameStr].store(false);

    // Pipe execution for progress tracking
    FILE* pipe = popen(scriptCmd.c_str(), "r");

    while (!abortFlags_[nameStr].load() &&
           fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
        if (output.find("PROGRESS:") != std::string::npos) {
            progressTrackers_[nameStr].store(progress);
        }
    }

    // Post-execution hooks
    for (const auto& hook : postHooks_[nameStr]) {
        hook(output, status);
    }
}
```

**Key Details:**
- Uses `popen()` for script execution
- Supports progress tracking via "PROGRESS:" markers
- Abort capability via atomic flags
- Custom exit code for aborted scripts: -999
- Timeout handling via futures and async execution

### Code Section: HTTP API Integration

**File:** `src/server/controller/python.hpp`
**Lines:** 128-282
**Purpose:** REST endpoints for Python script management

```cpp
class PythonController : public Controller {
    void registerRoutes(lithium::server::ServerApp& app) override {
        GET_OR_CREATE_WEAK_PTR(mPythonWrapper, lithium::PythonWrapper,
                               Constants::PYTHON_WRAPPER);

        // 30+ endpoints including:
        CROW_ROUTE(app, "/python/load").methods("POST"_method);
        CROW_ROUTE(app, "/python/call").methods("POST"_method);
        CROW_ROUTE(app, "/python/callAsync").methods("POST"_method);
        CROW_ROUTE(app, "/python/batchExecute").methods("POST"_method);
        CROW_ROUTE(app, "/python/eval").methods("POST"_method);
        CROW_ROUTE(app, "/python/inject").methods("POST"_method);
        // ... and many more
    }
};
```

**Key Details:**
- 30+ HTTP endpoints via Crow framework
- Global pointer injection pattern (GET_OR_CREATE_WEAK_PTR)
- JSON to Python object conversion (jsonToPy, lines 39-70)
- Generic handler pattern for error management
- Supports: script lifecycle, function calls, variable access, async ops, package management

### Code Section: Task System Integration

**File:** `src/task/custom/script_task.hpp`
**Lines:** 73-301
**Purpose:** Task-based script execution with workflow support

```cpp
class ScriptTask : public Task {
    std::shared_ptr<ScriptManager> scriptManager_;
    std::unique_ptr<ScriptAnalyzer> scriptAnalyzer_;
    std::unique_ptr<PythonWrapper> pythonWrapper_;

    std::map<std::string, ScriptExecutionContext> executionContexts_;
    std::map<std::string, std::vector<std::string>> workflows_;
    std::map<std::string, std::vector<std::string>> dependencies_;

    void executeWithContext(const std::string& scriptName,
                            const ScriptExecutionContext& context);

    void createWorkflow(const std::string& workflowName,
                        const std::vector<std::string>& scriptSequence);

    std::future<ScriptStatus> executeAsync(const std::string& scriptName,
                                           const json& params = {});
};
```

**Key Details:**
- Composition pattern: uses ScriptManager, ScriptAnalyzer, PythonWrapper
- Resource pool management
- Workflow support for script pipelines
- Dependency management and resolution
- Event-driven architecture

## Findings Section

### Current Implementation Status

The script engine consists of three main components:

1. **PythonWrapper** (`python_caller.hpp/cpp`)
   - Provides complete Python integration via pybind11
   - Manages script loading, unloading, reloading with module caching
   - Supports function calls, variable access, object manipulation
   - Thread pool with configurable sizing (default 4 threads)
   - Function result caching with 1-hour TTL
   - GIL management via py::gil_scoped_acquire and custom GILManager
   - Performance configuration: threading, caching, optimization flags

2. **ScriptManager** (`sheller.hpp/cpp`)
   - Manages shell (bash/sh) and PowerShell script execution
   - Uses popen() for subprocess management
   - Supports versioning with configurable max versions (default 10)
   - Pre/post execution hooks
   - Timeout with millisecond precision
   - Retry strategies: Linear, Exponential, None, Custom
   - Environment variable injection
   - Progress tracking via atomic float flags
   - Abort capability with custom exit code (-999)

3. **ScriptAnalyzer** (`check.hpp/cpp`)
   - Pattern-based security analysis
   - Multi-language detection: PowerShell, Python, Ruby, Bash
   - Parallel analysis via thread pool
   - Configurable timeout (default 30 seconds)
   - Complexity calculation via control flow pattern matching
   - Safe version generation via pattern replacement
   - Multiple report formats: TEXT, JSON, XML

### API Integration Points

- **HTTP API**: 30+ endpoints via PythonController and ScriptController (Crow framework)
- **Task System**: ScriptTask integrates all three components with dependency management
- **Component System**: Registered via global pointer injection pattern
- **Event System**: Fire/listen pattern for script events

### Identified Limitations

1. **GIL Management**:
   - GILManager acquired per-thread in worker threads (python_caller.cpp:194)
   - No explicit GIL release/reacquisition optimization for concurrent operations
   - Counting semaphore limits to 10 concurrent tasks

2. **Error Handling**:
   - Three strategies defined but inconsistently applied across methods
   - Python exceptions logged but not all converted to structured responses
   - Shell script execution errors may lose context

3. **Shell Execution Security**:
   - Uses popen() without explicit sandbox
   - Environment variable passthrough without validation
   - Script content passed directly to shell with potential injection risk

4. **Module Loading**:
   - No explicit Python module path isolation
   - sys.path manipulation via add_sys_path() allows module search path modification
   - No sandbox or restricted execution context

5. **Resource Management**:
   - Script caching but no cache eviction beyond TTL
   - No explicit memory limit enforcement (atomic flags only)
   - Workflow state not persisted
