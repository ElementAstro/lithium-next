# Modular Focuser CMakeLists.txt
cmake_minimum_required(VERSION 3.20)

# Define the focuser module library sources
set(FOCUSER_SOURCES
    types.hpp
    property_manager.hpp
    property_manager.cpp
    movement_controller.hpp
    movement_controller.cpp
    temperature_manager.hpp
    temperature_manager.cpp
    preset_manager.hpp
    preset_manager.cpp
    statistics_manager.hpp
    statistics_manager.cpp
    modular_focuser.hpp
    modular_focuser.cpp
)

# Create the focuser module library
add_library(lithium_focuser_indi SHARED ${FOCUSER_SOURCES})

# Set target properties
set_target_properties(lithium_focuser_indi PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(lithium_focuser_indi
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/..
        ${CMAKE_SOURCE_DIR}/src
)

# Required libraries (similar to the parent CMakeLists)
set(FOCUSER_LIBS
    atom-system
    atom-io
    atom-utils
    atom-component
    atom-error
    spdlog::spdlog
)

if (NOT WIN32)
    list(APPEND FOCUSER_LIBS indiclient)
endif()

# Link against required libraries
target_link_libraries(lithium_focuser_indi
    PUBLIC
        ${FOCUSER_LIBS}
)

# Compiler-specific options
target_compile_options(lithium_focuser_indi PRIVATE
    $<$<CXX_COMPILER_ID:GNU>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:Clang>:-Wall -Wextra -Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4>
)

# Create alias for easier usage
add_library(lithium::focuser::indi ALIAS lithium_focuser_indi)
