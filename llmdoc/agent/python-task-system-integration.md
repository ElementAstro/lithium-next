# Python Task System Integration

## Overview

Lithium-Next integrates Python execution into its task system, allowing Python tools to participate in automated workflows, task sequencing, and parameterized execution pipelines.

## Task System Architecture

### Task Hierarchy

**Code Section: Task Base Classes**

**File:** `src/task/custom/script/base.hpp`
**Lines:** 48-108

```cpp
class BaseScriptTask : public Task {
public:
    BaseScriptTask(const std::string& name,
                   const std::string& scriptConfigPath = "");

    void execute(const json& params) override;

protected:
    virtual ScriptExecutionResult executeScript(
        const std::string& scriptName,
        const std::unordered_map<std::string, std::string>& args) = 0;

    void validateScriptParams(const json& params);
    void setupScriptDefaults();
    ScriptType detectScriptType(const std::string& content);
    void handleScriptError(const std::string& scriptName,
                          const std::string& error);
};
```

### Python Script Task Implementation

**Code Section: PythonScriptTask**

**File:** `src/task/custom/script/python.hpp`
**Lines:** 14-44

```cpp
class PythonScriptTask : public BaseScriptTask {
public:
    PythonScriptTask(const std::string& name,
                    const std::string& scriptConfigPath = "");

    void loadPythonModule(const std::string& moduleName,
                         const std::string& alias = "");

    template <typename ReturnType, typename... Args>
    ReturnType callPythonFunction(const std::string& alias,
                                 const std::string& functionName,
                                 Args... args);

    template <typename T>
    T getPythonVariable(const std::string& alias,
                       const std::string& varName);

    void setPythonVariable(const std::string& alias,
                          const std::string& varName,
                          const py::object& value);

protected:
    ScriptExecutionResult executeScript(
        const std::string& scriptName,
        const std::unordered_map<std::string, std::string>& args) override;

private:
    void initializePythonEnvironment();
    void setupPythonDefaults();
    std::unique_ptr<PythonWrapper> pythonWrapper_;
    std::map<std::string, py::object> compiledScripts_;
};
```

**Key Details:**

- Inherits from `BaseScriptTask` which inherits from `Task`
- Owns its own `PythonWrapper` instance (separate from global instance)
- Caches compiled Python scripts in `compiledScripts_` map
- Provides template methods for type-safe function calling

## Task Execution Flow

### Script Execution Result

**Code Section: Execution Result Structure**

**File:** `src/task/custom/script/base.hpp`
**Lines:** 30-37

```cpp
struct ScriptExecutionResult {
    bool success;                          // Execution status
    int exitCode;                          // Return code
    std::string output;                    // stdout or result
    std::string error;                     // stderr or error message
    std::chrono::milliseconds executionTime;  // Duration
};
```

### Task Execution Interface

**Code Section: Task Execute Method**

**File:** `src/task/custom/script/base.hpp`
**Lines:** 62-62

```cpp
void execute(const json& params) override;
```

Execution flow:

1. Receives parameters as JSON object
2. Validates parameters via `validateScriptParams()`
3. Extracts script name and arguments
4. Calls `executeScript()` (implemented by subclass)
5. Handles errors via `handleScriptError()`
6. Updates task state based on result

### Template Method Implementations

**Code Section: Function Calling Template**

**File:** `src/task/custom/script/python.hpp`
**Lines:** 47-65

```cpp
template <typename ReturnType, typename... Args>
ReturnType PythonScriptTask::callPythonFunction(
    const std::string& alias,
    const std::string& functionName,
    Args... args) {
    if (!pythonWrapper_) {
        throw std::runtime_error("Python wrapper not initialized");
    }

    try {
        addHistoryEntry("Calling Python function: " + alias +
                       "::" + functionName);
        return pythonWrapper_->call_function<ReturnType>(
            alias, functionName, args...);
    } catch (const std::exception& e) {
        handleScriptError(
            alias, "Python function call failed: " + std::string(e.what()));
        throw;
    }
}
```

**Features:**

- Validates wrapper initialization
- Logs function calls to history
- Propagates exceptions with error context
- Type-safe return value handling

## Task Parameter Definition

### Script Type Detection

**Code Section: Script Type Enum**

**File:** `src/task/custom/script/base.hpp`
**Lines:** 20-24

```cpp
enum class ScriptType {
    Shell,   ///< Shell or Bash script
    Python,  ///< Python script
    Auto     ///< Automatically detect script type
};
```

### Parameter Validation

**Code Section: Parameter Setup**

**File:** `src/task/custom/script/base.hpp`
**Lines:** 80-85

```cpp
void validateScriptParams(const json& params);
void setupScriptDefaults();
```

Uses the task API to:

1. Define parameter types and constraints
2. Set default values
3. Validate inputs against definitions
4. Provide helpful error messages

## Task Registration and Discovery

### Task Manager Integration

Tasks are registered through the task system when the application starts:

```cpp
// In task registration system
TaskManager::getInstance()->registerTask<PythonScriptTask>(
    "python_script",
    [](const json& config) {
        return std::make_shared<PythonScriptTask>(
            config["name"], config.value("scriptConfigPath", ""));
    });
```

### Task Discovery

Available tasks can be queried via:

```cpp
auto task_list = task_manager->getTaskList();
auto task_info = task_manager->getTaskInfo("python_script");
```

## Workflow Integration

### Task Sequences with Python

Python script tasks can be chained in workflows:

```json
{
    "workflow": "image_processing",
    "tasks": [
        {
            "type": "python_script",
            "name": "calibration",
            "script": "calibration_tool",
            "params": {
                "darkFrames": 10,
                "calibrationPath": "/data/calibration"
            }
        },
        {
            "type": "python_script",
            "name": "analysis",
            "script": "analysis_tool",
            "params": {
                "input": "/data/calibrated",
                "output": "/data/results"
            },
            "dependencies": ["calibration"]
        }
    ]
}
```

### Dependency Management

**Code Section: Task Dependencies**

**File:** `src/task/task.hpp`

The base `Task` class provides:

```cpp
// Dependency specification
std::vector<std::string> dependencies;

// Pre and post task hooks
std::function<bool()> preTask;
std::function<bool()> postTask;

// Timeout support
std::chrono::milliseconds timeout;

// Priority levels (1-10)
int priority;
```

Python script tasks inherit these capabilities:

```cpp
auto task1 = TaskManager::create<PythonScriptTask>("step1");
auto task2 = TaskManager::create<PythonScriptTask>("step2");

task2->dependencies.push_back("step1");  // step2 depends on step1
task2->priority = 5;  // Medium priority
task2->timeout = std::chrono::seconds(30);  // 30 second timeout
```

## Error Handling in Tasks

### Error Context Tracking

**Code Section: Error Handling**

**File:** `src/task/custom/script/base.hpp`
**Lines:** 99-100

```cpp
void handleScriptError(const std::string& scriptName,
                      const std::string& error);
```

Error handling:

1. Logs error message
2. Updates task state to ERROR
3. Stores error for inspection
4. Triggers error callbacks if configured
5. Decides whether to continue or abort sequence

### Exception Propagation

Exceptions in Python functions propagate through the task system:

```cpp
try {
    task->execute(params);
} catch (const std::runtime_error& e) {
    // Handle task execution error
    task->setState(TaskState::Failed);
    notifyErrorHandlers(task, e);
}
```

## History and Logging

### Task History Tracking

**Code Section: History Entry**

**File:** `src/task/custom/script/python.hpp`
**Lines:** 56-57

```cpp
addHistoryEntry("Calling Python function: " + alias +
               "::" + functionName);
```

Maintains execution history with timestamps:

```cpp
struct HistoryEntry {
    std::chrono::system_clock::time_point timestamp;
    std::string event;
    std::string status;  // "start", "success", "error"
};
```

### Debug Information

History provides:

- Function call sequence
- Parameter values
- Return values
- Error messages with timestamps
- Performance metrics

## Configuration Management

### Script Configuration Files

**File Format:** `script_config.json`

```json
{
    "name": "image_processing",
    "scripts": {
        "calibration": {
            "path": "astronomy.calibration",
            "timeout": 300,
            "retries": 2
        },
        "analysis": {
            "path": "astronomy.analysis",
            "timeout": 600,
            "cacheResults": true
        }
    },
    "parameters": {
        "darkFrames": {
            "type": "integer",
            "default": 10,
            "min": 1,
            "max": 100
        },
        "outputPath": {
            "type": "string",
            "default": "/data/output",
            "required": true
        }
    }
}
```

### Runtime Configuration

Configuration accessed via task API:

```cpp
json config = task->getConfig("calibration");
int timeout = config.value("timeout", 300);
int retries = config.value("retries", 1);
```

## Performance Considerations

### Script Caching

**Code Section: Compiled Script Cache**

**File:** `src/task/custom/script/python.hpp`
**Lines:** 43-43

```cpp
std::map<std::string, py::object> compiledScripts_;
```

Caches compiled Python code objects:

```cpp
// First call: compile and cache
py::object compiled = py::compile(script_content);
compiledScripts_[script_name] = compiled;

// Subsequent calls: use cached version
py::exec(compiledScripts_[script_name]);
```

### Asynchronous Execution

Tasks can execute asynchronously:

```cpp
auto future = std::async(std::launch::async, [this, params]() {
    this->execute(params);
});

// Check completion
if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
    future.get();  // Get result
}
```

## Example: Complete Python Task

### Python Script

```python
# astronomy_tool.py

import json
from datetime import datetime

class AstronomyCalculator:
    def __init__(self):
        self.last_calculation = None

    def calculate_altitude(self, ra: float, dec: float,
                          lat: float, ha: float) -> dict:
        """Calculate altitude and azimuth."""
        import math

        sin_alt = (math.sin(dec * math.pi/180) *
                   math.sin(lat * math.pi/180) +
                   math.cos(dec * math.pi/180) *
                   math.cos(lat * math.pi/180) *
                   math.cos(ha * math.pi/180))

        altitude = math.asin(sin_alt) * 180 / math.pi

        cos_az = (math.sin(dec * math.pi/180) -
                  math.sin(alt * math.pi/180) *
                  math.sin(lat * math.pi/180)) / (
                  math.cos(alt * math.pi/180) *
                  math.cos(lat * math.pi/180))

        azimuth = math.acos(cos_az) * 180 / math.pi

        self.last_calculation = {
            "timestamp": datetime.now().isoformat(),
            "altitude": altitude,
            "azimuth": azimuth
        }

        return self.last_calculation

    def get_tool_info(self) -> dict:
        return {
            "name": "astronomy_calculator",
            "version": "1.0.0",
            "description": "Astronomical coordinate calculations",
            "functions": ["calculate_altitude"]
        }

# Global instance
_calculator = AstronomyCalculator()

def calculate_altitude(ra: float, dec: float,
                      lat: float, ha: float) -> dict:
    return _calculator.calculate_altitude(ra, dec, lat, ha)

def get_tool_info() -> dict:
    return _calculator.get_tool_info()
```

### Task Configuration

```json
{
    "name": "altitude_calculation",
    "type": "python_script",
    "script": "astronomy_tool",
    "scriptAlias": "astro",
    "function": "calculate_altitude",
    "parameters": {
        "ra": 45.5,
        "dec": 30.2,
        "lat": 40.0,
        "ha": 2.5
    },
    "timeout": 10,
    "priority": 5
}
```

### C++ Task Creation and Execution

```cpp
auto task = std::make_shared<PythonScriptTask>(
    "altitude_calculation", "config/script_config.json");

task->loadPythonModule("astronomy_tool", "astro");

json params;
params["ra"] = 45.5;
params["dec"] = 30.2;
params["lat"] = 40.0;
params["ha"] = 2.5;

task->execute(params);

// Get result
auto result = task->callPythonFunction<json>(
    "astro", "calculate_altitude", 45.5, 30.2, 40.0, 2.5);

std::cout << "Altitude: " << result["altitude"] << std::endl;
std::cout << "Azimuth: " << result["azimuth"] << std::endl;
```

## Summary

**Task Integration:**
- Python tasks inherit from `BaseScriptTask` → `Task`
- Execute within task system with lifecycle management
- Support dependencies, priorities, timeouts
- Integrated error handling and history tracking

**Execution Context:**
- Direct function calling via task methods
- Parameter validation and configuration
- Async execution support
- Script caching for performance

**Workflow Integration:**
- Chain Python tasks in sequences
- Manage dependencies automatically
- Monitor execution via history
- Handle errors gracefully with recovery options

**Best Practices:**
- Implement `get_tool_info()` for discoverability
- Use configuration files for complex setups
- Provide meaningful error messages
- Cache compiled scripts when possible
- Set appropriate timeouts for long-running tasks
