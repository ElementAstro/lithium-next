# Enhanced Task System - Clean and Maintainable Build Configuration
# Follows C++ best practices for organization and maintainability

cmake_minimum_required(VERSION 3.20)
project(lithium_task_enhanced VERSION 1.0.0 LANGUAGES CXX)

# Modern C++ standards and compiler setup
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build type and optimization settings
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Compiler-specific optimizations
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        $<$<CONFIG:Release>:-O3 -DNDEBUG -march=native>
        $<$<CONFIG:Debug>:-O0 -g3 -DDEBUG>
    )
endif()

# Feature detection and optional dependencies
include(CheckIncludeFile)
enable_language(C)  # Enable C language for header checks
check_include_file("numa.h" HAVE_NUMA_H)
if(HAVE_NUMA_H)
    add_definitions(-DHAVE_NUMA=1)
    find_library(NUMA_LIBRARY numa)
    if(NUMA_LIBRARY)
        set(NUMA_LIBRARIES ${NUMA_LIBRARY})
    else()
        set(NUMA_LIBRARIES "")
        message(WARNING "NUMA header found but library not available")
    endif()
else()
    set(NUMA_LIBRARIES "")
    message(STATUS "NUMA not available - using fallback implementations")
endif()

# Required dependencies
find_package(Threads REQUIRED)
find_package(spdlog QUIET)

if(NOT spdlog_FOUND)
    message(WARNING "spdlog not found, some logging features may be disabled")
endif()

# Check for C++20 coroutines support
include(CheckCXXSourceCompiles)
check_cxx_source_compiles("
#include <coroutine>
int main() {
    std::coroutine_handle<> h;
    return 0;
}
" HAVE_CXX20_COROUTINES)

if(NOT HAVE_CXX20_COROUTINES)
    message(WARNING "C++20 coroutines not fully supported - some features may be disabled")
endif()

# Source file organization
set(CONCURRENCY_HEADERS
    concurrency/common_types.hpp
    concurrency/lock_free_queue.hpp
    concurrency/atomic_shared_ptr.hpp
    concurrency/work_stealing_scheduler_fixed.hpp
)

set(CORE_HEADERS
    enhanced_task_system.hpp
    task.hpp
    target.hpp
    sequencer.hpp
    generator.hpp
    sequence_manager.hpp
    registration.hpp
    exception.hpp
)

# Collect implementation files
file(GLOB_RECURSE IMPL_SOURCES
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
)

# Filter out problematic files
list(FILTER IMPL_SOURCES EXCLUDE REGEX ".*test.*\\.cpp$")
list(FILTER IMPL_SOURCES EXCLUDE REGEX ".*example.*\\.cpp$")
list(FILTER IMPL_SOURCES EXCLUDE REGEX ".*benchmark.*\\.cpp$")

# Organize all source files
set(ALL_SOURCES
    ${IMPL_SOURCES}
    ${CONCURRENCY_HEADERS}
    ${CORE_HEADERS}
)

# Create the enhanced task library
add_library(${PROJECT_NAME} STATIC ${ALL_SOURCES})

# Set target properties for maintainability
set_target_properties(${PROJECT_NAME} PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION 1
    OUTPUT_NAME ${PROJECT_NAME}
    POSITION_INDEPENDENT_CODE ON
    CXX_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN TRUE
)

# Include directories - clean organization
target_include_directories(${PROJECT_NAME} 
    PUBLIC 
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/concurrency>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# Link required libraries
set(REQUIRED_LIBRARIES
    Threads::Threads
)

# Add spdlog if available
if(spdlog_FOUND)
    list(APPEND REQUIRED_LIBRARIES spdlog::spdlog)
endif()

# Add optional libraries
if(NUMA_LIBRARIES)
    list(APPEND REQUIRED_LIBRARIES ${NUMA_LIBRARIES})
endif()

target_link_libraries(${PROJECT_NAME} 
    PUBLIC ${REQUIRED_LIBRARIES}
    PRIVATE 
        atom
        lithium_config
        lithium_database
        yaml-cpp
)

# Conditional compilation based on available features
if(HAVE_NUMA_H)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_NUMA=1)
endif()

if(HAVE_CXX20_COROUTINES)
    target_compile_definitions(${PROJECT_NAME} PRIVATE HAVE_CXX20_COROUTINES=1)
endif()

# Development and testing targets
option(BUILD_TESTS "Build unit tests" OFF)
option(BUILD_BENCHMARKS "Build performance benchmarks" OFF)
option(BUILD_EXAMPLES "Build example programs" OFF)

if(BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

# Static analysis and code quality targets
find_program(CLANG_TIDY_EXE NAMES "clang-tidy")
if(CLANG_TIDY_EXE)
    set_target_properties(${PROJECT_NAME} PROPERTIES
        CXX_CLANG_TIDY "${CLANG_TIDY_EXE};-checks=-*,readability-*,performance-*,modernize-*"
    )
endif()

# Documentation generation
find_package(Doxygen QUIET)
if(DOXYGEN_FOUND)
    set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/docs)
    set(DOXYGEN_GENERATE_HTML YES)
    set(DOXYGEN_GENERATE_MAN NO)
    set(DOXYGEN_EXTRACT_ALL YES)
    set(DOXYGEN_EXTRACT_PRIVATE NO)
    set(DOXYGEN_EXTRACT_STATIC NO)
    set(DOXYGEN_CALL_GRAPH YES)
    set(DOXYGEN_CALLER_GRAPH YES)
    
    doxygen_add_docs(docs
        ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Generating API documentation"
    )
endif()

# Installation configuration
include(GNUInstallDirs)

install(TARGETS ${PROJECT_NAME}
    EXPORT ${PROJECT_NAME}Targets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

# Install headers with proper organization
install(FILES ${CORE_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lithium/task
)

install(FILES ${CONCURRENCY_HEADERS}
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lithium/task/concurrency
)

# Generate and install cmake configuration files
include(CMakePackageConfigHelpers)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY AnyNewerVersion
)

# Skip missing cmake configuration file
# configure_package_config_file(
#     "${CMAKE_CURRENT_SOURCE_DIR}/cmake/${PROJECT_NAME}Config.cmake.in"
#     "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
#     INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
# )

# Skip installation of config files that don't exist
# install(FILES
#     "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}Config.cmake"
#     "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}ConfigVersion.cmake"
#     DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
# )

install(EXPORT ${PROJECT_NAME}Targets
    FILE ${PROJECT_NAME}Targets.cmake
    NAMESPACE lithium::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/${PROJECT_NAME}
)

# Custom targets for common development tasks
add_custom_target(format
    COMMAND find ${CMAKE_CURRENT_SOURCE_DIR} -name "*.hpp" -o -name "*.cpp" | 
            xargs clang-format -i -style=file
    COMMENT "Formatting source code"
)

add_custom_target(check-format
    COMMAND find ${CMAKE_CURRENT_SOURCE_DIR} -name "*.hpp" -o -name "*.cpp" | 
            xargs clang-format -style=file --dry-run --Werror
    COMMENT "Checking code formatting"
)

# Print configuration summary
message(STATUS "Enhanced Task System Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build Type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "  Compiler: ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  NUMA Support: ${HAVE_NUMA_H}")
message(STATUS "  C++20 Coroutines: ${HAVE_CXX20_COROUTINES}")
message(STATUS "  Tests: ${BUILD_TESTS}")
message(STATUS "  Benchmarks: ${BUILD_BENCHMARKS}")
message(STATUS "  Examples: ${BUILD_EXAMPLES}")
message(STATUS "  Documentation: ${DOXYGEN_FOUND}")

# Packaging support (skip missing files)
set(CPACK_PACKAGE_NAME ${PROJECT_NAME})
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Enhanced Task System with Advanced Concurrency")
set(CPACK_PACKAGE_VENDOR "Lithium Project")
# set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
# set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# include(CPack)