# ============================================================================
# CMakeLists.txt for Lithium-Debug-Terminal
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Debug-Terminal
# Description: Modular terminal components for debugging
# Author: Max Qian
# License: GPL3
# ============================================================================

set(TERMINAL_SOURCES
    command_executor.cpp
    history_manager.cpp
    input_controller.cpp
    renderer.cpp
    terminal.cpp
    tui_manager.cpp
)

set(TERMINAL_HEADERS
    command_executor.hpp
    history_manager.hpp
    input_controller.hpp
    renderer.hpp
    terminal.hpp
    tui_manager.hpp
    types.hpp
)

# Create object library for terminal components
add_library(lithium_debug_terminal_obj OBJECT ${TERMINAL_SOURCES})

target_include_directories(lithium_debug_terminal_obj PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/libs/atom
    ${CMAKE_SOURCE_DIR}/src
)

# Link dependencies
target_link_libraries(lithium_debug_terminal_obj PUBLIC
    atom
    ${CMAKE_THREAD_LIBS_INIT}
)

# Check for ncurses
find_package(Curses QUIET)
if(CURSES_FOUND)
    target_include_directories(lithium_debug_terminal_obj PRIVATE ${CURSES_INCLUDE_DIRS})
    target_link_libraries(lithium_debug_terminal_obj PUBLIC ${CURSES_LIBRARIES})
    target_compile_definitions(lithium_debug_terminal_obj PRIVATE HAS_NCURSES=1)
endif()

# Check for readline
find_library(READLINE_LIBRARY readline)
if(READLINE_LIBRARY)
    target_link_libraries(lithium_debug_terminal_obj PUBLIC ${READLINE_LIBRARY})
    target_compile_definitions(lithium_debug_terminal_obj PRIVATE HAS_READLINE=1)
endif()

# Set properties
set_target_properties(lithium_debug_terminal_obj PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# IDE folder
set_target_properties(lithium_debug_terminal_obj PROPERTIES FOLDER "Core/Debug/Terminal")
