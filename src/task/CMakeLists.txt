# Enhanced Task System - Clean and Maintainable Build Configuration
# Follows C++ best practices for organization and maintainability

cmake_minimum_required(VERSION 3.20)
project(lithium_task VERSION 1.0.0 LANGUAGES CXX)

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

# Required dependencies
find_package(Threads REQUIRED)
find_package(spdlog QUIET)

if(NOT spdlog_FOUND)
    message(WARNING "spdlog not found, some logging features may be disabled")
endif()

set(CORE_HEADERS
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
