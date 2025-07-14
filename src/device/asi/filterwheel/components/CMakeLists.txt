# ASI Filterwheel Components CMakeLists.txt

set(ASI_FILTERWHEEL_COMPONENTS_SOURCES
    hardware_interface.cpp
    position_manager.cpp
    configuration_manager.cpp
    sequence_manager.cpp
    monitoring_system.cpp
    calibration_system.cpp
)

set(ASI_FILTERWHEEL_COMPONENTS_HEADERS
    hardware_interface.hpp
    position_manager.hpp
    configuration_manager.hpp
    sequence_manager.hpp
    monitoring_system.hpp
    calibration_system.hpp
)

# Create filterwheel components library
add_library(asi_filterwheel_components STATIC
    ${ASI_FILTERWHEEL_COMPONENTS_SOURCES}
    ${ASI_FILTERWHEEL_COMPONENTS_HEADERS}
)

# Set target properties
set_target_properties(asi_filterwheel_components PROPERTIES
    CXX_STANDARD 20
    CXX_STANDARD_REQUIRED ON
    POSITION_INDEPENDENT_CODE ON
)

# Include directories
target_include_directories(asi_filterwheel_components
    PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
        ${CMAKE_SOURCE_DIR}/libs
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../
)

# Link libraries
target_link_libraries(asi_filterwheel_components
    PRIVATE
        atom
        ${CMAKE_THREAD_LIBS_INIT}
)

# Conditional linking based on EFW SDK availability
if(LITHIUM_ASI_EFW_ENABLED)
    message(STATUS "ASI EFW support enabled for filterwheel components")
    target_compile_definitions(asi_filterwheel_components PRIVATE LITHIUM_ASI_EFW_ENABLED)

    # Link EFW SDK if available
    if(TARGET EFW::EFW)
        target_link_libraries(asi_filterwheel_components PRIVATE EFW::EFW)
        message(STATUS "Linking filterwheel components with EFW SDK")
    endif()
else()
    message(STATUS "ASI EFW support disabled - using stub implementation for filterwheel components")
endif()

# Compiler-specific options
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    target_compile_options(asi_filterwheel_components PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    target_compile_options(asi_filterwheel_components PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    target_compile_options(asi_filterwheel_components PRIVATE
        /W4
        /wd4100  # unreferenced formal parameter
        /wd4267  # conversion from 'size_t' to 'int'
    )
endif()

# Export the target
set_property(TARGET asi_filterwheel_components PROPERTY EXPORT_NAME FilterwheelComponents)

# Installation (if needed)
if(LITHIUM_INSTALL_COMPONENTS)
    install(TARGETS asi_filterwheel_components
        EXPORT LithiumTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    install(FILES ${ASI_FILTERWHEEL_COMPONENTS_HEADERS}
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/lithium/device/asi/filterwheel/components
    )
endif()

# Tests
if(LITHIUM_BUILD_TESTS)
    add_subdirectory(tests)
endif()

# Documentation
if(LITHIUM_BUILD_DOCS)
    # Add to main documentation build
endif()

message(STATUS "ASI Filterwheel Components configured successfully")
