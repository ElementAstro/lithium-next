# pybind11 Integration and Type Conversion

## Overview

Lithium-Next uses pybind11 as the core mechanism for C++ ↔ Python interoperability. This document details how pybind11 is integrated into the system and how type conversions work.

## pybind11 in the Codebase

### Header Includes

**Code Section: pybind11 Integration Setup**

**File:** `src/script/python_caller.hpp`
**Lines:** 1-23

```cpp
#include <pybind11/embed.h>      // Embedding Python in C++
#include <pybind11/iostream.h>   // Python I/O stream integration
#include <pybind11/pybind11.h>   // Core pybind11 functionality
#include <pybind11/stl.h>        // STL container conversion

namespace py = pybind11;
```

These headers enable:

- **embed.h**: Initializing and managing Python interpreter within C++ application
- **iostream.h**: Redirecting Python print statements to C++ streams
- **pybind11.h**: Core Python object manipulation
- **stl.h**: Automatic conversion of STL containers (std::vector, std::map, etc.)

### Python Interpreter Initialization

**Code Section: Scoped Interpreter**

**File:** `src/script/python_caller.cpp`
**Lines:** 239-240

```cpp
// In PythonWrapper::Impl class
py::scoped_interpreter guard_;
std::unordered_map<std::string, py::module> scripts_;
```

**Key Details:**

- `py::scoped_interpreter guard_` manages Python interpreter lifetime
- Created during Impl constructor, destroyed during destructor
- Ensures proper GIL initialization/cleanup
- Handles Python startup/shutdown automatically

### Type Conversion Mechanism

#### Automatic Conversions via pybind11

**Code Section: pybind11 Cast Operations**

**File:** `src/script/python_caller.hpp`
**Lines:** 30-34

```cpp
template <typename T>
concept PythonConvertible = requires(T a) {
    { py::cast(a) } -> std::convertible_to<py::object>;
    { py::cast<T>(py::object()) } -> std::same_as<T>;
};
```

This concept ensures types are convertible both directions:

**C++ → Python conversions:**

```cpp
py::object py_val = py::cast(cpp_value);

// Examples:
int i = 42;
py::object py_i = py::cast(i);  // → Python int

std::string s = "hello";
py::object py_s = py::cast(s);  // → Python str

std::vector<int> vec = {1, 2, 3};
py::object py_list = py::cast(vec);  // → Python list
```

**Python → C++ conversions:**

```cpp
T cpp_value = py::cast<T>(py_object);

// Examples:
int i = py::cast<int>(py_int_obj);
std::string s = py::cast<std::string>(py_str_obj);
std::vector<int> vec = py::cast<std::vector<int>>(py_list_obj);
```

#### JSON to Python Conversion

**Code Section: JSON-to-Python Bridge**

**File:** `src/server/controller/python.hpp`
**Lines:** 39-70

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

**Supported Conversions:**

| JSON Type | Python Type |
|-----------|-------------|
| String | str |
| Number (integer) | int |
| Number (float) | float |
| true | True |
| false | False |
| Array | list |
| Object | dict |
| null | None |

#### Template-Based Type Deduction

**Code Section: Function Return Type Handling**

**File:** `src/script/python_caller.cpp`
**Lines:** 105-138

```cpp
template <typename ReturnType, typename... Args>
ReturnType call_function(const std::string& alias,
                        const std::string& function_name, Args... args) {
    try {
        auto iter = scripts_.find(alias);
        if (iter == scripts_.end()) {
            throw std::runtime_error("Alias '" + alias + "' not found");
        }

        py::object result = iter->second.attr(function_name.c_str())(args...);
        return result.cast<ReturnType>();  // Type conversion happens here
    } catch (const py::error_already_set& e) {
        throw std::runtime_error("Error calling function: " +
                                std::string(e.what()));
    }
}
```

**Explicit Template Instantiation:**

```cpp
// In python_caller.cpp (lines 273-278)
template int PythonWrapper::call_function<int>(
    const std::string&, const std::string&);

template std::string PythonWrapper::call_function<std::string>(
    const std::string&, const std::string&);

template double PythonWrapper::call_function<double>(
    const std::string&, const std::string&);
```

Only common return types are instantiated to reduce binary size.

## Python Object Wrapping

### py::object Class

**Code Section: Generic Python Object Handling**

**File:** `src/script/python_caller.hpp`
**Lines:** 308-324

```cpp
template <typename T>
py::object cpp_to_python(const T& value) {
    return py::cast(value);
}

template <typename T>
T python_to_cpp(const py::object& obj) {
    return py::cast<T>(obj);
}
```

The `py::object` type represents any Python object and can hold:

- Primitive types (int, float, str, bool)
- Collections (list, dict, tuple, set)
- Custom objects
- None/null values

## GIL Management via pybind11

### GIL Concept

The Global Interpreter Lock (GIL) in Python prevents true parallelism in multi-threaded code. pybind11 provides utilities for managing it.

**Code Section: GIL Manager RAII**

**File:** `src/script/python_caller.hpp`
**Lines:** 483-498

```cpp
class GILManager {
public:
    GILManager() : gil_state_(PyGILState_Ensure()) {}
    ~GILManager() { PyGILState_Release(gil_state_); }

private:
    PyGILState_STATE gil_state_;
};
```

**Usage in Thread Pool:**

```cpp
void thread_worker(std::stop_token stop_token) {
    while (!stop_token.stop_requested()) {
        // Acquire lock, execute task
        std::unique_lock<std::mutex> lock(queue_mutex_);
        // Wait for task...

        try {
            py::gil_scoped_acquire gil;  // Acquire GIL
            task();                       // Execute Python function
        } catch (const std::exception& e) {
            // Handle error
        }
    }  // GIL released automatically here
}
```

### Configuration

**Code Section: GIL Optimization**

**File:** `src/script/python_caller.hpp`
**Lines:** 262-267

```cpp
struct PerformanceConfig {
    bool enable_threading;         // Use thread pool
    bool enable_gil_optimization;  // Smart GIL handling
    size_t thread_pool_size;       // Number of worker threads
    bool enable_caching;           // Cache function results
};
```

## Variable and Attribute Access

### Direct Variable Access

**Code Section: Variable Get/Set**

**File:** `src/script/python_caller.hpp`
**Lines:** 95-106

```cpp
template <typename T>
T get_variable(const std::string& alias, const std::string& variable_name) {
    // Retrieves Python variable and converts to C++ type
}

void set_variable(const std::string& alias,
                  const std::string& variable_name,
                  const py::object& value) {
    // Sets Python variable from py::object
}
```

### Object Attribute Access

**Code Section: Object Attribute Operations**

**File:** `src/script/python_caller.hpp`
**Lines:** 136-151

```cpp
template <typename T>
T get_object_attribute(const std::string& alias,
                       const std::string& class_name,
                       const std::string& attr_name) {
    // Get class instance attribute and convert
}

void set_object_attribute(const std::string& alias,
                         const std::string& class_name,
                         const std::string& attr_name,
                         const py::object& value) {
    // Set class instance attribute
}
```

## Method Invocation

### Class Method Calling

**Code Section: Python Method Invocation**

**File:** `src/script/python_caller.hpp`
**Lines:** 123-126

```cpp
py::object call_method(const std::string& alias,
                       const std::string& class_name,
                       const std::string& method_name,
                       const py::args& args) {
    // Invoke method with variable arguments
}
```

**Implementation Pattern:**

```cpp
py::object cls = scripts_[alias].attr(class_name.c_str());
py::object instance = cls();  // Create instance
py::object result = instance.attr(method_name.c_str())(*args);
return result;
```

## Expression Evaluation

**Code Section: Dynamic Code Evaluation**

**File:** `src/script/python_caller.hpp`
**Lines:** 159-160

```cpp
py::object eval_expression(const std::string& alias,
                           const std::string& expression) {
    // Evaluate arbitrary Python expression
}
```

Enables dynamic computation like:

```cpp
python_wrapper->eval_expression("astro",
    "import math; math.sin(0.5)");
```

## Exception Handling

### Exception Bridging

**Code Section: Python Exception Handling**

**File:** `src/script/python_caller.cpp`
**Lines:** 282-288

```cpp
static void handle_exception(const py::error_already_set& e) {
    spdlog::error("Python Exception: {}", e.what());

    py::module_ traceback = py::module_::import("traceback");
    py::object tb = traceback.attr("format_exc")();
    spdlog::error("Traceback: {}", py::cast<std::string>(tb));
}
```

**Exception Conversion:**

- Python exceptions caught as `py::error_already_set`
- Converted to `std::runtime_error` for C++ code
- Full traceback logged for debugging

## pybind11 Module Integration

### Existing Tool Example: Git Utils

**Code Section: Git Utilities pybind11 Adapter**

**File:** `python/tools/git_utils/pybind_adapter.py`
**Lines:** 18-55

```python
class GitUtilsPyBindAdapter:
    """Adapter class to expose GitUtils functionality via pybind11."""

    @staticmethod
    def clone_repository(repo_url: str, clone_dir: str) -> bool:
        """Simplified clone operation for C++ binding."""
        git = GitUtils()
        result = git.clone_repository(repo_url, clone_dir)
        return result.success

    @staticmethod
    def add_and_commit(repo_dir: str, message: str) -> bool:
        """Combined operation for C++ binding."""
        git = GitUtils(repo_dir)
        # Simplified interface for C++ binding
```

This pattern shows:

1. Adapter class wraps C++ functionality
2. Methods simplified for pybind11 compatibility
3. Return types are basic (bool, str, dict)
4. Error handling via exceptions

### Binding Code Generation

**Code Section: Binding Code Generation**

**File:** `python/tools/pacman_manager/pybind_integration.py`
**Lines:** 23-42

```python
@staticmethod
def generate_bindings() -> str:
    """Generate C++ code for pybind11 bindings."""
    return """
// pacman_bindings.cpp - pybind11 bindings for PacmanManager
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>

PYBIND11_MODULE(pacman_manager, m) {
    // Expose PacmanManager class and methods
    // Example:
    // pybind11::class_<PacmanManager>(m, "PacmanManager")
    //     .def("install", &PacmanManager::install)
    //     .def("remove", &PacmanManager::remove);
}
"""
```

## Performance Optimizations

### Function Caching

**Code Section: Result Caching**

**File:** `src/script/python_caller.cpp`
**Lines:** 117-135

```cpp
if (config_.enable_caching) {
    std::string cache_key = alias + "::" + function_name;
    auto cache_it = function_cache_.find(cache_key);
    if (cache_it != function_cache_.end()) {
        cache_it->second.update_access();
        return cache_it->second.cached_result.cast<ReturnType>();
    }
}

// ... execute function ...

if (config_.enable_caching) {
    function_cache_.emplace(cache_key, CacheEntry(result));
}
```

**Cache Expiration:** 1 hour TTL (configurable via `cache_timeout_`)

### Memory Pool

**Code Section: Memory Pool for Python Objects**

**File:** `src/script/python_caller.cpp`
**Lines:** 43-43

```cpp
static thread_local std::pmr::synchronized_pool_resource memory_pool;
```

Uses polymorphic memory resource (PMR) for efficient allocation.

## Summary

**pybind11 Integration:**
- Embeds Python interpreter with `py::scoped_interpreter`
- Automatic type conversion via `py::cast<T>()`
- Template-based function calling with type deduction
- GIL management via RAII `GILManager`
- Exception bridging between Python and C++
- JSON-to-Python conversion for HTTP API

**Type Support:**
- Primitives: int, float, bool, str
- Collections: list, dict, tuple, set (via `#include <pybind11/stl.h>`)
- Custom objects: via `py::object` generic wrapper
- Template instantiation for common return types

**Performance:**
- Function result caching (1-hour TTL)
- Thread pool for async execution
- Memory pooling for allocations
- Smart GIL management in worker threads
