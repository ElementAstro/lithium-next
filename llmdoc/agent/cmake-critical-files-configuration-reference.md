# Lithium-Next CMake Critical Files and Configuration Reference

## Evidence Section

<CodeSection>

## Code Section: Root CMakeLists.txt - Full Structure Overview

**File:** `/d/Project/lithium-next/CMakeLists.txt`
**Lines:** 1-181
**Purpose:** Entry point for entire CMake build system

```cmake
# Project initialization
cmake_minimum_required(VERSION 3.20)
project(lithium-next VERSION 1.0.0 LANGUAGES C CXX)

# Set C++ standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include core CMake modules
include(cmake/policies.cmake)
include(cmake/LithiumColors.cmake)
include(cmake/LithiumUtils.cmake)

# Define project directories
set(lithium_src_dir ${CMAKE_SOURCE_DIR}/src)
set(lithium_thirdparty_dir ${CMAKE_SOURCE_DIR}/libs/thirdparty)
set(lithium_atom_dir ${CMAKE_SOURCE_DIR}/libs/atom)

# External dependency detection
lithium_find_package(Python VERSION 3.7 COMPONENTS Interpreter Development REQUIRED)
lithium_find_package(pybind11 REQUIRED)
lithium_find_package(Readline REQUIRED)
lithium_find_package(Curses REQUIRED)

# Compiler configuration
include(cmake/compiler_options.cmake)

# Add subdirectories
add_subdirectory(libs)
add_subdirectory(modules)
add_subdirectory(src)
add_subdirectory(example)
add_subdirectory(tests)

# Main executable
add_executable(lithium-next ${lithium_src_dir}/app.cpp)
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

# Installation and summary
install(TARGETS lithium-next RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
```

**Key Details:**

- Minimum CMake version 3.20 required
- Project initialized with version 1.0.0
- Imports all utility functions before use
- Declares all project-wide dependencies
- Creates main executable that links 12+ internal libraries

</CodeSection>

<CodeSection>

## Code Section: LithiumColors.cmake - Output Formatting

**File:** `/d/Project/lithium-next/cmake/LithiumColors.cmake`
**Lines:** 1-80
**Purpose:** Provides consistent colored output throughout build system

```cmake
# Define color variables (ANSI codes on Unix, empty on Windows)
if(NOT WIN32)
    string(ASCII 27 Esc)
    set(ColorReset "${Esc}[m")
    set(BoldRed     "${Esc}[1;31m")
    set(BoldGreen   "${Esc}[1;32m")
    set(BoldYellow  "${Esc}[1;33m")
    set(BoldBlue    "${Esc}[1;34m")
    set(BoldMagenta "${Esc}[1;35m")
    set(BoldCyan    "${Esc}[1;36m")
else()
    set(ColorReset "")
    set(BoldRed     "")
    set(BoldGreen   "")
    set(BoldYellow  "")
    set(BoldBlue    "")
    set(BoldMagenta "")
    set(BoldCyan    "")
endif()

# Message function
function(lithium_message TYPE MESSAGE)
    if(TYPE STREQUAL "SUCCESS")
        message(STATUS "[${BoldGreen}Lithium${ColorReset}] ${MESSAGE}")
    elseif(TYPE STREQUAL "WARNING")
        message(STATUS "[${BoldYellow}Lithium${ColorReset}] ${MESSAGE}")
    elseif(TYPE STREQUAL "ERROR")
        message(STATUS "[${BoldRed}Lithium${ColorReset}] ${MESSAGE}")
    elseif(TYPE STREQUAL "INFO")
        message(STATUS "[${BoldCyan}Lithium${ColorReset}] ${MESSAGE}")
    else()
        message(STATUS "[${BoldBlue}Lithium${ColorReset}] ${MESSAGE}")
    endif()
endfunction()

# Section headers
function(lithium_print_header HEADER_TEXT)
    string(LENGTH "${HEADER_TEXT}" len)
    math(EXPR FILL_LEN "80 - ${len} - 2")
    string(REPEAT "=" ${FILL_LEN} FILL)
    message(STATUS "")
    message(STATUS "${BoldCyan}== ${HEADER_TEXT} ${FILL}${ColorReset}")
    message(STATUS "")
endfunction()

function(lithium_print_subsection TEXT)
    message(STATUS "")
    message(STATUS "${BoldBlue}--- ${TEXT} ---${ColorReset}")
    message(STATUS "")
endfunction()

function(lithium_print_config KEY VALUE)
    message(STATUS "[${BoldGreen}Lithium${ColorReset}] ${KEY}: ${BoldYellow}${VALUE}${ColorReset}")
endfunction()
```

**Key Details:**

- Platform-aware color codes (ANSI on Unix, empty on Windows)
- Five message types: SUCCESS, WARNING, ERROR, INFO, STATUS
- Section formatting with headers and subsections
- Config key-value display for clarity

</CodeSection>

<CodeSection>

## Code Section: compiler_options.cmake - Compiler Configuration

**File:** `/d/Project/lithium-next/cmake/compiler_options.cmake`
**Lines:** 1-180
**Purpose:** Handles compiler detection, version validation, and flag management

```cmake
# Build type configuration
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Choose the build type." FORCE)
endif()

# C++ standard detection
check_cxx_compiler_flag(-std=c++23 HAS_CXX23_FLAG)
check_cxx_compiler_flag(-std=c++20 HAS_CXX20_FLAG)

if(HAS_CXX23_FLAG)
    set(CMAKE_CXX_STANDARD 23)
elseif(HAS_CXX20_FLAG)
    set(CMAKE_CXX_STANDARD 20)
else()
    message(FATAL_ERROR "C++20 standard or higher is required!")
endif()

# Compiler version validation
function(lithium_check_compiler_version)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        # GCC 13+ required
        if(GCC_VERSION VERSION_LESS "13.0")
            find_program(GCC_COMPILER NAMES g++-13 g++-14 g++-15)
            if(GCC_COMPILER)
                set(CMAKE_CXX_COMPILER ${GCC_COMPILER} CACHE STRING "C++ Compiler" FORCE)
            else()
                message(FATAL_ERROR "g++ version 13.0 or higher is required")
            endif()
        endif()
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Clang 16+ required
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        # MSVC 19.28+ required
    endif()
endfunction()

# Compiler flag application
function(lithium_apply_compiler_flags TARGET_NAME)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${TARGET_NAME} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
        target_compile_options(${TARGET_NAME} PRIVATE /W4)
    endif()

    # LTO for Release
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

- Default build type is Debug
- Automatically selects C++23 or falls back to C++20
- Validates GCC 13+, Clang 16+, MSVC 19.28+
- Applies appropriate warning flags per compiler
- Enables LTO for Release builds
- Handles Apple libc++ preference

</CodeSection>

<CodeSection>

## Code Section: policies.cmake - CMake Policy Settings

**File:** `/d/Project/lithium-next/cmake/policies.cmake`
**Lines:** 1-10
**Purpose:** Sets CMake policies for consistent cross-version behavior

```cmake
# Policies for CMake compatibility
cmake_policy(SET CMP0003 NEW)  # Libraries linked by full-path must have valid library file name
cmake_policy(SET CMP0043 OLD)  # Ignore COMPILE_DEFINITIONS_<Config> properties
cmake_policy(SET CMP0148 OLD)  # The FindPythonInterp and FindPythonLibs modules are removed
```

**Key Details:**

- Three policies configured for compatibility
- CMP0003: Full-path library linking behavior
- CMP0043: Backward compatibility for configuration-specific flags
- CMP0148: Python module compatibility for older CMake versions

</CodeSection>

<CodeSection>

## Code Section: LithiumUtils.cmake - Subdirectory Discovery

**File:** `/d/Project/lithium-next/cmake/LithiumUtils.cmake`
**Lines:** 15-48
**Purpose:** Recursive discovery and addition of CMakeLists.txt subdirectories

```cmake
function(lithium_add_subdirectories_recursive START_DIR)
    set(options "")
    set(oneValueArgs PATTERN_FILE MESSAGE_PREFIX)
    set(multiValueArgs "")
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT ARG_MESSAGE_PREFIX)
        set(ARG_MESSAGE_PREFIX "Adding subdirectory")
    endif()

    file(GLOB entries "${START_DIR}/*")
    foreach(entry ${entries})
        if(IS_DIRECTORY ${entry})
            set(should_add FALSE)

            # Check if CMakeLists.txt exists
            if(EXISTS "${entry}/CMakeLists.txt")
                # If pattern file specified, check if it exists too
                if(ARG_PATTERN_FILE)
                    if(EXISTS "${entry}/${ARG_PATTERN_FILE}")
                        set(should_add TRUE)
                    endif()
                else()
                    set(should_add TRUE)
                endif()
            endif()

            if(should_add)
                get_filename_component(dir_name ${entry} NAME)
                lithium_message(STATUS "${ARG_MESSAGE_PREFIX}: ${BoldCyan}${dir_name}${ColorReset}")
                add_subdirectory(${entry})
            endif()
        endif()
    endforeach()
endfunction()
```

**Key Details:**

- One level of subdirectory discovery (non-recursive per-level)
- Searches for CMakeLists.txt presence
- Optional pattern file checking
- Customizable message prefix for clarity
- Used by libs/CMakeLists.txt for automatic discovery

</CodeSection>

<CodeSection>

## Code Section: Component CMakeLists.txt Template Structure

**File:** `/d/Project/lithium-next/src/config/CMakeLists.txt`
**Purpose:** Shows standardized template all components follow

```cmake
# ============================================================================
# CMakeLists.txt for Lithium-Component
# ============================================================================

lithium_component_begin(lithium_component 1.0.0)

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
    loguru
    ${CMAKE_THREAD_LIBS_INIT}
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
    DESCRIPTION "Component description"
    PUBLIC_HEADERS
)

lithium_apply_compiler_flags(${PROJECT_NAME})
lithium_set_target_folder(${PROJECT_NAME} "Core/Component")

lithium_component_end(${PROJECT_NAME})
```

**Key Details:**

- Consistent six-section structure across all components
- Macro-based begin/end for consistency
- Variable-based source and dependency lists
- Single call to `lithium_add_library()` for all library setup

</CodeSection>

### Critical CMake File Locations

| File Path | Lines | Purpose |
|-----------|-------|---------|
| `/d/Project/lithium-next/CMakeLists.txt` | 181 | Root project configuration |
| `/d/Project/lithium-next/cmake/LithiumColors.cmake` | 80 | Colored output functions |
| `/d/Project/lithium-next/cmake/LithiumUtils.cmake` | 280 | Core utility functions |
| `/d/Project/lithium-next/cmake/compiler_options.cmake` | 235 | Compiler configuration |
| `/d/Project/lithium-next/cmake/policies.cmake` | 10 | CMake policies |
| `/d/Project/lithium-next/cmake/README.md` | 580 | Build system documentation |
| `/d/Project/lithium-next/libs/CMakeLists.txt` | 28 | Library layer coordination |
| `/d/Project/lithium-next/libs/thirdparty/CMakeLists.txt` | 24 | Third-party auto-discovery |
| `/d/Project/lithium-next/src/CMakeLists.txt` | 12 | Source module discovery |
| `/d/Project/lithium-next/src/*/CMakeLists.txt` | ~60 | Component definitions |

---

## Findings Section

### 1. Root CMakeLists.txt Functions

**Location:** `/d/Project/lithium-next/CMakeLists.txt`

Acts as master orchestrator:
- Initializes project with version 1.0.0
- Imports utility modules from cmake/ directory
- Discovers system dependencies (Python, pybind11, Readline, Curses)
- Sets include directories for legacy compatibility
- Configures Crow framework options (COMPRESSION, SSL)
- Includes compiler configuration from separate file
- Adds five main subdirectories (libs, modules, src, example, tests)
- Defines main executable linking 12+ component libraries
- Specifies installation rules

### 2. Core CMake Module Hierarchy

**Three-tier module system:**

**Tier 1 - Foundational (LithiumColors):**
- Provides colored message output
- Used by all higher-tier modules
- Platform-aware (ANSI colors vs empty on Windows)

**Tier 2 - Utilities (LithiumUtils):**
- Depends on LithiumColors
- Provides library creation, package discovery, subdirectory discovery
- Used throughout project

**Tier 3 - Application (compiler_options, policies):**
- compiler_options.cmake: Compiler detection and flag management
- policies.cmake: CMake version compatibility

### 3. Build Types and Their Configuration

**Four build types supported:**

- **Debug** (default): No optimization, full symbols
- **Release**: Full optimization with LTO enabled
- **RelWithDebInfo**: Optimization with debug symbols and LTO
- **MinSizeRel**: Size-optimized builds

Each type triggers different compiler flag behavior (especially LTO).

### 4. Compiler Version Requirements and Fallback

**Minimum versions enforced:**
- GCC: 13.0+
- Clang: 16.0+
- MSVC: 19.28 (Visual Studio 2019 16.10+)

**Fallback mechanism:**
- If current compiler too old, CMake searches for newer versions
- For GCC: searches g++-13, g++-14, g++-15
- For Clang: searches clang-17, clang-18, clang-19
- Updates CMAKE_CXX_COMPILER cache variable if found
- Fails with clear error if no compatible compiler available

### 5. C++ Standard Selection

**Preference order:**
1. C++23 (preferred if available)
2. C++20 (minimum fallback)
3. Configuration fails if neither available

Uses `check_cxx_compiler_flag()` to detect capability.

### 6. Compiler Flag Strategy

**Platform-specific warning levels:**
- GCC/Clang: `-Wall -Wextra -Wpedantic`
- MSVC: `/W4`

**Strict mode available:**
- Adds `-Werror` (GCC/Clang) or `/WX` (MSVC)
- Converts warnings to errors
- Used for new/clean code

**Link Time Optimization (LTO):**
- Enabled for Release and RelWithDebInfo builds
- GCC/Clang: `-flto` compiler and linker flags
- MSVC: `/GL` compiler flag and `/LTCG` linker flag

### 7. Component Template Structure

All components in `src/*/CMakeLists.txt` follow identical structure:

1. **Component Begin**: `lithium_component_begin()` macro
2. **Sources Section**: List source and header files
3. **Dependencies Section**: List library dependencies
4. **Build Library**: Single `lithium_add_library()` call
5. **Flags**: Apply compiler flags
6. **IDE Folder**: Set IDE folder structure
7. **Component End**: `lithium_component_end()` macro

This consistency enables automatic discovery and standardized building.

### 8. Subdirectory Discovery System

**Three-level automatic discovery:**

**Level 1 (Root):**
- Explicit `add_subdirectory()` calls for major sections
- Manually specified: libs, modules, src, example, tests

**Level 2 (Library):**
- `libs/CMakeLists.txt` uses `lithium_add_subdirectories_recursive()`
- Discovers both atom and thirdparty subdirectories
- No manual configuration of library components needed

**Level 3 (Source):**
- `src/CMakeLists.txt` uses `lithium_add_subdirectories_recursive()`
- Each subdirectory with CMakeLists.txt auto-discovered
- Adds: components, config, database, debug, device, script, server, target, task, tools, client directories

### 9. External Dependency Organization

**Root discovery:**
- All system-level package discovery in root CMakeLists.txt (lines 69-83)
- Components use found packages transitively

**Component-level dependencies:**
- Components explicitly list what they link
- Dependencies inherited transitively through main executable

**Dependency types:**
- System libraries: `find_library()` with path hints
- CMake packages: `lithium_find_package()` wrapper
- Third-party submodules: Auto-discovered from libs/thirdparty/

### 10. Installation Configuration

**Standard CMake installation:**
- Executables → `${CMAKE_INSTALL_BINDIR}` (typically `/usr/local/bin`)
- Libraries → `${CMAKE_INSTALL_LIBDIR}` (typically `/usr/local/lib`)
- Headers → `${CMAKE_INSTALL_INCLUDEDIR}` (typically `/usr/local/include`)

**Installation setup:**
- All done through `lithium_add_library()` function
- Automatic export rules generated
- CMake package configuration files created

### 11. Key CMake Variables Used Throughout System

| Variable | Purpose | Example |
|----------|---------|---------|
| `CMAKE_CXX_STANDARD` | C++ standard version | 23 or 20 |
| `CMAKE_BUILD_TYPE` | Build configuration | Debug, Release, etc |
| `CMAKE_CXX_COMPILER_ID` | Compiler type | GNU, Clang, MSVC |
| `CMAKE_CXX_COMPILER` | Compiler executable path | g++-13, clang++, cl.exe |
| `PROJECT_NAME` | Current component name | lithium_config, etc |
| `PROJECT_VERSION` | Component version | 1.0.0 |
| `PROJECT_SOURCES` | Component source files | List of .cpp files |
| `PROJECT_HEADERS` | Component header files | List of .hpp files |
| `PROJECT_LIBRARIES` | Component dependencies | List of target names |
| `CMAKE_THREAD_LIBS_INIT` | Platform threading lib | pthread on Unix, Windows on Win32 |

### 12. Common Configuration Commands

**Standard build process:**
```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
cmake --install build --prefix /usr/local
```

**Specific compiler:**
```bash
cmake -B build -DCMAKE_CXX_COMPILER=g++-13 -DCMAKE_C_COMPILER=gcc-13
```

**Custom installation prefix:**
```bash
cmake -B build -DCMAKE_INSTALL_PREFIX=/opt/lithium
```

</CodeSection>
