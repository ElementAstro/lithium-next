# ============================================================================
# CMakeLists.txt for Lithium-Tools
# ============================================================================
# This project is licensed under the terms of the GPL3 license.
#
# Project Name: Lithium-Tools
# Description: Astronomy calculation and utility tools
# Author: Max Qian
# License: GPL3
# ============================================================================

lithium_component_begin(lithium_tools 1.0.0)

# ============================================================================
# Source Files
# ============================================================================

set(PROJECT_SOURCES
    # Calculation submodule
    calculation/julian.cpp

    # Solver submodule
    solver/wcs.cpp
)

# ============================================================================
# Header Files
# ============================================================================

set(PROJECT_HEADERS
    # Main aggregated header
    tools.hpp

    # Astronomy types submodule
    astronomy/constants.hpp
    astronomy/constraints.hpp
    astronomy/coordinates.hpp
    astronomy/exposure.hpp
    astronomy/meridian.hpp
    astronomy/target_config.hpp
    astronomy/types.hpp

    # Calculation submodule
    calculation/julian.hpp
    calculation/sidereal.hpp
    calculation/transform.hpp
    calculation/precession.hpp
    calculation/calculate.hpp

    # Conversion submodule
    conversion/angle.hpp
    conversion/coordinate.hpp
    conversion/format.hpp
    conversion/convert.hpp

    # Solver submodule
    solver/wcs.hpp
)

# ============================================================================
# Library Dependencies
# ============================================================================

set(PROJECT_LIBRARIES
    atom
    loguru
    yaml-cpp
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
    DESCRIPTION "Astronomy calculation and utility tools"
)

# Add public include directory for constant.hpp and other headers
target_include_directories(${PROJECT_NAME} PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include/lithium/tools>
)

# Apply standard compiler flags
lithium_apply_compiler_flags(${PROJECT_NAME})

# Set IDE folder
lithium_set_target_folder(${PROJECT_NAME} "Core/Tools")

lithium_component_end(${PROJECT_NAME})
