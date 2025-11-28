# Script Engine Operations and Capabilities

## Evidence Section

### Code Section: Python Function Calling Mechanism

**File:** `src/script/python_caller.cpp`
**Lines:** 105-145
**Purpose:** Template function calling with caching

```cpp
template <typename ReturnType, typename... Args>
ReturnType call_function(const std::string& alias,
                         const std::string& function_name, Args... args) {
    try {
        // Cache lookup
        if (config_.enable_caching) {
            std::string cache_key = alias + "::" + function_name;
            auto cache_it = function_cache_.find(cache_key);
            if (cache_it != function_cache_.end()) {
                cache_it->second.update_access();
                return cache_it->second.cached_result.cast<ReturnType>();
            }
        }

        py::object result = iter->second.attr(function_name.c_str())(args...);

        // Store in cache
        if (config_.enable_caching) {
            function_cache_.emplace(cache_key, CacheEntry(result));
        }

        return result.cast<ReturnType>();
    } catch (const py::error_already_set& e) {
        spdlog::error("Error calling function: {}", e.what());
        throw std::runtime_error(...);
    }
}
```

**Key Details:**
- Generic return type support via template
- Variadic argument passing
- Two-level caching: cache check before execution, store after
- Exception conversion: py::error_already_set to std::runtime_error
- pybind11 casting for type conversion

### Code Section: Async Function Execution

**File:** `src/script/python_caller.hpp`
**Lines:** 282-284
**Purpose:** Asynchronous Python function invocation

```cpp
template <typename ReturnType>
std::future<ReturnType> async_call_function(
    const std::string& alias, const std::string& function_name);

// Implementation in cpp (lines 303-309)
template <typename ReturnType>
std::future<ReturnType> PythonWrapper::async_call_function(
    const std::string& alias, const std::string& function_name) {
    return std::async(std::launch::async, [this, alias, function_name]() {
        return this->call_function<ReturnType>(alias, function_name);
    });
}
```

**Key Details:**
- Uses std::async with std::launch::async policy
- Deferred execution pattern
- Future-based result handling
- No explicit GIL management in async path (relies on thread worker)

### Code Section: Shell Script Argument Handling

**File:** `src/script/sheller.cpp`
**Lines:** 404-442
**Purpose:** Script argument injection and environment setup

```cpp
// Build environment variables
std::string envVarsCmd;
if (environmentVars_.contains(nameStr)) {
    for (const auto& [key, value] : environmentVars_[nameStr]) {
        if (atom::system::isWsl()) {
            envVarsCmd += "$env:" + key + "=\"" + value + "\";";
        } else {
            envVarsCmd += "export " + key + "=\"" + value + "\";";
        }
    }
}

// PowerShell specific setup
std::string powershellSetup;
if (powerShellScripts_.contains(nameStr)) {
    powershellSetup = "$ErrorActionPreference = 'Stop';\n";
    for (const auto& module : loadedPowerShellModules_) {
        powershellSetup += "Import-Module " + module + ";\n";
    }
}

// Add parameters
for (const auto& [key, value] : args) {
    scriptCmd += " \"" + key + "=" + value + "\"";
}
```

**Key Details:**
- OS detection: WSL vs standard PowerShell vs bash
- Environment variable concatenation
- Quoted parameter passing
- PowerShell module auto-import
- String concatenation without escaping (potential injection risk)

### Code Section: Concurrent Script Execution

**File:** `src/script/sheller.cpp`
**Lines:** 567-596
**Purpose:** Multi-threaded script execution

```cpp
auto ScriptManagerImpl::runScriptsConcurrently(
    const std::vector<std::pair<
        std::string, std::unordered_map<std::string, std::string>>>& scripts,
    bool safe, int retryCount)
    -> std::vector<std::optional<std::pair<std::string, int>>> {

    std::vector<std::future<std::optional<std::pair<std::string, int>>>>
        futures;
    futures.reserve(scripts.size());

    for (const auto& [name, args] : scripts) {
        futures.emplace_back(
            std::async(std::launch::async, &ScriptManagerImpl::runScriptImpl,
                       this, name, args, safe, std::nullopt, retryCount));
    }

    std::vector<std::optional<std::pair<std::string, int>>> results;
    for (auto& future : futures) {
        try {
            results.emplace_back(future.get());
        } catch (const std::exception& e) {
            results.emplace_back(std::nullopt);
        }
    }
    return results;
}
```

**Key Details:**
- Creates futures for each script
- No concurrency limit enforcement (relies on system resources)
- Exception handling per-script with nullopt on failure
- Awaits all futures before returning

### Code Section: Security Analysis Pattern Detection

**File:** `src/script/check.cpp`
**Lines:** 139-194
**Purpose:** Multi-language script type detection and analysis

```cpp
void detectScriptTypeAndAnalyze(const std::string& script,
                                std::vector<DangerItem>& dangers) {
    std::shared_lock lock(config_mutex_);

#ifdef _WIN32
    bool isPowerShell = detectPowerShell(script);
    if (isPowerShell) {
        checkPattern(script, config_["powershell_danger_patterns"],
                     "PowerShell Security Issue", dangers);
    } else {
        checkPattern(script, config_["windows_cmd_danger_patterns"],
                     "CMD Security Issue", dangers);
    }
#else
    if (detectPython(script)) {
        checkPattern(script, config_["python_danger_patterns"],
                     "Python Script Security Issue", dangers);
    } else if (detectRuby(script)) {
        checkPattern(script, config_["ruby_danger_patterns"],
                     "Ruby Script Security Issue", dangers);
    } else {
        checkPattern(script, config_["bash_danger_patterns"],
                     "Shell Script Security Issue", dangers);
    }
#endif
}

static bool detectPowerShell(const std::string& script) {
    return script.find("param(") != std::string::npos ||
           script.find("$PSVersionTable") != std::string::npos;
}
```

**Key Details:**
- Platform-specific detection: Windows vs Unix
- Pattern-based language detection (substring matching)
- Loads configuration from JSON with regex patterns
- Executes parallel analysis tasks

### Code Section: Dangerous Pattern Replacement

**File:** `src/script/check.cpp`
**Lines:** 181-194
**Purpose:** Suggest safe alternatives for dangerous operations

```cpp
void suggestSafeReplacements(const std::string& script,
                             std::vector<DangerItem>& dangers) {
    static const std::unordered_map<std::string, std::string> replacements = {
#ifdef _WIN32
        {"Remove-Item -Recurse -Force", "Remove-Item -Recurse"},
        {"Stop-Process -Force", "Stop-Process"},
#else
        {"rm -rf /", "find . -type f -delete"},
        {"kill -9", "kill -TERM"},
#endif
    };
    checkReplacements(script, replacements, dangers);
}
```

**Key Details:**
- Pre-defined replacement pairs per platform
- Line-by-line string matching (no regex)
- Reports unsafe patterns with suggested safe alternatives

### Code Section: Batch Function Execution

**File:** `src/script/python_caller.hpp`
**Lines:** 293-296
**Purpose:** Execute multiple functions in sequence

```cpp
template <typename ReturnType>
std::vector<ReturnType> batch_execute(
    const std::string& alias,
    const std::vector<std::string>& function_names);
```

**Key Details:**
- Processes function list in order
- Single return type for all functions
- Results collected in vector
- Used for multi-step workflows

### Code Section: Timeout Handling in Script Execution

**File:** `src/script/sheller.cpp`
**Lines:** 489-508
**Purpose:** Async execution with timeout support

```cpp
try {
    std::stop_source stopSource;
    auto future = std::async(std::launch::async,
        [this, name, args, &stopSource, safe, timeoutMs, retryCount]() {
            return runScriptImpl(name, args, safe, timeoutMs, retryCount);
        });

    if (timeoutMs) {
        if (future.wait_for(std::chrono::milliseconds(*timeoutMs)) ==
            std::future_status::timeout) {
            stopSource.request_stop();
            if (auto handler = timeoutHandlers_.find(std::string(name));
                handler != timeoutHandlers_.end()) {
                handler->second();
            }
            return std::nullopt;
        }
    }

    return future.get();
}
```

**Key Details:**
- std::stop_source for cancellation (C++20)
- Millisecond precision timeout
- Custom timeout handler callback
- Returns std::nullopt on timeout
- Does not terminate the subprocess

## Findings Section

### Supported Operations

**Python Operations:**
- Script loading (by file) and unloading
- Module reloading via importlib
- Function calls with type-safe return values
- Variable read/write access
- Object method calls and attribute access
- Expression evaluation
- Code injection via exec
- Memory usage introspection
- Package installation/uninstallation
- Virtual environment management
- Asynchronous function calls
- Batch function execution
- Breakpoint-based debugging

**Shell Script Operations:**
- Bash and PowerShell script execution
- Version-controlled script storage (rollback capability)
- Sequential and concurrent execution
- Timeout with cancellation
- Retry with strategies (Linear, Exponential, Custom)
- Environment variable injection (platform-aware)
- Pre/post execution hooks
- Progress tracking via output parsing
- Script abort capability
- PowerShell module loading
- Output and exit status capture

**Security Analysis:**
- Script validation (dangerous pattern detection)
- Complexity calculation (control flow counting)
- Safe version generation (pattern substitution)
- Configuration-driven pattern definitions
- JSON/XML/Text report generation
- Callback-based danger notification
- Statistics: total analyzed, average analysis time

### Async and Threading Support

**Python:**
- Per-function async calls via std::async(std::launch::async)
- Thread pool with configurable size (default 4)
- Counting semaphore limits concurrent tasks to 10
- GIL acquired per worker thread
- Synchronization primitives: barrier, latch, semaphore

**Shell Scripts:**
- std::async for concurrent script execution
- No built-in concurrency limit (unbounded)
- Timeout-aware futures

### Error Handling Strategies

1. **THROW_EXCEPTION**: Exceptions propagate to caller
2. **RETURN_DEFAULT**: Return default values on error
3. **LOG_AND_CONTINUE**: Log and continue execution

Default: THROW_EXCEPTION

### Configuration Options

**Python Wrapper:**
- `enable_threading`: bool (default false)
- `enable_gil_optimization`: bool (default true)
- `thread_pool_size`: size_t (default 4)
- `enable_caching`: bool (default true)
- Cache timeout: 1 hour

**Script Manager:**
- `maxVersions`: int (default 10)
- `safe` mode flag per execution
- Timeout in milliseconds
- Retry count configuration
- Error handling strategy selection

**Script Analyzer:**
- `async_mode`: bool (default true)
- `deep_analysis`: bool (default false)
- `thread_count`: size_t (default 4)
- `timeout_seconds`: int (default 30)
- `ignore_patterns`: vector<string>

### Output and Logging

- spdlog integration for structured logging
- Python exception conversion to traceback strings
- Script output captured via popen()
- Analysis results with line numbers and context
- JSON serialization for API responses
- Progress tracking via atomic float (0.0-1.0)
