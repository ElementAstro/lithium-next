# Lithium-Next CMake Build System

## Overview

The Lithium-Next project uses a modern, modular CMake build system designed for clarity, maintainability, and extensibility. This document describes the enhanced build system architecture and available utilities.

## Architecture

### Core Modules

The build system is organized around several core CMake modules located in the `cmake/` directory:

#### 1. **LithiumColors.cmake**
Provides colored and formatted output for better readability during configuration.

**Features:**
- Cross-platform color support (ANSI colors on Unix-like systems)
- Standardized message functions for different severity levels
- Formatted section headers and configuration output

**Key Functions:**
```cmake
lithium_message(TYPE MESSAGE)          # Colored status messages
lithium_print_header(HEADER_TEXT)      # Section headers
lithium_print_subsection(TEXT)         # Subsection headers
lithium_print_config(KEY VALUE)        # Key-value configuration output
lithium_print_build_config()           # Complete build summary
```

**Message Types:**
- `STATUS` - Standard informational messages (green)
- `WARNING` - Warning messages (yellow)
- `ERROR` - Error messages (red)
- `FATAL_ERROR` - Fatal error messages (red)
- `INFO` - Informational messages (cyan)
- `SUCCESS` - Success messages (green)

#### 2. **LithiumUtils.cmake**
Comprehensive utility functions for common CMake operations.

**Key Functions:**

##### Library Creation
```cmake
lithium_add_library(
    NAME <target_name>
    SOURCES <source_files>
    [HEADERS <header_files>]
    [LIBRARIES <libraries_to_link>]
    [INCLUDES <include_directories>]
    [VERSION <version>]
    [TYPE STATIC|SHARED|MODULE]
    [PUBLIC_HEADERS]
    [DESCRIPTION <description>]
)
```
Creates a standardized library with proper installation rules, include directories, and versioning.

##### Subdirectory Discovery
```cmake
lithium_add_subdirectories_recursive(
    START_DIR
    [PATTERN_FILE <filename>]
    [MESSAGE_PREFIX <prefix>]
)
```
Recursively discovers and adds subdirectories containing `CMakeLists.txt` files.

##### Package Discovery
```cmake
lithium_find_package(
    PACKAGE_NAME
    [VERSION <version>]
    [REQUIRED]
    [QUIET]
    [COMPONENTS <components>]
)
```
Finds external packages with enhanced reporting and colored output.

##### Component Management
```cmake
lithium_component_begin(COMPONENT_NAME VERSION)
lithium_component_end(COMPONENT_NAME)
```
Macros for standardized component initialization and completion.

##### Compiler Configuration
```cmake
lithium_apply_compiler_flags(TARGET_NAME)
lithium_apply_strict_flags(TARGET_NAME)
lithium_enable_lto(TARGET_NAME)
lithium_add_compiler_warnings(TARGET_NAME)
```

##### IDE Integration
```cmake
lithium_set_target_folder(TARGET_NAME FOLDER_NAME)
```
Organizes targets in IDE folder structures.

#### 3. **compiler_options.cmake**
Handles compiler detection, version checking, and flag management.

**Features:**
- Automatic C++23/C++20 standard detection
- Compiler version validation (GCC 13+, Clang 16+, MSVC 19.28+)
- Automatic fallback to newer compiler versions
- Platform-specific compiler configurations
- LTO (Link Time Optimization) support for Release builds

**Functions:**
```cmake
lithium_check_compiler_version()      # Validates compiler version
lithium_apply_compiler_flags(TARGET)  # Applies standard flags
lithium_apply_strict_flags(TARGET)    # Applies strict warning flags
```

#### 4. **policies.cmake**
Sets CMake policies for consistent behavior across different CMake versions.

**Current Policies:**
- `CMP0003` - Libraries linked by full-path must have a valid library file name
- `CMP0043` - Ignore COMPILE_DEFINITIONS_<Config> properties
- `CMP0148` - The FindPythonInterp and FindPythonLibs modules are removed

#### 5. **Find Modules**
Custom find modules for external dependencies:
- `FindCFITSIO.cmake` - CFITSIO library
- `FindGlib.cmake` - GLib library
- `FindINDI.cmake` - INDI astronomy framework
- `FindLibSecret-1.cmake` - libsecret
- `FindReadline.cmake` - Readline library
- `LibFindMacros.cmake` - Helper macros for find modules

## Project Structure

```
lithium-next/
├── cmake/                     # CMake modules and utilities
│   ├── LithiumColors.cmake   # Colored output functions
│   ├── LithiumUtils.cmake    # Utility functions
│   ├── compiler_options.cmake # Compiler configuration
│   ├── policies.cmake        # CMake policies
│   └── Find*.cmake           # Package find modules
├── src/                      # Source code modules
│   ├── components/           # Component management
│   ├── config/              # Configuration module
│   ├── database/            # Database ORM
│   ├── debug/               # Debugging utilities
│   ├── device/              # Device abstraction
│   ├── script/              # Scripting support
│   ├── server/              # Web server
│   ├── target/              # Target calculation
│   ├── task/                # Task management
│   ├── tools/               # Utility tools
│   └── client/              # Client libraries
├── modules/                  # Loadable modules
│   └── image/               # Image processing module
├── libs/                     # Third-party libraries (submodules)
│   ├── atom/                # Atom library (submodule)
│   └── thirdparty/          # Third-party deps (submodule)
├── tests/                    # Unit tests
└── example/                  # Example programs
```

## Build Configuration

### Standard Build Process

```bash
# Configure
cmake -B build -DCMAKE_BUILD_TYPE=Release

# Build
cmake --build build

# Install
cmake --install build --prefix /usr/local
```

### Build Types

- **Debug** - Full debug symbols, no optimization (default)
- **Release** - Full optimization, LTO enabled
- **RelWithDebInfo** - Optimization with debug symbols, LTO enabled
- **MinSizeRel** - Size optimization

### CMake Options

The build system automatically detects and configures:
- C++ standard (C++23 preferred, C++20 minimum)
- Compiler version and capabilities
- Available external dependencies
- Platform-specific requirements

### Environment Variables

- `CMAKE_BUILD_TYPE` - Build configuration type
- `CMAKE_INSTALL_PREFIX` - Installation directory
- `CMAKE_CXX_COMPILER` - C++ compiler to use
- `CMAKE_C_COMPILER` - C compiler to use

## Component Development

### Creating a New Component

Use the standardized template for new components:

```cmake
# ============================================================================
# CMakeLists.txt for Lithium-YourComponent
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: lithium_yourcomponent
# Description: Brief description of the component
# Author: Your Name
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_yourcomponent 1.0.0)

# ============================================================================
# Dependencies
# ============================================================================

lithium_find_package(SomeDependency REQUIRED)

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    file1.cpp
    file2.cpp
)

set(PROJECT_HEADERS
    file1.hpp
    file2.hpp
)

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    atom
    lithium_config
    SomeDependency::SomeDependency
)

# ============================================================================
# Build Library
# ============================================================================

lithium_add_library(
    NAME ${PROJECT_NAME}
    SOURCES ${PROJECT_SOURCES} ${PROJECT_HEADERS}
    LIBRARIES ${PROJECT_LIBRARIES}
    VERSION ${PROJECT_VERSION}
    TYPE STATIC
    DESCRIPTION "Brief description"
    PUBLIC_HEADERS
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/YourComponent")

lithium_component_end(${PROJECT_NAME})
```

### Best Practices

1. **Always use utility functions** - Use `lithium_add_library()` instead of raw `add_library()`
2. **Consistent formatting** - Follow the section-based structure with clear separators
3. **Descriptive messages** - Use appropriate message types for clarity
4. **Version management** - Specify versions for all components
5. **IDE organization** - Use `lithium_set_target_folder()` for logical grouping
6. **Documentation** - Include clear comments explaining non-obvious configurations

### Common Patterns

#### Finding Optional Dependencies
```cmake
lithium_find_package(OptionalDep)
if(OptionalDep_FOUND)
    list(APPEND PROJECT_LIBRARIES OptionalDep::OptionalDep)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_OPTIONALDEP)
endif()
```

#### Platform-Specific Code
```cmake
if(WIN32)
    list(APPEND PROJECT_SOURCES windows_specific.cpp)
    list(APPEND PROJECT_LIBRARIES ws2_32)
elseif(UNIX)
    list(APPEND PROJECT_SOURCES unix_specific.cpp)
    list(APPEND PROJECT_LIBRARIES pthread)
endif()
```

#### Conditional Features
```cmake
option(ENABLE_FEATURE_X "Enable feature X" ON)
if(ENABLE_FEATURE_X)
    target_compile_definitions(${PROJECT_NAME} PRIVATE FEATURE_X_ENABLED)
endif()
```

## Output and Logging

The enhanced build system provides clear, organized output:

```
================================================================================
  Lithium-Next: Open Astrophotography Terminal
================================================================================
[Lithium] Version: 1.0.0
[Lithium] Author: Max Qian
[Lithium] License: GPL3

--- Project Directories ---
  Source Directory: /path/to/lithium-next/src
  Third-party Libraries: /path/to/lithium-next/libs/thirdparty
  Atom Library: /path/to/lithium-next/libs/atom

[Lithium] Build Type: Release
[Lithium Success] Using C++23 standard
--- Compiler Detection ---
[Lithium Success] Using g++ version 13.2.0

--- Detecting External Libraries ---
[Lithium Info] Searching for package: Python
[Lithium Success] Found Python
  Version: 3.11.5
...
```

## Installation

The build system provides comprehensive installation rules:

- **Executables** → `${CMAKE_INSTALL_BINDIR}` (typically `/usr/local/bin`)
- **Libraries** → `${CMAKE_INSTALL_LIBDIR}` (typically `/usr/local/lib`)
- **Headers** → `${CMAKE_INSTALL_INCLUDEDIR}` (typically `/usr/local/include`)

## IDE Integration

### Visual Studio Code
The build system automatically configures:
- Folder grouping in the CMake Tools extension
- IntelliSense integration
- Debug configurations

### CLion
Full CMake integration with:
- Project structure tree
- Target organization
- Run/Debug configurations

### Visual Studio
MSVC-specific configurations:
- Solution folders
- Project grouping
- Optimal compiler flags

## Troubleshooting

### Compiler Not Found
The build system automatically searches for compatible compilers. If your compiler isn't detected:

```bash
cmake -B build -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_C_COMPILER=gcc-13
```

### Missing Dependencies
All required dependencies are reported during configuration with clear error messages:

```
[Lithium Error] SomeDependency not found!
```

Install the missing dependency and reconfigure.

### Build Errors
The standardized structure makes it easy to identify problematic components:
- Check the component's CMakeLists.txt
- Verify all source files are listed
- Ensure all dependencies are correctly linked

## Contributing

When adding new components or modifying the build system:

1. Follow the established patterns and templates
2. Use the utility functions provided
3. Add appropriate error checking and messages
4. Test on multiple platforms if possible
5. Update this README if adding new functionality

## Version History

### Version 2.0 (Current)
- Introduced modular CMake utilities (LithiumColors, LithiumUtils)
- Standardized all component CMakeLists.txt files
- Enhanced compiler detection and version validation
- Added colored, formatted output
- Implemented comprehensive installation rules
- Improved IDE integration

### Version 1.0 (Legacy)
- Basic CMake configuration
- Manual subdirectory management
- Limited output formatting

## License

This build system is part of the Lithium-Next project and is licensed under GPL3.

## Support

For issues or questions about the build system:
- Check this README first
- Review the CMake utility functions in `cmake/LithiumUtils.cmake`
- Consult existing component CMakeLists.txt files for examples
- Open an issue on the project repository
