# Python Tools Interface Requirements

## Overview

Python tools in Lithium-Next can be integrated in two ways: as pure Python modules (loaded via PythonWrapper) or as compiled C++ modules with pybind11 adapters. This document specifies the interface requirements for both.

## Pure Python Tool Integration (PythonWrapper)

### Basic Requirements

To make a Python tool loadable and callable via PythonWrapper:

1. **Module Structure**: Create a Python module (file or package)
2. **Function Naming**: Expose functions that PythonWrapper can discover and call
3. **Type Compatibility**: Use types that pybind11 can convert (strings, numbers, lists, dicts)
4. **Error Handling**: Raise standard Python exceptions for error cases

### Example: Simple Python Tool

```python
# my_astronomy_tool.py

def calculate_altitude(ra: float, dec: float, lat: float, ha: float) -> float:
    """Calculate altitude given coordinates and hour angle."""
    import math
    sin_alt = (math.sin(dec * math.pi/180) * math.sin(lat * math.pi/180) +
               math.cos(dec * math.pi/180) * math.cos(lat * math.pi/180) *
               math.cos(ha * math.pi/180))
    return math.asin(sin_alt) * 180 / math.pi

def get_tool_info() -> dict:
    """Return metadata about this tool."""
    return {
        "name": "astronomy_calculator",
        "version": "1.0.0",
        "description": "Basic astronomical calculations",
        "functions": ["calculate_altitude"]
    }
```

### Loading and Calling

From C++ via PythonWrapper:

```cpp
// Load script with alias
python_wrapper->load_script("my_astronomy_tool", "astro");

// Call function
double altitude = python_wrapper->call_function<double>(
    "astro", "calculate_altitude", 45.5, 30.2, 40.0, 2.5);

// Get metadata
auto info = python_wrapper->call_function<py::dict>(
    "astro", "get_tool_info");
```

From HTTP REST API:

```bash
# Load
curl -X POST http://localhost:8000/python/load \
  -H "Content-Type: application/json" \
  -d '{"script_name": "my_astronomy_tool", "alias": "astro"}'

# Call function
curl -X POST http://localhost:8000/python/call \
  -H "Content-Type: application/json" \
  -d '{
    "alias": "astro",
    "function_name": "calculate_altitude"
  }'
```

### Interface Convention: get_tool_info()

While not required, tools should implement a `get_tool_info()` function returning metadata:

```python
def get_tool_info() -> dict:
    return {
        "name": str,                # Tool identifier
        "version": str,             # Semantic version
        "description": str,         # One-line description
        "author": str,              # Tool author
        "license": str,             # License type
        "functions": list[str],     # Exported function names
        "requirements": list[str],  # Python package dependencies
        "capabilities": list[str]   # Feature flags
    }
```

### Supported Type Conversions

pybind11 automatically converts between C++ and Python:

```
C++ int          ↔ Python int
C++ double       ↔ Python float
C++ bool         ↔ Python bool
C++ std::string  ↔ Python str
C++ std::vector  ↔ Python list
C++ std::map     ↔ Python dict
C++ nullptr      ↔ Python None
```

## Compiled Module Integration (C++ with pybind11)

### Purpose

For performance-critical tools or when leveraging existing C++ libraries, create compiled modules exposed via pybind11.

### Example: Git Utilities Adapter Pattern

**File:** `python/tools/git_utils/pybind_adapter.py`

```python
class GitUtilsPyBindAdapter:
    """Adapter exposing C++ functionality to Python via pybind11."""

    @staticmethod
    def clone_repository(repo_url: str, clone_dir: str) -> bool:
        """Simplified interface for C++ binding."""
        git = GitUtils()  # Calls C++ GitUtils via pybind11
        result = git.clone_repository(repo_url, clone_dir)
        return result.success

    @staticmethod
    def get_repository_status(repo_dir: str) -> dict:
        """Return structured status data."""
        git = GitUtils(repo_dir)
        result = git.view_status(porcelain=True)
        return {
            "success": result.success,
            "is_clean": result.success and not result.output.strip(),
            "output": result.output
        }
```

### Key Pattern: Simplification Layer

Pybind11 adapters should:

1. **Simplify C++ APIs**: Convert complex C++ types to Python-friendly types
2. **Handle Errors**: Convert C++ exceptions to Python exceptions
3. **Type Conversions**: Ensure proper conversion between C++ and Python types
4. **Logging**: Use Python's logging for observability

### Example: Pacman Manager Integration

**File:** `python/tools/pacman_manager/pybind_integration.py`

```python
class Pybind11Integration:
    """Helper for pybind11 integration."""

    @staticmethod
    def check_pybind11_available() -> bool:
        """Verify pybind11 is available."""
        import importlib.util
        return importlib.util.find_spec("pybind11") is not None

    @staticmethod
    def generate_bindings() -> str:
        """Generate C++ pybind11 binding code."""
        if not Pybind11Integration.check_pybind11_available():
            raise ImportError("pybind11 is not installed")

        # Return boilerplate C++ code for bindings
        return """
// pacman_bindings.cpp - pybind11 bindings
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

PYBIND11_MODULE(pacman_manager, m) {
    // Expose PacmanManager class and methods
}
"""
```

## Component System Integration (Native Modules)

### When to Use

For tools requiring:
- Maximum performance
- Complex C++ dependencies
- Direct system access
- Library linkage

### Registration Requirements

Native modules (compiled .so/.dll files) loaded via ModuleLoader need:

1. **Entry Point Function**: Symbol exported from shared library
2. **Module Metadata**: Version, dependencies, configuration
3. **Module Loading Symbol**: Function returning module instance

Example module structure:

```cpp
// my_module.cpp
#include "atom/components/component.hpp"

extern "C" {
    std::shared_ptr<Component> CreateModule(const json& config) {
        return std::make_shared<MyModule>(config);
    }
}

class MyModule : public Component {
    bool initialize() override { ... }
    bool finalize() override { ... }
    bool handle_event(const Event&) override { ... }
};
```

### Module Metadata

**File:** `modules/my_module/module.json`

```json
{
    "name": "my_module",
    "version": "1.0.0",
    "description": "Module description",
    "type": "native",
    "author": "Author Name",
    "license": "AGPL-3",
    "path": "./modules/my_module/my_module.so",
    "dependencies": ["base_module"],
    "configFile": "./modules/my_module/config.json",
    "enabled": true
}
```

## Execution Contexts

### Context 1: Direct Script Execution

```python
# my_script.py
result = perform_calculation()
```

Load and call via HTTP:

```bash
curl -X POST http://localhost:8000/python/call \
  -d '{"alias": "myscript", "function_name": "perform_calculation"}'
```

### Context 2: Task Sequence

Tools integrate with task system via PythonScriptTask:

```cpp
auto task = manager->createComponent<PythonScriptTask>("calc_task");
task->loadPythonModule("my_astronomy_tool", "astro");
task->execute(params);
```

### Context 3: Event Handlers

Python tools can be registered as event handlers via:

```cpp
// Pseudo-code
python_wrapper->register_function("on_device_connected",
    [python_wrapper]() {
        python_wrapper->call_function<void>("handlers", "handle_connect");
    });
```

## Discovery Mechanism

### Automatic Discovery

PythonWrapper discovers loaded modules via introspection:

```cpp
std::vector<std::string> functions =
    python_wrapper->get_function_list("module_alias");
```

### Convention-Based Discovery

Tools following the `get_tool_info()` convention are discoverable:

```bash
curl http://localhost:8000/python/call \
  -d '{"alias": "tool", "function_name": "get_tool_info"}'
```

## Error Handling

### Python Exception Handling

```python
def my_function():
    """Function that may raise exceptions."""
    try:
        # Potentially failing code
        pass
    except Exception as e:
        raise ValueError(f"Tool error: {e}")  # Converted to C++ exception
```

### C++ Side Exception Handling

```cpp
try {
    python_wrapper->call_function<void>("module", "my_function");
} catch (const py::error_already_set& e) {
    spdlog::error("Python error: {}", e.what());
    // Handle error
}
```

### HTTP Error Response

```json
{
    "status": "error",
    "code": 500,
    "error": "Python error details"
}
```

## Configuration Example

For a complete Python tool with configuration:

```python
# my_tool.py
import json
from pathlib import Path

class MyTool:
    def __init__(self, config_path: str = None):
        self.config = self._load_config(config_path)

    def _load_config(self, config_path: str) -> dict:
        if not config_path or not Path(config_path).exists():
            return {"debug": False, "timeout": 30}

        with open(config_path) as f:
            return json.load(f)

    def process(self, data: dict) -> dict:
        """Main processing function."""
        return {"result": "processed", "input": data}

    def get_tool_info(self) -> dict:
        return {
            "name": "my_tool",
            "version": "1.0.0",
            "description": "Custom processing tool",
            "functions": ["process"]
        }

# Global instance for HTTP access
_instance = MyTool()

def process(data: dict) -> dict:
    return _instance.process(data)

def get_tool_info() -> dict:
    return _instance.get_tool_info()
```

## Summary

Python tools integrate through:

1. **Direct Script Loading**: PythonWrapper loads and executes Python modules
2. **Function Calling**: Type-safe function invocation via templates
3. **Variable Access**: Get/set Python variables from C++
4. **HTTP API**: Remote access to all Python operations
5. **Task Integration**: Execution within automation workflows
6. **pybind11 Adapters**: Wrapping C++ modules for Python access

Minimal requirements: Expose functions with pybind11-compatible signatures and follow the convention of including `get_tool_info()` for discoverability.
