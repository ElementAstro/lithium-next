# Test Module - Shared Library for ModuleLoader Testing

This is a test shared library used to verify the functionality of the `ModuleLoader` component in the lithium-next project.

## Files

### test_module.hpp
Header file containing:
- `TestClass` - A simple test class that can be configured from JSON
  - Methods: `getName()`, `getValue()`, `setValue()`
- C-exported functions for dynamic loading:
  - `add(int a, int b)` - Simple addition
  - `multiply(int a, int b)` - Simple multiplication
  - `getVersion()` - Returns version string "1.0.0"
  - `createInstance(const json& config)` - Factory function returning std::shared_ptr
  - `createUniqueInstance(const json& config)` - Factory function returning std::unique_ptr

### test_module.cpp
Implementation of the test module functions and TestClass methods.

### CMakeLists.txt
Build configuration for compiling test_module as a shared library (.dll on Windows, .so on Unix).

## Build

The test module is built as part of the loader tests CMake configuration:

```bash
cd build
cmake ..
make
```

The compiled shared library will be copied to the test executable directory automatically via post-build commands.

## Usage

The test module is intended to be loaded by the ModuleLoader during testing, allowing verification of:
- Dynamic library loading and symbol resolution
- Function pointer invocation
- Configuration-based object instantiation
- Module lifecycle management (enable/disable/reload)
