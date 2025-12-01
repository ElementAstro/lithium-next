# ===========================================================================
# CMakeLists.txt for Lithium-Target IO
# ===========================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Module: Target IO
# Description: Input/Output operations and file handling for target module
# ===========================================================================

set(IO_SOURCES
    csv_handler.cpp
    json_handler.cpp
)

set(IO_HEADERS
    csv_handler.hpp
    json_handler.hpp
)

# Create object library for IO module
add_library(lithium_target_io_obj OBJECT ${IO_SOURCES})

target_include_directories(lithium_target_io_obj
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/..>
)

# Apply compiler flags
lithium_apply_compiler_flags(lithium_target_io_obj)

# Set IDE folder
lithium_set_target_folder(lithium_target_io_obj "Core/Target/IO")

# Expose object library for parent
set(LITHIUM_TARGET_IO_OBJECTS $<TARGET_OBJECTS:lithium_target_io_obj> PARENT_SCOPE)
set(LITHIUM_TARGET_IO_HEADERS ${IO_HEADERS} PARENT_SCOPE)
