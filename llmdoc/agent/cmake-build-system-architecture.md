# Lithium-Next CMake Build System Architecture Analysis

## Evidence Section

### CMake Core Modules and Utilities

<CodeSection>

## Code Section: LithiumUtils.cmake - Standardized Library Creation Function

**File:** `/d/Project/lithium-next/cmake/LithiumUtils.cmake`
**Lines:** 49-146
**Purpose:** Defines `lithium_add_library()` function for standardized library creation across all components

```cmake
function(lithium_add_library)
    set(options PUBLIC_HEADERS)
    set(oneValueArgs NAME VERSION TYPE DESCRIPTION)
    set(multiValueArgs SOURCES HEADERS LIBRARIES INCLUDES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Validate required arguments
    if(NOT ARG_NAME)
        message(FATAL_ERROR "lithium_add_library: NAME is required")
    endif()

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "lithium_add_library: SOURCES is required for ${ARG_NAME}")
    endif()

    # Create the library
    add_library(${ARG_NAME} ${ARG_TYPE} ${ARG_SOURCES} ${ARG_HEADERS})

    # Add include directories
    if(ARG_INCLUDES)
        target_include_directories(${ARG_NAME} PUBLIC ${ARG_INCLUDES})
    else()
        target_include_directories(${ARG_NAME} PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        )
    endif()

    # Link libraries
    if(ARG_LIBRARIES)
        target_link_libraries(${ARG_NAME} PRIVATE ${ARG_LIBRARIES})
    endif()

    # Install rules
    install(TARGETS ${ARG_NAME}
        EXPORT ${ARG_NAME}Targets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )
endfunction()
```

**Key Details:**

- Abstracts raw `add_library()` calls with validation and standardized configuration
- Supports library type selection (STATIC, SHARED, MODULE)
- Automatically configures include directories with build/install interface separation
- Handles library linking with private scope dependencies
- Implements automatic installation rules for all libraries

</CodeSection>

<CodeSection>

## Code Section: Root CMakeLists.txt - Dependency Detection

**File:** `/d/Project/lithium-next/CMakeLists.txt`
**Lines:** 69-83
**Purpose:** Shows external library discovery pattern used in Lithium-Next

```cmake
# ============================================================================
# External Library Detection
# ============================================================================

lithium_print_subsection("Detecting External Libraries")

# Core compression libraries
find_library(LIBZ_LIBRARY NAMES z PATHS /usr/lib/x86_64-linux-gnu /opt/conda/lib)
find_library(LIBBZ2_LIBRARY NAMES bz2 PATHS /usr/lib/x86_64-linux-gnu /opt/conda/lib)

# Python and bindings
set(PYBIND11_FINDPYTHON ON)
lithium_find_package(Python VERSION 3.7 COMPONENTS Interpreter Development REQUIRED)
lithium_find_package(pybind11 REQUIRED)

# Terminal libraries
lithium_find_package(Readline REQUIRED)
lithium_find_package(Curses REQUIRED)
```

**Key Details:**

- Uses both `find_library()` for simple libraries and `find_package()` for complex dependencies
- `lithium_find_package()` wrapper provides enhanced reporting with colored output
- Required dependencies fail configuration if not found
- Optional dependencies can be detected and conditionally linked

</CodeSection>

<CodeSection>

## Code Section: LithiumUtils.cmake - Package Discovery Function

**File:** `/d/Project/lithium-next/cmake/LithiumUtils.cmake`
**Lines:** 151-195
**Purpose:** Wrapper function for package discovery with enhanced reporting

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

    # Find the package
    find_package(${find_args})

    # Report result
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

- Supports REQUIRED, QUIET, and VERSION options
- Provides consistent colored output for package discovery results
- Differentiates between required and optional dependencies
- Shows version information when available

</CodeSection>

<CodeSection>

## Code Section: Compiler Options Detection - C++ Standard Selection

**File:** `/d/Project/lithium-next/cmake/compiler_options.cmake`
**Lines:** 12-30
**Purpose:** Automatic C++ standard detection with fallback mechanism

```cmake
# Check and set C++ standard
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-std=c++23 HAS_CXX23_FLAG)
check_cxx_compiler_flag(-std=c++20 HAS_CXX20_FLAG)

if(HAS_CXX23_FLAG)
    set(CMAKE_CXX_STANDARD 23)
    lithium_message(SUCCESS "Using C++23 standard")
elseif(HAS_CXX20_FLAG)
    set(CMAKE_CXX_STANDARD 20)
    lithium_message(SUCCESS "Using C++20 standard")
else()
    lithium_message(FATAL_ERROR "C++20 standard or higher is required!")
endif()
```

**Key Details:**

- Prefers C++23 but accepts C++20 as minimum requirement
- Uses `CheckCXXCompilerFlag` for capability detection
- Configuration fails if required standard unavailable
- Reports selected standard with colored output

</CodeSection>

<CodeSection>

## Code Section: Compiler Options - Compiler Version Validation

**File:** `/d/Project/lithium-next/cmake/compiler_options.cmake`
**Lines:** 32-76
**Purpose:** Validates compiler versions meet project minimum requirements

```cmake
function(lithium_check_compiler_version)
    lithium_print_subsection("Compiler Detection")

    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        execute_process(
            COMMAND ${CMAKE_CXX_COMPILER} -dumpfullversion
            OUTPUT_VARIABLE GCC_VERSION
            OUTPUT_STRIP_TRAILING_WHITESPACE
        )
        if(GCC_VERSION VERSION_LESS "13.0")
            lithium_message(WARNING "g++ version ${GCC_VERSION} is too old...")
            find_program(GCC_COMPILER NAMES g++-13 g++-14 g++-15)
            if(GCC_COMPILER)
                set(CMAKE_CXX_COMPILER ${GCC_COMPILER} CACHE STRING "C++ Compiler" FORCE)
                lithium_message(SUCCESS "Using found g++ compiler: ${GCC_COMPILER}")
            else()
                lithium_message(FATAL_ERROR "g++ version 13.0 or higher is required")
            endif()
        else()
            lithium_message(SUCCESS "Using g++ version ${BoldGreen}${GCC_VERSION}${ColorReset}")
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Similar validation for Clang 16+
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        # Similar validation for MSVC 19.28+
    endif()
endfunction()
```

**Key Details:**

- Enforces GCC 13+, Clang 16+, MSVC 19.28+ requirements
- Automatically searches for compatible compiler versions if current is too old
- Updates CMAKE_CXX_COMPILER cache variable when fallback found
- Provides detailed error messages for missing versions

</CodeSection>

<CodeSection>

## Code Section: Component Library Configuration Pattern

**File:** `/d/Project/lithium-next/src/config/CMakeLists.txt`
**Lines:** 1-72
**Purpose:** Demonstrates standardized component library creation pattern

```cmake
lithium_component_begin(lithium_config 1.0.0)

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    core/manager.cpp
    components/cache.cpp
    components/validator.cpp
    components/serializer.cpp
    components/watcher.cpp
)

set(PROJECT_HEADERS
    config.hpp
    core/exception.hpp
    core/manager.hpp
    # ... more headers
)

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

# ============================================================================
# Build Library
# ============================================================================

lithium_add_library(
    NAME ${PROJECT_NAME}
    SOURCES ${PROJECT_SOURCES} ${PROJECT_HEADERS}
    LIBRARIES ${PROJECT_LIBRARIES}
    VERSION ${PROJECT_VERSION}
    TYPE STATIC
    DESCRIPTION "Configuration management module"
    PUBLIC_HEADERS
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Config")

lithium_component_end(${PROJECT_NAME})
```

**Key Details:**

- Uses `lithium_component_begin()` and `lithium_component_end()` macros
- Separates sources, headers, and library dependencies as CMake variables
- Shows platform-specific library additions (Windows threading)
- Applies standard compiler flags and IDE folder organization
- All components follow identical structure

</CodeSection>

<CodeSection>

## Code Section: Root CMakeLists.txt - Main Executable Linking

**File:** `/d/Project/lithium-next/CMakeLists.txt`
**Lines:** 127-150
**Purpose:** Shows how all components are linked into main executable

```cmake
# ============================================================================
# Main Executable
# ============================================================================

lithium_print_subsection("Configuring Main Executable")

add_executable(lithium-next ${lithium_src_dir}/app.cpp)

# Link all required libraries
target_link_libraries(lithium-next PRIVATE
    pybind11::module
    pybind11::lto
    lithium_components
    lithium_config
    lithium_database
    lithium_debug
    lithium_device
    lithium_script
    lithium_server
    lithium_target
    lithium_task
    lithium_tools
    atom
    loguru
    ${Readline_LIBRARIES}
    ${CURSES_LIBRARIES}
)

target_include_directories(lithium-next PRIVATE ${Python_INCLUDE_DIRS})

# Apply compiler flags
lithium_apply_compiler_flags(lithium-next)

lithium_message(SUCCESS "Main executable 'lithium-next' configured")
```

**Key Details:**

- Main executable links all component libraries
- Uses CMake target names (with `::` namespace) for external packages
- Includes Python interpreter include directories
- Applies compiler flags consistently to all targets

</CodeSection>

<CodeSection>

## Code Section: Third-Party Library - libspng Integration Pattern

**File:** `/d/Project/lithium-next/libs/thirdparty/libspng/CMakeLists.txt`
**Lines:** 1-72
**Purpose:** Demonstrates pattern for integrating third-party libraries with options

```cmake
cmake_minimum_required(VERSION 3.12)

project(libspng C)

set(CMAKE_C_STANDARD 99)

set(SPNG_MAJOR 0)
set(SPNG_MINOR 7)
set(SPNG_REVISION 4)
set(SPNG_VERSION ${SPNG_MAJOR}.${SPNG_MINOR}.${SPNG_REVISION})

option(ENABLE_OPT "Enable architecture-specific optimizations" ON)
option(SPNG_SHARED "Build shared lib" ON)
option(SPNG_STATIC "Build static lib" ON)
option(BUILD_EXAMPLES "Build examples" OFF)

# ... configuration logic ...

set(spng_SOURCES spng/spng.c)

if(SPNG_STATIC)
    add_library(spng_static STATIC ${spng_SOURCES})
    target_compile_definitions(spng_static PUBLIC SPNG_STATIC)
    list(APPEND spng_TARGETS spng_static)
endif()

find_package(ZLIB REQUIRED)
foreach(spng_TARGET ${spng_TARGETS})
    target_include_directories(${spng_TARGET} PUBLIC
      $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/spng>
      $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    target_link_libraries(${spng_TARGET} PRIVATE ZLIB::ZLIB)
endforeach()
```

**Key Details:**

- Uses `option()` to control conditional compilation
- Demonstrates dual shared/static library building capability
- Shows proper use of generator expressions for build vs install interface
- Finds and links required transitive dependencies

</CodeSection>

<CodeSection>

## Code Section: Loguru Logging Library Configuration

**File:** `/d/Project/lithium-next/libs/atom/atom/log/CMakeLists.txt`
**Lines:** 1-100
**Purpose:** Shows complex third-party integration with optional features

```cmake
project(loguru VERSION "${LOGURU_VERSION}" LANGUAGES CXX)

# Expose cache variables for optional features
option(LOGURU_INSTALL        "Generate the install target(s)" ${PROJECT_IS_TOP_LEVEL})
option(LOGURU_BUILD_EXAMPLES "Build the project examples"     ${PROJECT_IS_TOP_LEVEL})
option(LOGURU_BUILD_TESTS    "Build the tests"                ${PROJECT_IS_TOP_LEVEL})

# Conditionally link fmt library
if (LOGURU_USE_FMTLIB)
    message(STATUS "linking to fmt")

    if (NOT TARGET fmt::fmt)
        find_package(fmt CONFIG REQUIRED)
    endif()

    if (LOGURU_FMT_HEADER_ONLY)
        target_link_libraries(loguru PUBLIC fmt::fmt-header-only)
    else()
        target_link_libraries(loguru PUBLIC fmt::fmt)
    endif()
endif()

# Compile definitions for feature control
target_compile_definitions(loguru
  PUBLIC
    $<$<NOT:$<STREQUAL:,${LOGURU_STACKTRACES}>>:LOGURU_STACKTRACES=$<BOOL:${LOGURU_STACKTRACES}>>
    $<$<NOT:$<STREQUAL:,${LOGURU_USE_FMTLIB}>>:LOGURU_USE_FMTLIB=$<BOOL:${LOGURU_USE_FMTLIB}>>
    $<$<NOT:$<STREQUAL:,${LOGURU_RTTI}>>:LOGURU_RTTI=$<BOOL:${LOGURU_RTTI}>>
)
```

**Key Details:**

- Exposes build options as CMake cache variables
- Conditionally finds and links optional transitive dependencies
- Uses generator expressions for compile-time feature control
- Demonstrates integration when library is used as subdirectory

</CodeSection>

<CodeSection>

## Code Section: Compiler Flag Application Pattern

**File:** `/d/Project/lithium-next/cmake/compiler_options.cmake`
**Lines:** 184-235
**Purpose:** Standardized compiler flag application across targets

```cmake
# Function to apply standard compiler flags to a target
function(lithium_apply_compiler_flags TARGET_NAME)
    # Apple-specific flags
    if(APPLE)
        check_cxx_compiler_flag(-stdlib=libc++ HAS_LIBCXX_FLAG)
        if(HAS_LIBCXX_FLAG)
            target_compile_options(${TARGET_NAME} PRIVATE -stdlib=libc++)
        endif()
    endif()

    # Warning flags
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${TARGET_NAME} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${TARGET_NAME} PRIVATE /W4)
    endif()

    # LTO for Release builds
    if(CMAKE_BUILD_TYPE MATCHES "Release|RelWithDebInfo")
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
            target_compile_options(${TARGET_NAME} PRIVATE -flto)
            target_link_options(${TARGET_NAME} PRIVATE -flto)
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
            target_compile_options(${TARGET_NAME} PRIVATE /GL)
            target_link_options(${TARGET_NAME} PRIVATE /LTCG)
        endif()
    endif()
endfunction()
```

**Key Details:**

- Abstracts platform and compiler-specific flags into reusable function
- Handles Apple-specific C++ standard library selection
- Applies different warning levels per compiler
- Enables LTO conditionally for Release builds

</CodeSection>

### Third-Party Library Directory Structure

Available third-party libraries in `/d/Project/lithium-next/libs/thirdparty/`:

- `crow/` - C++ web framework (header-only)
- `pocketpy/` - Python interpreter alternative
- `libspng/` - PNG image library with CMakeLists.txt
- `pybind11_json/` - Python JSON binding
- `matchit/` - Pattern matching library
- `cppitertools/` - Iterator utilities (header-only)
- `fast_float/` - Fast float parsing (header-only)
- `immer/` - Immutable data structures (header-only)

### Feature Macros and Options

From configuration analysis:

- `CROW_ENABLE_COMPRESSION` - HTTP compression in Crow framework
- `CROW_ENABLE_SSL` - HTTPS support in Crow
- `PYBIND11_FINDPYTHON` - Use modern Python finding in pybind11
- `LOGURU_USE_FMTLIB` - Use fmt library in loguru
- `LOGURU_STACKTRACES` - Enable stack trace support
- `LOGURU_RTTI` - Enable RTTI features
- `ENABLE_OPT` - Optimization features in third-party libraries

---

## Findings Section

### 1. Build System Architecture Overview

Lithium-Next uses a **modular, hierarchical CMake build system** organized into three layers:

**Top Level (Root CMakeLists.txt):**
- Initializes project with C++23 preference (C++20 minimum)
- Discovers external system dependencies (Python, pybind11, Readline, Curses)
- Calls `add_subdirectory()` for four main sub-projects: libs, modules, src, example, tests
- Creates main executable linking all components

**Library Level (libs/CMakeLists.txt):**
- Uses `lithium_add_subdirectories_recursive()` to auto-discover atom and thirdparty libraries
- Atom library includes include directories automatically

**Component Level (src/*/CMakeLists.txt):**
- Each major component (config, server, task, device, etc.) implements standardized template
- Uses `lithium_component_begin()` and `lithium_component_end()` macros
- Follows consistent pattern: sources → headers → dependencies → `lithium_add_library()` call

### 2. Dependency Addition Pattern

Adding new dependencies follows three approaches depending on type:

**System Libraries (find_library):**
- Direct `find_library()` calls with path hints
- Example: libz, libbz2 in root CMakeLists.txt
- Used for simple, widely-available libraries

**CMake Packages (find_package):**
- Wrapped with `lithium_find_package()` for consistent reporting
- Supports VERSION, REQUIRED, QUIET, COMPONENTS parameters
- Examples: Python, pybind11, Readline, Curses
- Automatic version reporting and colored output

**Third-Party Submodules (add_subdirectory):**
- Located in `libs/thirdparty/`
- Each has independent CMakeLists.txt
- Discovered recursively by thirdparty CMakeLists.txt
- Can define custom build options (ENABLE_*, BUILD_*, etc.)
- Examples: libspng (C library), pocketpy (Python interpreter)

### 3. Optional Feature Pattern

The system supports optional dependencies through two mechanisms:

**Conditional find_package():**
- Optional packages can be found without REQUIRED flag
- Checked with `if(${PACKAGE_NAME}_FOUND)` condition
- Dependencies conditionally added with `list(APPEND PROJECT_LIBRARIES ...)`
- Compile definitions set conditionally with `target_compile_definitions()`

**CMake Options for Third-Party:**
- Third-party libraries use `option()` for feature control
- Examples: `SPNG_SHARED`, `SPNG_STATIC`, `BUILD_EXAMPLES`
- `LOGURU_USE_FMTLIB`, `LOGURU_STACKTRACES` for loguru
- Options exposed as CMake cache variables

### 4. Key CMake Modules and Functions

**LithiumColors.cmake:**
- Provides colored output functions
- `lithium_message(TYPE MESSAGE)` for consistent formatting
- `lithium_print_header()`, `lithium_print_subsection()`, `lithium_print_config()`

**LithiumUtils.cmake:**
- `lithium_add_library()` - standardized library creation with validation
- `lithium_add_subdirectories_recursive()` - auto-discovery of CMakeLists.txt
- `lithium_find_package()` - wrapped package discovery with reporting
- `lithium_apply_compiler_flags()` - consistent flag application
- `lithium_apply_strict_flags()` - stricter warning configuration
- `lithium_enable_lto()` - LTO configuration for Release builds
- `lithium_component_begin/end()` - component lifecycle macros
- `lithium_set_target_folder()` - IDE folder organization

**compiler_options.cmake:**
- Compiler detection and version validation (GCC 13+, Clang 16+, MSVC 19.28+)
- C++ standard selection (C++23 preferred, C++20 fallback)
- Compiler flag functions for different platforms
- LTO configuration for Release builds

### 5. Executable Linking Pattern

Main executable (`lithium-next`) links these internal libraries:
- lithium_components
- lithium_config
- lithium_database
- lithium_debug
- lithium_device
- lithium_script
- lithium_server
- lithium_target
- lithium_task
- lithium_tools
- atom (utility library)
- loguru (logging)
- pybind11 (Python bindings)
- Readline and Curses (terminal libraries)

Each component is built as STATIC library independently and linked into main.

### 6. Third-Party Integration Pattern

Third-party libraries in `libs/thirdparty/` follow this pattern:

- Header-only libraries (crow, matchit, cppitertools, fast_float, immer) - no CMakeLists.txt
- Libraries with CMakeLists.txt can define custom options
- libspng demonstrates pattern with shared/static options
- Loguru shows complex integration with optional fmt dependency
- Pocketpy has conditional feature compilation (PK_ENABLE_OS, PK_MODULE_WIN32)

### 7. Platform-Specific Handling

- Windows: `ws2_32` added conditionally, special Crow SSL handling
- Apple: `libc++` standard library preference
- Linux/Unix: Traditional pthread and dl library usage
- CMake leverages `CMAKE_SYSTEM_NAME`, `WIN32`, `APPLE`, `UNIX` variables

### 8. Compiler Requirements and Capabilities

- C++23 standard preferred with C++20 minimum fallback
- GCC 13+, Clang 16+, MSVC 19.28+ required
- Automatic fallback search for compatible compilers
- Link Time Optimization (LTO) enabled for Release builds
- Compiler-specific warning levels (-Wall/-Wextra for GCC/Clang, /W4 for MSVC)

### 9. Installation Rules

All libraries and executables support standard CMake installation:
- Executables → `${CMAKE_INSTALL_BINDIR}`
- Libraries → `${CMAKE_INSTALL_LIBDIR}`
- Headers → `${CMAKE_INSTALL_INCLUDEDIR}`
- CMake package files → `${CMAKE_INSTALL_LIBDIR}/cmake/`

</CodeSection>
