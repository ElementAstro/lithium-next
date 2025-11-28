# Test Module Setup - lithium-next ModuleLoader Testing

## Overview

A complete test shared library has been created for testing the `ModuleLoader` component in lithium-next. The test module provides:

- A simple `TestClass` for demonstrating class instantiation from JSON configuration
- Exported C functions for dynamic symbol resolution testing
- Platform-independent shared library compilation (DLL on Windows, .so on Unix)
- Automatic build integration with CMake

## Created Files

### 1. `tests/components/loader/test_module/test_module.hpp`
Header file with:
- `TestClass` definition with JSON configuration support
- C-exported function declarations:
  - `add(int, int)` - Returns sum of two integers
  - `multiply(int, int)` - Returns product of two integers
  - `getVersion()` - Returns "1.0.0"
  - `createInstance(const json&)` - Creates std::shared_ptr<TestClass>
  - `createUniqueInstance(const json&)` - Creates std::unique_ptr<TestClass>

### 2. `tests/components/loader/test_module/test_module.cpp`
Implementation of:
- TestClass constructor with JSON-based configuration
- TestClass methods: getName(), getValue(), setValue()
- All exported C functions

### 3. `tests/components/loader/test_module/CMakeLists.txt`
Build configuration for:
- Creating a shared library target
- Linking against atom library and nlohmann_json
- Platform-specific output naming (test_module.dll on Windows, test_module.so on Unix)
- C++20 standard compliance

### 4. `tests/components/loader/test_module/README.md`
Detailed documentation of the test module functionality and usage.

### 5. Updated `tests/components/loader/CMakeLists.txt`
Added:
- `add_subdirectory(test_module)` to build the test module as part of the test suite
- `add_dependencies()` to ensure test module is built before loader tests
- `add_custom_command()` to automatically copy the compiled module to the test executable directory

## Build Integration

The test module is automatically built and deployed when running:

```bash
cd build
cmake ..
make
```

The compiled shared library will be:
- Created in the build directory
- Automatically copied to the test executable output directory
- Available for the ModuleLoader tests to dynamically load

## Usage in Tests

The test_module_loader.cpp can now:

1. Load the test module using ModuleLoader
2. Verify symbol resolution for exported functions
3. Test JSON-based object instantiation
4. Verify module lifecycle operations (enable/disable/reload)
5. Test error handling for missing symbols or load failures

## Key Features

- **Platform Independent**: Works on Windows (DLL), Linux, and macOS (.so)
- **JSON Support**: Uses nlohmann_json for configuration-based instantiation
- **C Export**: Uses extern "C" for dynamic symbol resolution testing
- **CMake Integration**: Seamlessly integrates with the project's build system
- **Automatic Deployment**: Post-build commands ensure the module is available for tests

## Testing Capabilities

This test module enables testing of:
- Dynamic library loading and unloading
- Function pointer invocation through dlopen/dlsym
- Module information querying
- Dependency management
- Module enable/disable functionality
- Module reloading
- Error handling for invalid module paths

## Future Enhancements

- Add more complex test classes for advanced testing scenarios
- Include test data structures for serialization/deserialization testing
- Add callback functions for testing event-driven module interactions
- Create multiple test modules with different dependency chains
