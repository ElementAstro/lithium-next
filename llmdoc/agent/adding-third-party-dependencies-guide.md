# Adding Third-Party Dependencies to Lithium-Next CMake Build System

## Evidence Section

<CodeSection>

## Code Section: External Library Detection Pattern - Root CMakeLists.txt

**File:** `/d/Project/lithium-next/CMakeLists.txt`
**Lines:** 68-83
**Purpose:** Shows current pattern for detecting and adding external dependencies

```cmake
# ============================================================================
# External Library Detection
# ============================================================================

lithium_print_subsection("Detecting External Libraries")

# Core compression libraries
find_library(LIBZ_LIBRARY NAMES z PATHS /usr/lib/x86_64-linux-gnu /opt/conda/lib)
find_library(LIBBZ2_LIBRARY NAMES bz2 PATHS /usr/lib/x86_64-linux-gnu /opt/conda/lib)

if(LIBZ_LIBRARY)
    lithium_message(SUCCESS "Found libz: ${LIBZ_LIBRARY}")
endif()

if(LIBBZ2_LIBRARY)
    lithium_message(SUCCESS "Found libbz2: ${LIBBZ2_LIBRARY}")
endif()

# Python and bindings
set(PYBIND11_FINDPYTHON ON)
lithium_find_package(Python VERSION 3.7 COMPONENTS Interpreter Development REQUIRED)
lithium_find_package(pybind11 REQUIRED)

# Terminal libraries
lithium_find_package(Readline REQUIRED)
lithium_find_package(Curses REQUIRED)

message(STATUS "")
```

**Key Details:**

- Uses `find_library()` for simple native libraries with fallback paths
- Uses `lithium_find_package()` wrapper for complex CMake packages
- REQUIRED flag controls whether missing dependencies fail configuration
- Optional dependencies can be detected separately with conditional linking

</CodeSection>

<CodeSection>

## Code Section: Optional Dependency Pattern in Components

**File:** `/d/Project/lithium-next/cmake/README.md`
**Lines:** 253-260
**Purpose:** Documented pattern for optional dependencies in component CMakeLists.txt

```cmake
# Finding Optional Dependencies
lithium_find_package(OptionalDep)
if(OptionalDep_FOUND)
    list(APPEND PROJECT_LIBRARIES OptionalDep::OptionalDep)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_OPTIONALDEP)
endif()
```

**Key Details:**

- Package can be found without REQUIRED flag
- Conditional linking through list append
- Compile definitions enable feature flags in source code
- Pattern allows graceful degradation when dependency unavailable

</CodeSection>

<CodeSection>

## Code Section: Component Library Dependency Configuration

**File:** `/d/Project/lithium-next/src/config/CMakeLists.txt`
**Lines:** 24-35
**Purpose:** Shows how component declares its dependencies

```cmake
# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    loguru
    ${CMAKE_THREAD_LIBS_INIT}
    atom
)

# Platform-specific libraries
if(WIN32)
    list(APPEND PROJECT_LIBRARIES ws2_32)
endif()
```

**Key Details:**

- Dependencies listed as CMake variable
- Platform-specific linking added conditionally
- Variables like `${CMAKE_THREAD_LIBS_INIT}` abstract platform differences
- Passed to `lithium_add_library()` LIBRARIES parameter

</CodeSection>

<CodeSection>

## Code Section: Loguru Optional Dependency Integration

**File:** `/d/Project/lithium-next/libs/atom/atom/log/CMakeLists.txt`
**Lines:** 85-110
**Purpose:** Demonstrates conditional linking of optional fmt dependency

```cmake
# --- import and link fmt (if needed)
# ----------------------------------------------------------

if (LOGURU_USE_FMTLIB)

  message(STATUS "linking to fmt")

  if (NOT TARGET fmt::fmt) # only search if not already found in parent scope
    find_package(fmt CONFIG REQUIRED)
  endif()

  if (LOGURU_FMT_HEADER_ONLY)
    target_link_libraries(loguru PUBLIC fmt::fmt-header-only)
  else()
    target_link_libraries(loguru PUBLIC fmt::fmt)
  endif()

  message(STATUS "linking to fmt - done")

endif()
```

**Key Details:**

- Feature can be enabled/disabled via CMake option
- Checks if target already exists to avoid duplicate searches
- Supports different linking modes (header-only vs shared library)
- Message output provides transparency during configuration

</CodeSection>

<CodeSection>

## Code Section: libspng Third-Party Library Pattern

**File:** `/d/Project/lithium-next/libs/thirdparty/libspng/CMakeLists.txt`
**Lines:** 1-50
**Purpose:** Complete example of third-party library with options and dependencies

```cmake
cmake_minimum_required(VERSION 3.12)

project(libspng C)

set(CMAKE_C_STANDARD 99)

set(SPNG_MAJOR 0)
set(SPNG_MINOR 7)
set(SPNG_REVISION 4)
set(SPNG_VERSION ${SPNG_MAJOR}.${SPNG_MINOR}.${SPNG_REVISION})

# Define build options
option(ENABLE_OPT "Enable architecture-specific optimizations" ON)
option(SPNG_SHARED "Build shared lib" ON)
option(SPNG_STATIC "Build static lib" ON)
option(BUILD_EXAMPLES "Build examples" OFF)

# Source files
set(spng_SOURCES spng/spng.c)

# Conditional library targets
if(SPNG_STATIC)
    add_library(spng_static STATIC ${spng_SOURCES})
    target_compile_definitions(spng_static PUBLIC SPNG_STATIC)
    list(APPEND spng_TARGETS spng_static)
endif()

# Find required dependency
find_package(ZLIB REQUIRED)

# Link targets with their dependencies
foreach(spng_TARGET ${spng_TARGETS})
    target_include_directories(${spng_TARGET} PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/spng>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    target_link_libraries(${spng_TARGET} PRIVATE ZLIB::ZLIB)
endforeach()
```

**Key Details:**

- Defines CMake options for conditional compilation
- Can build multiple target variants (shared/static)
- Uses proper generator expressions for build/install interfaces
- Links required transitive dependencies explicitly

</CodeSection>

<CodeSection>

## Code Section: lithium_find_package Wrapper Function

**File:** `/d/Project/lithium-next/cmake/LithiumUtils.cmake`
**Lines:** 151-195
**Purpose:** Wrapper providing consistent discovery and reporting

```cmake
function(lithium_find_package PACKAGE_NAME)
    set(options REQUIRED QUIET)
    set(oneValueArgs VERSION)
    set(multiValueArgs COMPONENTS)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    lithium_message(INFO "Searching for package: ${BoldYellow}${PACKAGE_NAME}${ColorReset}")

    # Build find_package arguments
    set(find_args ${PACKAGE_NAME})

    if(ARG_VERSION)
        list(APPEND find_args ${ARG_VERSION})
    endif()

    if(ARG_REQUIRED)
        list(APPEND find_args REQUIRED)
    endif()

    if(ARG_QUIET)
        list(APPEND find_args QUIET)
    endif()

    if(ARG_COMPONENTS)
        list(APPEND find_args COMPONENTS ${ARG_COMPONENTS})
    endif()

    # Find the package
    find_package(${find_args})

    # Report result with colored output
    if(${PACKAGE_NAME}_FOUND OR ${${PACKAGE_NAME}_FOUND})
        lithium_message(SUCCESS "Found ${PACKAGE_NAME}")
        if(${PACKAGE_NAME}_VERSION)
            message(STATUS "  Version: ${${PACKAGE_NAME}_VERSION}")
        endif()
    else()
        if(ARG_REQUIRED)
            lithium_message(FATAL_ERROR "${PACKAGE_NAME} not found!")
        else()
            lithium_message(WARNING "${PACKAGE_NAME} not found")
        endif()
    endif()
endfunction()
```

**Key Details:**

- Single-point wrapper for all package discovery
- Supports VERSION, REQUIRED, QUIET, COMPONENTS parameters
- Consistent colored message output across all discoveries
- Optional dependencies can fail gracefully without REQUIRED

</CodeSection>

### Existing Third-Party Dependencies

Current dependencies in Lithium-Next project:

**System Libraries (via find_library):**
- libz (zlib) - Compression
- libbz2 - Bzip2 compression
- Readline - Terminal input library
- Curses - Terminal control

**CMake Packages (via find_package):**
- Python 3.7+ - Scripting support
- pybind11 - Python C++ bindings
- Threads (CMAKE_THREAD_LIBS_INIT) - Platform threading

**Third-Party Submodules (libs/thirdparty/):**
- crow - C++ web framework
- pocketpy - Python interpreter alternative
- libspng - PNG image library
- pybind11_json - JSON-Python binding
- matchit - Pattern matching
- cppitertools - Iterator utilities
- fast_float - Float parsing
- immer - Immutable data structures

**Component-Level Dependencies:**
- loguru - Logging library (in atom)
- yaml-cpp - YAML parsing (in server)

---

## Findings Section

### 1. Adding System Library Dependencies

For simple C/C++ libraries available on system (e.g., spdlog, cpptrace):

**Step 1:** Add to root CMakeLists.txt after line 83:

```cmake
# Find the library
lithium_find_package(spdlog REQUIRED)
# or for header-only:
lithium_find_package(cpptrace REQUIRED)
```

**Step 2:** Modify component CMakeLists.txt that needs the library:

```cmake
set(PROJECT_LIBRARIES
    loguru
    atom
    spdlog::spdlog  # Add here with proper namespace
)
```

**Step 3:** Link in main executable if needed at component level, or ensure component is linked to main.

### 2. Adding Third-Party Submodule Dependencies

For libraries with independent CMakeLists.txt (like libspng):

**Step 1:** Create subdirectory under `libs/thirdparty/newlib/`

**Step 2:** Create CMakeLists.txt in that directory following pattern:

```cmake
cmake_minimum_required(VERSION 3.12)
project(newlib C)

# Define options if needed
option(BUILD_SHARED_LIBS "Build shared library" OFF)
option(BUILD_TESTS "Build tests" OFF)

# Add library
add_library(newlib ...)

# Link dependencies
target_link_libraries(newlib PRIVATE ...)

# Install rules
install(TARGETS newlib ...)
```

**Step 3:** Auto-discovery happens - no root CMakeLists.txt modification needed

### 3. Adding Optional Dependencies

For feature-gated dependencies (like LOGURU_USE_FMTLIB pattern):

**Step 1:** Add CMake option in root or component CMakeLists.txt:

```cmake
option(ENABLE_NEW_FEATURE "Enable feature X" ON)
```

**Step 2:** Conditionally find and link:

```cmake
if(ENABLE_NEW_FEATURE)
    lithium_find_package(FeatureLib REQUIRED)
    list(APPEND PROJECT_LIBRARIES FeatureLib::FeatureLib)
    target_compile_definitions(${PROJECT_NAME} PRIVATE FEATURE_ENABLED)
endif()
```

**Step 3:** Use compile definition in source code:

```cpp
#ifdef FEATURE_ENABLED
    // Use feature
#endif
```

### 4. CMake Variables and Namespace Conventions

When using discovered packages, follow these patterns:

**CMake Variables:**
- `${PACKAGE_NAME}_FOUND` - Package found flag
- `${PACKAGE_NAME}_VERSION` - Version string
- `${PACKAGE_NAME}_INCLUDE_DIRS` - Include directories
- `${PACKAGE_NAME}_LIBRARIES` - Library names/paths

**Target Namespaces:**
- Use `PACKAGE::LIBRARY` format when available (e.g., `spdlog::spdlog`, `ZLIB::ZLIB`)
- Preferred over raw variables for better CMake integration
- Easier to use with `target_link_libraries()`

### 5. Compiler Definitions for Feature Control

Pattern for conditional compilation based on dependency availability:

```cmake
# In CMakeLists.txt
if(SomeLib_FOUND)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAS_SOMELIB)
endif()

# In source code (e.g., .hpp file)
#ifdef HAS_SOMELIB
    #include <somelib/header.hpp>
#endif
```

### 6. Key Files to Modify for Dependency Addition

**Root CMakeLists.txt** (`/d/Project/lithium-next/CMakeLists.txt`):
- Lines 69-83: External library detection section
- Add `lithium_find_package()` or `find_library()` calls here
- Main discovery location for all project-wide dependencies

**Component CMakeLists.txt** (e.g., `src/*/CMakeLists.txt`):
- Lines 24-35 in config example: `PROJECT_LIBRARIES` section
- `list(APPEND PROJECT_LIBRARIES ...)` for local dependencies
- Conditional additions for optional features

**Main Executable Linking** (`/d/Project/lithium-next/CMakeLists.txt`):
- Lines 127-150: `target_link_libraries(lithium-next PRIVATE ...)`
- If dependency used only in main executable, add here
- If used in components, add to component CMakeLists.txt instead

### 7. Validation and Configuration Output

After adding dependency:

```bash
# Run configuration
cmake -B build

# Check for success/failure in output
# Look for lines like:
# [Lithium] Searching for package: spdlog
# [Lithium Success] Found spdlog
#   Version: 1.11.0
```

### 8. Handling Header-Only Libraries

For header-only libraries like cpptrace:

```cmake
# Simpler discovery with no linking needed
lithium_find_package(cpptrace REQUIRED)

# In component, just need includes
target_include_directories(${PROJECT_NAME} PRIVATE ${cpptrace_INCLUDE_DIRS})
# Or if it has proper targets:
target_link_libraries(${PROJECT_NAME} PRIVATE cpptrace::cpptrace)
```

### 9. Cross-Platform Considerations

Dependencies may require platform-specific handling:

```cmake
if(WIN32)
    list(APPEND PROJECT_LIBRARIES ws2_32)  # Windows socket library
elseif(UNIX AND NOT APPLE)
    list(APPEND PROJECT_LIBRARIES pthread)  # POSIX threads
endif()
```

### 10. Testing Dependency Integration

After adding dependency:

1. Run full CMake configuration with clean build directory
2. Verify colored output shows successful detection
3. Build affected components: `cmake --build build --target lithium_component_name`
4. Run tests if dependency affects testable code
5. Verify main executable builds: `cmake --build build --target lithium-next`

</CodeSection>
