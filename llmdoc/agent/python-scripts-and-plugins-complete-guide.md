# Python Scripts and Plugins - Complete Integration Guide

## Quick Start

### Loading a Python Script (Pure Python)

```bash
# Via HTTP API
curl -X POST http://localhost:8000/python/load \
  -H "Content-Type: application/json" \
  -d '{"script_name": "astronomy_tool", "alias": "astro"}'

# List available functions
curl http://localhost:8000/python/list

# Call a function
curl -X POST http://localhost:8000/python/call \
  -H "Content-Type: application/json" \
  -d '{"alias": "astro", "function_name": "calculate_altitude"}'
```

### Writing a Basic Python Tool

```python
# my_tool.py
def process_data(data: dict) -> dict:
    """Process input data and return result."""
    return {"status": "success", "processed": data}

def get_tool_info() -> dict:
    """Tool metadata for discovery."""
    return {
        "name": "my_tool",
        "version": "1.0.0",
        "description": "Basic data processing",
        "functions": ["process_data"]
    }
```

### Creating a Python Task

```cpp
// C++ Code
auto task = std::make_shared<PythonScriptTask>("my_task");
task->loadPythonModule("my_tool", "mytool");
task->execute(params);
```

## Architecture Summary

### Three Integration Layers

**Layer 1: Direct C++ API**
- Use `PythonWrapper` directly in C++ code
- Most control, requires C++ knowledge
- Type-safe function calling with templates

**Layer 2: HTTP REST API**
- Remote access to Python operations
- JSON request/response format
- Stateless, scalable architecture

**Layer 3: Task System Integration**
- Workflow automation with sequencing
- Dependency management
- Integrated error handling and history

### Key Components

| Component | Purpose | Use Case |
|-----------|---------|----------|
| PythonWrapper | Core Python execution engine | Direct C++ integration |
| PythonController | HTTP API endpoints | Remote/web access |
| PythonScriptTask | Task system integration | Automated workflows |
| ModuleLoader | Native .so/.dll loading | Compiled modules |
| ComponentManager | Module lifecycle management | Plugin system |

## Complete Reference

### 1. Python Integration System Architecture

**Location:** `python-integration-system-architecture.md`

Covers:
- PythonWrapper core functionality
- HTTP REST API layer
- Task system integration
- Global pointer injection pattern
- Performance features
- Type conversion mechanisms

### 2. Python Tools Interface Requirements

**Location:** `python-tools-interface-requirements.md`

Specifies:
- Pure Python tool requirements
- Type conversions (pybind11)
- Compiled module integration
- Discovery mechanisms
- Configuration patterns
- Error handling conventions

### 3. Module Loading and Discovery System

**Location:** `module-loading-and-discovery-system.md`

Details:
- Python script loading via import
- Native module loading from ./modules/
- Dependency resolution
- Module metadata format (module.json)
- Function/symbol extraction
- Component lifecycle management

### 4. pybind11 Integration and Type Conversion

**Location:** `pybind11-integration-and-type-conversion.md`

Documents:
- pybind11 header integration
- Automatic type conversions
- JSON-to-Python bridging
- GIL management for threading
- Variable and method access
- Exception handling
- Performance optimizations

### 5. Python Task System Integration

**Location:** `python-task-system-integration.md`

Explains:
- Task hierarchy and inheritance
- Execution flow and parameters
- Error handling and history tracking
- Configuration management
- Workflow sequencing
- Complete example implementation

## Critical Implementation Points

### Point 1: Initialization

```cpp
// In app.cpp
AddPtr<lithium::PythonWrapper>(
    Constants::PYTHON_WRAPPER,
    atom::memory::makeShared<lithium::PythonWrapper>());
```

The global PythonWrapper is created once and shared via global pointer injection.

### Point 2: Loading Scripts

```cpp
// Script loading happens with alias
python_wrapper->load_script("my_module", "myalias");

// Functions are called using alias + function name
double result = python_wrapper->call_function<double>(
    "myalias", "my_function");
```

### Point 3: Type Conversions

```cpp
// Automatic via pybind11
int i = 42;
py::object py_i = py::cast(i);  // C++ → Python

int back = py::cast<int>(py_i);  // Python → C++
```

### Point 4: GIL Management

```cpp
// RAII pattern handles GIL acquisition/release
{
    py::gil_scoped_acquire gil;
    // Call Python code here, GIL is held
}  // GIL automatically released
```

### Point 5: Error Handling

```cpp
try {
    python_wrapper->call_function<void>("alias", "func");
} catch (const std::runtime_error& e) {
    // Python exceptions converted to C++ std::runtime_error
    spdlog::error("Python error: {}", e.what());
}
```

## Common Patterns

### Pattern 1: Simple Function Call

```python
# my_calc.py
def add(a: int, b: int) -> int:
    return a + b
```

```cpp
// C++ Code
python_wrapper->load_script("my_calc", "calc");
int result = python_wrapper->call_function<int>(
    "calc", "add", 5, 3);  // returns 8
```

### Pattern 2: Dictionary Return

```python
# my_data.py
def get_status() -> dict:
    return {"status": "ready", "temperature": 25.5}
```

```cpp
// C++ Code
py::dict status = python_wrapper->call_function<py::dict>(
    "data", "get_status");
// Access: status["status"].cast<std::string>()
```

### Pattern 3: Object-Oriented Approach

```python
# my_class.py
class Calculator:
    def __init__(self):
        self.result = 0

    def add(self, a: int, b: int) -> int:
        self.result = a + b
        return self.result

    def get_result(self) -> int:
        return self.result
```

```cpp
// C++ Code
auto calc = python_wrapper->call_method(
    "myclass", "Calculator", "add", py::args(5, 3));
// Method invocation for stateful operations
```

### Pattern 4: Configuration-Based Tool

```python
# my_tool.py
class Tool:
    def __init__(self, config_path: str):
        self.config = self._load_config(config_path)

    def process(self, data):
        return {"result": data}  # Use config...

tool_instance = Tool("/path/to/config.json")

def process(data):
    return tool_instance.process(data)

def get_tool_info():
    return {"name": "my_tool", "version": "1.0.0"}
```

### Pattern 5: Async Task Execution

```cpp
// C++ Code - Async execution
std::future<int> future = python_wrapper->async_call_function<int>(
    "async_module", "long_running_function");

// Do other work...

int result = future.get();  // Wait for completion
```

## Performance Guidelines

### Caching Enabled by Default

```cpp
python_wrapper->configure_performance({
    .enable_caching = true,           // Cache results 1 hour
    .enable_threading = true,         // Use thread pool
    .enable_gil_optimization = true,  // Smart GIL handling
    .thread_pool_size = 4             // 4 worker threads
});
```

### For CPU-Intensive Tasks

```python
# my_calc.py - Offload heavy computation
def cpu_intensive(data: list) -> list:
    import numpy as np
    # NumPy uses C extensions, can release GIL
    result = np.array(data) * 2
    return result.tolist()
```

### For I/O-Bound Tasks

```python
# my_io.py - Use asyncio for I/O operations
async def fetch_data(url: str) -> dict:
    import aiohttp
    async with aiohttp.ClientSession() as session:
        async with session.get(url) as resp:
            return await resp.json()

def fetch_data_sync(url: str) -> dict:
    import asyncio
    return asyncio.run(fetch_data(url))
```

## Dependency Management

### Python Packages

Tools can require external packages. Use `get_tool_info()` to declare:

```python
def get_tool_info() -> dict:
    return {
        "name": "astronomy",
        "version": "1.0.0",
        "requirements": ["astropy>=4.0", "numpy>=1.18"],
        "functions": ["calculate"]
    }
```

Installation via API:

```bash
curl -X POST http://localhost:8000/python/installPackage \
  -d '{"package_name": "astropy"}'
```

### Native Module Dependencies

Specified in `modules/my_module/module.json`:

```json
{
    "name": "my_module",
    "dependencies": ["base_library", "device_driver"],
    "configFile": "./modules/my_module/config.json"
}
```

## Troubleshooting

### Issue: Module Not Found

**Cause:** Script not on Python path or module not yet loaded

**Solution:**
```cpp
// Add path to sys.path
python_wrapper->add_sys_path("/path/to/modules");

// Then load script
python_wrapper->load_script("module_name", "alias");
```

### Issue: Type Conversion Error

**Cause:** Python return type incompatible with C++ target type

**Solution:**
```cpp
// Wrong: trying to get list as int
// int result = python_wrapper->call_function<int>("mod", "func");

// Right: get as py::object, then convert
auto obj = python_wrapper->call_function<py::object>("mod", "func");
if (py::isinstance<py::list>(obj)) {
    auto list = py::cast<std::vector<int>>(obj);
}
```

### Issue: GIL Deadlock

**Cause:** Nested GIL acquisition in same thread

**Solution:**
```cpp
// pybind11 handles this automatically with scoped acquisition
// Just make sure not to hold other locks while holding GIL

// Bad:
std::lock_guard<std::mutex> lock(mutex_);
py::gil_scoped_acquire gil;  // Can deadlock
```

### Issue: Memory Leak

**Cause:** Python objects not properly cleaned up

**Solution:**
```cpp
// Objects cleaned automatically when py::object destroyed
{
    py::object obj = python_wrapper->eval_expression("mod", "expr");
    // Use obj...
}  // obj destroyed, memory freed

// For long-lived objects, use manage_object_lifecycle
python_wrapper->manage_object_lifecycle("mod", "obj_name", true);
```

## Advanced Topics

### Custom Data Binding with pybind11

For compiled modules that need C++ ↔ Python data passing:

```cpp
// In module binding
PYBIND11_MODULE(my_module, m) {
    pybind11::class_<MyData>(m, "MyData")
        .def("__init__", &MyData::init)
        .def("process", &MyData::process)
        .def_readonly("result", &MyData::result);
}
```

### Performance Profiling

```python
# my_tool.py
import cProfile
import pstats
from io import StringIO

def profile_function():
    pr = cProfile.Profile()
    pr.enable()
    # Your code here
    pr.disable()
    s = StringIO()
    ps = pstats.Stats(pr, stream=s).sort_stats('cumulative')
    ps.print_stats()
    return s.getvalue()
```

### Extending Task System

Create custom task types by inheriting from `Task`:

```cpp
class CustomPythonTask : public PythonScriptTask {
public:
    CustomPythonTask(const std::string& name)
        : PythonScriptTask(name) {}

    void setupCustomDefaults() {
        // Custom parameter setup
    }
};
```

## Security Considerations

### Input Validation

```python
# my_tool.py
def process(data: dict) -> dict:
    # Always validate input
    if not isinstance(data, dict):
        raise TypeError("Expected dict")

    if "required_field" not in data:
        raise ValueError("Missing required_field")

    return {"status": "ok"}
```

### Sandboxing

For untrusted Python code, consider:

```cpp
// Disable certain modules before loading
python_wrapper->inject_code("import sys; sys.modules['os'] = None");
python_wrapper->load_script("untrusted_script", "sandbox");
```

### Error Information Disclosure

Log full details internally, return sanitized errors to clients:

```json
{
    "status": "error",
    "code": 500,
    "error": "Operation failed"  // Don't expose internal details
}
```

## Summary of Documents

This guide references five detailed documents covering specific aspects:

1. **python-integration-system-architecture.md** - System design and layers
2. **python-tools-interface-requirements.md** - What tools must implement
3. **module-loading-and-discovery-system.md** - How modules are found and loaded
4. **pybind11-integration-and-type-conversion.md** - Type system and mechanics
5. **python-task-system-integration.md** - Workflow and automation support

Each document contains code sections with exact file paths and line numbers for reference.

## Key Takeaways

- **PythonWrapper** provides core Python execution; use it directly for maximum control
- **HTTP REST API** enables remote access; useful for web dashboards and services
- **Task System** provides workflow automation; use for complex sequences
- **Type Safety** via templates means compile-time error checking for common operations
- **GIL Management** handled automatically with RAII pattern
- **Error Handling** bridges Python and C++ exception systems
- **Discovery** via `get_tool_info()` convention makes tools self-documenting
- **Caching** enabled by default improves performance
- **Threading** via thread pool enables async execution within GIL limits
